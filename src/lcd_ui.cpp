#include "lcd_ui.h"
#include "generated_assets.h"
#include "temperature_value.h"
#include <SPIFFS.h>
#include <TJpg_Decoder.h>
#include <algorithm>
#include <cstring>

namespace
{
constexpr int FIELD_WIDTH = 104;
constexpr int FIELD_HEIGHT = 38;
constexpr int CENTER_X[6] = {147, 147, 147, 331, 331, 331};
// Buffer order: left top/middle/bottom, right middle/top/bottom.
constexpr int CENTER_Y[6] = {95, 171, 248, 171, 95, 248};
// Cache only the changing regions (~49 KB), never a full 480x320 framebuffer.
uint16_t fieldPixels[6][FIELD_WIDTH * FIELD_HEIGHT];
uint16_t wifiPixels[32 * 28];
TFT_eSPI *screen = nullptr;
bool filesystemReady = false;
bool backgroundReady = false;
bool dirty = true;
bool lastWifi = false;
float lastValues[6] = {};
bool lastValid[6] = {};

void capture(int x, int y, int width, int height, const uint16_t *pixels,
             int regionX, int regionY, int regionW, int regionH, uint16_t *target)
{
  int left = std::max(x, regionX);
  int top = std::max(y, regionY);
  int right = std::min(x + width, regionX + regionW);
  int bottom = std::min(y + height, regionY + regionH);
  for (int row = top; row < bottom; ++row)
  {
    if (right > left)
      memcpy(target + (row - regionY) * regionW + left - regionX,
             pixels + (row - y) * width + left - x, (right - left) * sizeof(uint16_t));
  }
}

bool outputBlock(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *pixels)
{
  if (y >= screen->height())
    return false;
  screen->pushImage(x, y, w, h, pixels);
  for (int i = 0; i < 6; ++i)
    capture(x, y, w, h, pixels, CENTER_X[i] - FIELD_WIDTH / 2,
            CENTER_Y[i] - FIELD_HEIGHT / 2, FIELD_WIDTH, FIELD_HEIGHT, fieldPixels[i]);
  capture(x, y, w, h, pixels, 440, 10, 32, 28, wifiPixels);
  return true;
}

bool validationBlock(int16_t, int16_t, uint16_t, uint16_t, uint16_t *)
{
  return true;
}

void drawWifi(bool connected)
{
  screen->pushImage(440, 10, 32, 28, wifiPixels);
  uint16_t color = connected ? screen->color565(95, 255, 211) : screen->color565(255, 119, 126);
  for (int radius : {17, 12, 7})
    screen->drawArc(456, 33, radius, radius - 1, 135, 225, color, color, false);
  screen->fillCircle(456, 34, 2, color);
  if (!connected)
  {
    screen->drawLine(444, 12, 468, 36, color);
    screen->drawLine(445, 12, 469, 36, color);
  }
}
}

void lcdInitialize(TFT_eSPI &display, bool filesystemAvailable)
{
  screen = &display;
  filesystemReady = filesystemAvailable;
  TJpgDec.setJpgScale(1);
  TJpgDec.setCallback(outputBlock);
}

bool lcdValidateImage(const char *path)
{
  uint16_t width = 0, height = 0;
  if (!filesystemReady || TJpgDec.getFsJpgSize(&width, &height, path) != JDR_OK ||
      width != 480 || height != 320)
    return false;
  // Decode the whole file before accepting it, without touching the LCD/cache.
  TJpgDec.setCallback(validationBlock);
  JRESULT result = TJpgDec.drawFsJpg(0, 0, path);
  TJpgDec.setCallback(outputBlock);
  return result == JDR_OK;
}

void lcdDrawBackground()
{
  JRESULT result = JDR_FMT1;
  if (filesystemReady && SPIFFS.exists(LCD_CUSTOM_PATH))
    result = TJpgDec.drawFsJpg(0, 0, LCD_CUSTOM_PATH);
  if (result != JDR_OK)
    result = TJpgDec.drawJpg(0, 0, LCD_BACKGROUND, LCD_BACKGROUND_SIZE);
  backgroundReady = result == JDR_OK;
  dirty = true;
  if (!backgroundReady)
    Serial.printf("LCD background decode failed: %d\n", result);
}

void lcdRender(const float values[6], bool sampled, bool wifiConnected)
{
  if (!backgroundReady)
    return;
  screen->setTextSize(1);
  screen->setTextDatum(TL_DATUM);
  screen->setTextPadding(0);
  screen->setTextColor(screen->color565(23, 60, 97)); // Transparent over cached artwork.
  for (int i = 0; i < 6; ++i)
  {
    bool valid = sampled && validTemperature(values[i]);
    if (!dirty && valid == lastValid[i] && (!valid || fabsf(values[i] - lastValues[i]) < 0.05f))
      continue;
    screen->pushImage(CENTER_X[i] - FIELD_WIDTH / 2, CENTER_Y[i] - FIELD_HEIGHT / 2,
                      FIELD_WIDTH, FIELD_HEIGHT, fieldPixels[i]);
    char text[12];
    if (valid)
      snprintf(text, sizeof(text), "%.1f", values[i]);
    else
      strcpy(text, "--.-");
    int width = screen->textWidth(text, 4);
    int x = CENTER_X[i] - (width + 15) / 2;
    screen->drawString(text, x, CENTER_Y[i] - 13, 4);
    screen->drawCircle(x + width + 3, CENTER_Y[i] - 6, 2, screen->color565(23, 60, 97));
    screen->drawString("C", x + width + 7, CENTER_Y[i] - 7, 2);
    lastValues[i] = values[i];
    lastValid[i] = valid;
  }
  if (dirty || lastWifi != wifiConnected)
    drawWifi(wifiConnected);
  lastWifi = wifiConnected;
  dirty = false;
}
