#pragma once
#include <TFT_eSPI.h>

constexpr const char *LCD_CUSTOM_PATH = "/lcd-custom.jpg";
void lcdInitialize(TFT_eSPI &display, bool filesystemAvailable);
void lcdDrawBackground();
void lcdRender(const float values[6], bool sampled, bool wifiConnected);
bool lcdValidateImage(const char *path);
