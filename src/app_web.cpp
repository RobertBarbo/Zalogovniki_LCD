#include "app_web.h"
#include "firmware_version.h"
#include "generated_assets.h"
#include "lcd_ui.h"
#include "temperature_value.h"
#include "device_restart.h"
#include <SPIFFS.h>
#include <WiFi.h>
#include <esp_ota_ops.h>
#include <esp_timer.h>
#include <esp_random.h>
#include <algorithm>

namespace
{
constexpr size_t MAX_IMAGE_BYTES = 128 * 1024;
constexpr const char *TEMP_IMAGE = "/lcd-upload.tmp";
constexpr const char *PREVIOUS_IMAGE = "/lcd-previous.jpg";
WebServer *http = nullptr;
StatusProvider getStatus = nullptr;
SensorDiagnosticsProvider getSensors = nullptr;
ImageChanged redrawImage = nullptr;
bool fsReady = false;
uint32_t bootId = 0;
struct UploadState
{
  File file;
  bool started = false;
  bool complete = false;
  int error = 0;
  size_t bytes = 0;
  size_t limit = 0;
  String message;
} upload;

String jsonString(const String &input)
{
  String output = "\"";
  output.reserve(input.length() + 16);
  for (size_t i = 0; i < input.length(); ++i)
  {
    unsigned char c = input[i];
    if (c == '"' || c == '\\')
    {
      output += '\\';
      output += static_cast<char>(c);
    }
    else if (c < 0x20)
    {
      char escaped[7];
      snprintf(escaped, sizeof(escaped), "\\u%04x", c);
      output += escaped;
    }
    else
      output += static_cast<char>(c);
  }
  return output + '"';
}

void noCache()
{
  http->sendHeader("Cache-Control", "no-store");
  http->sendHeader("X-Content-Type-Options", "nosniff");
}

void sendJson(int code, const String &body)
{
  noCache();
  http->send(code, "application/json; charset=utf-8", body);
}

void serveStatus()
{
  DeviceStatus state = getStatus();
  bool connected = WiFi.status() == WL_CONNECTED;
  String body;
  body.reserve(900);
  body = "{\"firmware\":\"" FIRMWARE_VERSION "\",\"sampled\":";
  body += state.sampled ? "true" : "false";
  body += ",\"boot_id\":" + jsonString(String(bootId, HEX));
  body += ",\"sample_age_ms\":";
  body += state.sampled ? String(static_cast<uint32_t>(millis() - state.sampledAt)) : "null";
  body += ",\"temperatures\":[";
  for (int i = 0; i < 6; ++i)
  {
    if (i)
      body += ',';
    body += state.sampled && validTemperature(state.temperatures[i]) ? String(state.temperatures[i], 1) : "null";
  }
  body += "],\"wifi\":{\"connected\":";
  body += connected ? "true" : "false";
  body += ",\"ssid\":" + jsonString(WiFi.SSID());
  body += ",\"ip\":" + jsonString(connected ? WiFi.localIP().toString() : String(""));
  body += ",\"rssi\":";
  body += connected ? String(WiFi.RSSI()) : "null";
  body += "},\"mqtt_connected\":";
  body += state.mqttConnected ? "true" : "false";
  char uptime[24];
  snprintf(uptime, sizeof(uptime), "%llu", static_cast<unsigned long long>(esp_timer_get_time() / 1000000));
  body += ",\"uptime_s\":" + String(uptime);
  body += ",\"free_heap\":" + String(ESP.getFreeHeap());
  body += ",\"lcd_custom\":";
  body += fsReady && SPIFFS.exists(LCD_CUSTOM_PATH) ? "true" : "false";
  body += ",\"filesystem_ready\":";
  body += fsReady ? "true" : "false";
  const esp_partition_t *running = esp_ota_get_running_partition();
  const esp_partition_t *next = esp_ota_get_next_update_partition(nullptr);
  if (next && running && next->address == running->address)
    next = nullptr;
  body += ",\"ota\":{\"running_partition\":" + jsonString(running ? String(running->label) : String(""));
  body += ",\"next_partition\":" + jsonString(next ? String(next->label) : String(""));
  body += ",\"slot_bytes\":" + String(next ? next->size : 0);
  body += "}}";
  sendJson(200, body);
}

void serveSensors()
{
  DeviceStatus state = getStatus();
  SensorDiagnostics diagnostics = getSensors();
  String body;
  body.reserve(1500);
  body = "{\"firmware\":\"" FIRMWARE_VERSION "\",\"devices_at_boot\":";
  body += String(diagnostics.devicesAtBoot);
  body += ",\"sampled\":";
  body += state.sampled ? "true" : "false";
  body += ",\"sample_age_ms\":";
  body += state.sampled ? String(static_cast<uint32_t>(millis() - state.sampledAt)) : "null";
  body += ",\"address_mode\":\"rom\",\"index_origin\":\"boot\",\"sensors\":[";
  for (int buffer = 0; buffer < 6; ++buffer)
  {
    if (buffer)
      body += ',';
    body += "{\"buffer\":" + String(buffer + 1);
    body += ",\"index\":";
    body += diagnostics.indexFoundAtBoot[buffer] ? String(diagnostics.index[buffer]) : "null";
    body += ",\"rom\":";
    bool found = diagnostics.addressConfigured[buffer];
    if (found)
    {
      char rom[24];
      const uint8_t *address = diagnostics.address[buffer];
      snprintf(rom, sizeof(rom), "%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X",
               address[0], address[1], address[2], address[3],
               address[4], address[5], address[6], address[7]);
      body += jsonString(rom);
    }
    else
      body += "null";
    body += ",\"temperature\":";
    body += state.sampled && found && validTemperature(state.temperatures[buffer]) ? String(state.temperatures[buffer], 1) : "null";
    body += '}';
  }
  body += "]}";
  sendJson(200, body);
}

bool mutationAllowed()
{
  // A custom header prevents silent cross-origin form uploads; no CORS opt-in.
  if (http->header("X-Requested-With") != "ZalogovnikiLCD")
    return false;
  String origin = http->header("Origin");
  return origin.isEmpty() || origin == "http://" + http->hostHeader();
}

void restartDevice()
{
  if (!mutationAllowed())
    sendJson(403, "{\"ok\":false,\"message\":\"Ponovni zagon odpri prek strani naprave.\"}");
  else if (http->arg("plain") != "RESTART")
    sendJson(400, "{\"ok\":false,\"message\":\"Manjka potrditev ponovnega zagona.\"}");
  else if (!requestDeviceRestart())
    sendJson(409, "{\"ok\":false,\"message\":\"Poteka prenos slike ali firmware-a. Poskusi po koncu prenosa.\"}");
  else
    sendJson(202, "{\"ok\":true,\"boot_id\":" + jsonString(String(bootId, HEX)) + "}");
}

void failUpload(int code, const char *message)
{
  if (upload.error)
    return;
  upload.error = code;
  upload.message = message;
  if (upload.file)
    upload.file.close();
}

void receiveUpload()
{
  HTTPUpload &part = http->upload();
  if (part.status == UPLOAD_FILE_START)
  {
    if (upload.started)
    {
      failUpload(400, "Izberi samo eno sliko.");
      return;
    }
    upload = UploadState{};
    upload.started = true;
    if (!mutationAllowed())
      failUpload(403, "Nalaganje odpri prek strani naprave.");
    else if (deviceRestartPending())
      failUpload(409, "Naprava se bo ponovno zagnala. Pocakaj na povezavo.");
    else if (!fsReady)
      failUpload(503, "SPIFFS ni dostopen. Privzeto ozadje ostaja na voljo.");
    else
    {
      size_t free = SPIFFS.totalBytes() - SPIFFS.usedBytes();
      upload.limit = free > 4096 ? std::min(MAX_IMAGE_BYTES, free - 4096) : 0;
      if (!upload.limit)
        failUpload(507, "V filesystemu ni dovolj prostora.");
      else
      {
        upload.file = SPIFFS.open(TEMP_IMAGE, FILE_WRITE);
        if (!upload.file)
          failUpload(500, "Zacasne datoteke ni mogoce odpreti.");
        else
          setDeviceRestartBlocked(RestartBlock::ImageUpload, true);
      }
    }
  }
  else if (part.status == UPLOAD_FILE_WRITE && !upload.error)
  {
    if (!upload.started || !upload.file)
      failUpload(400, "Neveljaven prenos slike.");
    else if (part.currentSize > upload.limit - upload.bytes)
      failUpload(413, "Slika je prevelika ali pa zmanjkuje prostora. Najvec 128 KB.");
    else if (upload.file.write(part.buf, part.currentSize) != part.currentSize)
      failUpload(507, "Zapisa slike ni bilo mogoce dokoncati.");
    else
      upload.bytes += part.currentSize;
  }
  else if (part.status == UPLOAD_FILE_END && !upload.error)
  {
    if (upload.file)
      upload.file.close();
    if (!upload.bytes || !lcdValidateImage(TEMP_IMAGE))
      failUpload(422, "Potrebna je veljavna baseline JPG slika, tocno 480 x 320 px.");
    else
      upload.complete = true;
  }
  else if (part.status == UPLOAD_FILE_ABORTED)
  {
    failUpload(400, "Prenos slike je bil prekinjen.");
    if (fsReady)
      SPIFFS.remove(TEMP_IMAGE);
    upload.started = false;
    setDeviceRestartBlocked(RestartBlock::ImageUpload, false);
  }
}

void finishUpload()
{
  if (!mutationAllowed())
    failUpload(403, "Nalaganje odpri prek strani naprave.");
  if (!upload.error && (!upload.started || !upload.complete))
    failUpload(400, "Slika ni bila prejeta v celoti.");
  if (!upload.error)
  {
    bool hadCustom = SPIFFS.exists(LCD_CUSTOM_PATH);
    bool backupReady = true;
    if (hadCustom)
    {
      if (SPIFFS.exists(PREVIOUS_IMAGE))
        backupReady = SPIFFS.remove(PREVIOUS_IMAGE);
      if (backupReady)
        backupReady = SPIFFS.rename(LCD_CUSTOM_PATH, PREVIOUS_IMAGE);
    }
    if (!backupReady)
      failUpload(500, "Prejsnje slike ni mogoce varno ohraniti.");
    else if (!SPIFFS.rename(TEMP_IMAGE, LCD_CUSTOM_PATH))
    {
      if (hadCustom && !SPIFFS.rename(PREVIOUS_IMAGE, LCD_CUSTOM_PATH))
        Serial.println("LCD image rollback failed; embedded v5 remains available.");
      failUpload(500, "Slike ni mogoce aktivirati.");
    }
    else
      redrawImage();
  }
  if (upload.error)
  {
    if (fsReady)
      SPIFFS.remove(TEMP_IMAGE);
    sendJson(upload.error, "{\"ok\":false,\"message\":" + jsonString(upload.message) + "}");
  }
  else
    sendJson(200, "{\"ok\":true,\"message\":\"Slika je nalozena in prikazana na LCD-ju.\"}");
  upload = UploadState{};
  setDeviceRestartBlocked(RestartBlock::ImageUpload, false);
}

void sendAsset(const char *type, const uint8_t *data, size_t size)
{
  http->sendHeader("Content-Encoding", "gzip");
  http->sendHeader("Cache-Control", "no-cache");
  http->sendHeader("X-Content-Type-Options", "nosniff");
  http->sendHeader("Content-Security-Policy", "default-src 'self'; script-src 'self'; style-src 'self' 'unsafe-inline'; img-src 'self' data: blob:; object-src 'none'; base-uri 'none'; frame-ancestors 'none'");
  http->send_P(200, type, reinterpret_cast<const char *>(data), size);
}
}

void setupAppWeb(WebServer &server, StatusProvider status, ImageChanged imageChanged,
                 bool filesystemReady, SensorDiagnosticsProvider sensorDiagnostics)
{
  http = &server;
  getStatus = status;
  getSensors = sensorDiagnostics;
  redrawImage = imageChanged;
  fsReady = filesystemReady;
  bootId = esp_random();
  // Recover the prior custom image if power was lost between the two renames.
  if (fsReady && !SPIFFS.exists(LCD_CUSTOM_PATH) && SPIFFS.exists(PREVIOUS_IMAGE) &&
      lcdValidateImage(PREVIOUS_IMAGE))
  {
    if (!SPIFFS.rename(PREVIOUS_IMAGE, LCD_CUSTOM_PATH))
      Serial.println("Previous LCD image recovery failed; using embedded v5.");
  }
  const char *headers[] = {"Origin", "X-Requested-With"};
  server.collectHeaders(headers, 2);
  server.on("/", HTTP_GET, []() { sendAsset("text/html; charset=utf-8", WEB_INDEX_GZIP, WEB_INDEX_GZIP_SIZE); });
  server.on("/app.js", HTTP_GET, []() { sendAsset("application/javascript; charset=utf-8", WEB_SCRIPT_GZIP, WEB_SCRIPT_GZIP_SIZE); });
  server.on("/senzorji", HTTP_GET, []() { sendAsset("text/html; charset=utf-8", WEB_SENSORS_GZIP, WEB_SENSORS_GZIP_SIZE); });
  server.on("/sensors.js", HTTP_GET, []() { sendAsset("application/javascript; charset=utf-8", WEB_SENSORS_SCRIPT_GZIP, WEB_SENSORS_SCRIPT_GZIP_SIZE); });
  server.on("/api/status", HTTP_GET, serveStatus);
  server.on("/api/sensors", HTTP_GET, serveSensors);
  server.on("/api/restart", HTTP_POST, restartDevice);
  server.on("/lcd-background.jpg", HTTP_GET, []()
            {
    noCache();
    http->send_P(200, "image/jpeg", reinterpret_cast<const char *>(LCD_BACKGROUND), LCD_BACKGROUND_SIZE); });
  server.on("/api/lcd-image", HTTP_GET, []()
            {
    noCache();
    if (fsReady && SPIFFS.exists(LCD_CUSTOM_PATH))
    {
      File file = SPIFFS.open(LCD_CUSTOM_PATH, FILE_READ);
      if (file) { http->streamFile(file, "image/jpeg"); return; }
    }
    http->send_P(200, "image/jpeg", reinterpret_cast<const char *>(LCD_BACKGROUND), LCD_BACKGROUND_SIZE); });
  server.on("/upload", HTTP_POST, finishUpload, receiveUpload);
  server.on("/files", HTTP_GET, []()
            {
    if (!fsReady) { http->send(503, "text/plain", "SPIFFS unavailable"); return; }
    String body = "SPIFFS files:\n\n";
    File root = SPIFFS.open("/");
    File file = root.openNextFile();
    while (file) { body += String(file.name()) + " (" + String(file.size()) + " B)\n"; file = root.openNextFile(); }
    noCache();
    http->send(200, "text/plain; charset=utf-8", body); });
  server.onNotFound([]() { sendJson(404, "{\"ok\":false,\"message\":\"Ni najdeno.\"}"); });
  server.begin();
}
