// ============ MQTT BUFFER - MORA BITI NA VRHU ============
#define MQTT_MAX_PACKET_SIZE 512

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <TFT_eSPI.h>
#include <TJpg_Decoder.h>
#include "SPIFFS.h"
#include <ArduinoOTA.h>
#include "firmware_version.h"
#include "app_web.h"
#include "lcd_ui.h"
#include "sensor_mapping.h"
#include "device_restart.h"
#include "secrets.h"

#include <WebServer.h> // ali ESPAsyncWebServer, če ga želiš
WebServer server(80);

// ============ WIFI & MQTT ============

const uint16_t MQTT_PORT = 1883;
const char *MQTT_CLIENTID = "zalogovniki-lcd";

// MQTT topici - temperature
const char *TOPIC_BUF1 = "zalogovniki/buffer1";
const char *TOPIC_BUF2 = "zalogovniki/buffer2";
const char *TOPIC_BUF3 = "zalogovniki/buffer3";
const char *TOPIC_BUF4 = "zalogovniki/buffer4";
const char *TOPIC_BUF5 = "zalogovniki/buffer5";
const char *TOPIC_BUF6 = "zalogovniki/buffer6";

// MQTT topici - sistem info
const char *TOPIC_IP = "zalogovniki/ip";
const char *TOPIC_SIGNAL = "zalogovniki/wifi_signal";
const char *TOPIC_SSID = "zalogovniki/wifi_ssid";
const char *TOPIC_UPTIME = "zalogovniki/uptime";
const char *TOPIC_RESTART = "zalogovniki/restart/set";
const char *TOPIC_AVAILABILITY = "zalogovniki/status";

// ============ Dallas Senzorji ============
#define ONE_WIRE_BUS 16
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

float t1, t2, t3, t4, t5, t6;
bool temperaturesSampled = false;
uint32_t sampledAt = 0;
bool filesystemAvailable = false;
bool discoveryPending = true;
uint32_t lcdRestoreAt = 0;
SensorDiagnostics sensorDiagnostics;

// ============ TFT Display ============
TFT_eSPI tft = TFT_eSPI();

// ============ WiFi & MQTT klienti ============
WiFiClient espClient;
PubSubClient mqtt(espClient);

// ============ FORWARD DEKLARACIJE ============
void connectWiFi();
void connectMQTT();
void drawBackground();
void displayTemperatures();
void updateProgressBar(int percent, String status);
void publishDiscovery();
void onMqttMessage(char *topic, byte *payload, unsigned int length);

// ============ FUNKCIJE ============

// Shared snapshot for the web API; sensor order stays identical to MQTT/LCD.
DeviceStatus statusSnapshot()
{
  return {{t1, t2, t3, t4, t5, t6}, temperaturesSampled, sampledAt, mqtt.connected()};
}

SensorDiagnostics sensorSnapshot()
{
  return sensorDiagnostics;
}

void initializeSensorDiagnostics()
{
  sensorDiagnostics.devicesAtBoot = sensors.getDeviceCount();
  for (uint8_t buffer = 0; buffer < 6; ++buffer)
  {
    memcpy(sensorDiagnostics.address[buffer], BUFFER_ROM[buffer], 8);
    sensorDiagnostics.addressConfigured[buffer] = sensors.validAddress(BUFFER_ROM[buffer]);
  }
  // Indices are diagnostic information from startup, never a source of assignment.
  for (uint8_t index = 0; index < sensorDiagnostics.devicesAtBoot; ++index)
  {
    DeviceAddress discovered;
    if (!sensors.getAddress(discovered, index))
      continue;
    for (uint8_t buffer = 0; buffer < 6; ++buffer)
    {
      if (memcmp(discovered, BUFFER_ROM[buffer], 8) == 0)
      {
        sensorDiagnostics.index[buffer] = index;
        sensorDiagnostics.indexFoundAtBoot[buffer] = true;
      }
    }
  }
}

float readTemperatureByRom(uint8_t buffer)
{
  if (!sensorDiagnostics.addressConfigured[buffer])
    return DEVICE_DISCONNECTED_C;
  return sensors.getTempC(BUFFER_ROM[buffer]);
}

void redrawLcd()
{
  lcdDrawBackground();
  displayTemperatures();
}

// Progress bar funkcija
void updateProgressBar(int percent, String status)
{
  int barWidth = 440;
  int barHeight = 40;
  int barX = (480 - barWidth) / 2;
  int barY = 250;

  tft.fillRect(0, 210, 480, 30, TFT_BLUE);

  tft.setTextColor(TFT_WHITE, TFT_BLUE);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(status, 240, 225, 2);

  tft.drawRect(barX, barY, barWidth, barHeight, TFT_WHITE);

  int fillWidth = (barWidth - 4) * percent / 100;
  tft.fillRect(barX + 2, barY + 2, fillWidth, barHeight - 4, TFT_GREEN);

  tft.fillRect(0, barY + barHeight + 5, 480, 30, TFT_BLUE);
  tft.setTextColor(TFT_WHITE, TFT_BLUE);
  tft.setTextDatum(MC_DATUM);
  char buf[10];
  sprintf(buf, "%d%%", percent);
  tft.drawString(buf, 240, barY + barHeight + 15, 4);
}

// Splash screen
void showSplashScreen()
{
  tft.fillScreen(TFT_BLUE);
  tft.setTextColor(TFT_WHITE, TFT_BLUE);
  tft.setTextDatum(MC_DATUM);

  tft.drawString("Centrometal", 240, 70, 4);
  tft.drawString("Zalogovniki", 240, 110, 4);

  tft.drawString("Verzija " FIRMWARE_VERSION, 240, 150, 2);
  tft.drawString("Avtor: Barbo", 240, 175, 2);

  updateProgressBar(0, "Initializing...");
}

// WiFi povezava
void connectWiFi()
{
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Povezujem WiFi");

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40)
  {
    delay(250);
    Serial.print(".");
    attempts++;

    int progress = 40 + (attempts * 20 / 40);
    updateProgressBar(progress, "Connecting to WiFi...");
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("\nWiFi OK! IP: " + WiFi.localIP().toString());

    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(TFT_WHITE, TFT_BLUE);
    tft.drawString("SSID: " + String(WIFI_SSID), 10, 10, 2);

    tft.setTextDatum(TC_DATUM);
    tft.drawString("IP: " + WiFi.localIP().toString(), 240, 10, 2);

    int rssi = WiFi.RSSI();
    tft.setTextDatum(TR_DATUM);
    char signalStr[20];
    sprintf(signalStr, "Signal: %d dBm", rssi);
    tft.drawString(signalStr, 470, 10, 2);

    tft.setTextDatum(TL_DATUM);
  }
  else
  {
    Serial.println("\nWiFi FAILED!");
  }
}

// MQTT povezava - S POVEČANIM BUFFERJEM
void onMqttMessage(char *topic, byte *payload, unsigned int length)
{
  if (strcmp(topic, TOPIC_RESTART) == 0 && length == 7 && memcmp(payload, "RESTART", 7) == 0)
  {
    if (!requestDeviceRestart())
      Serial.println("MQTT restart ignored: image or OTA update in progress.");
  }
}

void connectMQTT()
{
  static bool configured = false;
  static bool attempted = false;
  static uint32_t lastAttempt = 0;
  if (!configured)
  {
    mqtt.setServer(MQTT_SERVER, MQTT_PORT);
    mqtt.setBufferSize(512);
    mqtt.setCallback(onMqttMessage);
    mqtt.setSocketTimeout(2);
    espClient.setConnectionTimeout(1500);
    configured = true;
  }
  if (WiFi.status() != WL_CONNECTED || mqtt.connected())
    return;
  uint32_t now = millis();
  if (attempted && static_cast<uint32_t>(now - lastAttempt) < 5000)
    return;
  attempted = true;
  lastAttempt = now;
  Serial.print("Povezujem MQTT...");
  if (mqtt.connect(MQTT_CLIENTID, MQTT_USER, MQTT_PASS, TOPIC_AVAILABILITY, 0, true, "offline"))
  {
    // Clear stale retained commands BEFORE subscribing on this fresh clean session.
    // PubSubClient's callback does not expose the MQTT retained flag.
    if (!mqtt.publish(TOPIC_RESTART, "", true) || !mqtt.subscribe(TOPIC_RESTART))
    {
      Serial.println("MQTT restart subscription failed; reconnecting later.");
      mqtt.disconnect();
      return;
    }
    mqtt.publish(TOPIC_AVAILABILITY, "online", true);
    discoveryPending = true;
    Serial.println("OK");
  }
  else
    Serial.printf("napaka rc=%d; naslednji poskus cez 5 s\n", mqtt.state());
}

// MQTT Discovery - SKRAJŠAN JSON
void publishDiscovery()
{
  if (!mqtt.connected())
  {
    connectMQTT();
  }

  if (!mqtt.connected())
    return;

  Serial.println("Publishing MQTT Discovery...");

  // Skrajšan device JSON
  const char *dev = ",\"dev\":{\"ids\":[\"zalogovniki_lcd\"],\"name\":\"Centrometal Zalogovniki\",\"mdl\":\"ESP32 LCD\",\"mf\":\"Barbo\",\"sw\":\"" FIRMWARE_VERSION "\"}}";

  // Buffer 1
  String config = "{\"name\":\"Buffer 1\",\"stat_t\":\"zalogovniki/buffer1\",\"unit_of_meas\":\"°C\",\"dev_cla\":\"temperature\",\"uniq_id\":\"zalogovniki_buf1\"" + String(dev);
  mqtt.publish("homeassistant/sensor/zalogovniki_buf1/config", config.c_str(), true);
  Serial.println("Published: Buffer 1");
  delay(100);

  // Buffer 2
  config = "{\"name\":\"Buffer 2\",\"stat_t\":\"zalogovniki/buffer2\",\"unit_of_meas\":\"°C\",\"dev_cla\":\"temperature\",\"uniq_id\":\"zalogovniki_buf2\"" + String(dev);
  mqtt.publish("homeassistant/sensor/zalogovniki_buf2/config", config.c_str(), true);
  Serial.println("Published: Buffer 2");
  delay(100);

  // Buffer 3
  config = "{\"name\":\"Buffer 3\",\"stat_t\":\"zalogovniki/buffer3\",\"unit_of_meas\":\"°C\",\"dev_cla\":\"temperature\",\"uniq_id\":\"zalogovniki_buf3\"" + String(dev);
  mqtt.publish("homeassistant/sensor/zalogovniki_buf3/config", config.c_str(), true);
  Serial.println("Published: Buffer 3");
  delay(100);

  // Buffer 4
  config = "{\"name\":\"Buffer 4\",\"stat_t\":\"zalogovniki/buffer4\",\"unit_of_meas\":\"°C\",\"dev_cla\":\"temperature\",\"uniq_id\":\"zalogovniki_buf4\"" + String(dev);
  mqtt.publish("homeassistant/sensor/zalogovniki_buf4/config", config.c_str(), true);
  Serial.println("Published: Buffer 4");
  delay(100);

  // Buffer 5
  config = "{\"name\":\"Buffer 5\",\"stat_t\":\"zalogovniki/buffer5\",\"unit_of_meas\":\"°C\",\"dev_cla\":\"temperature\",\"uniq_id\":\"zalogovniki_buf5\"" + String(dev);
  mqtt.publish("homeassistant/sensor/zalogovniki_buf5/config", config.c_str(), true);
  Serial.println("Published: Buffer 5");
  delay(100);

  // Buffer 6
  config = "{\"name\":\"Buffer 6\",\"stat_t\":\"zalogovniki/buffer6\",\"unit_of_meas\":\"°C\",\"dev_cla\":\"temperature\",\"uniq_id\":\"zalogovniki_buf6\"" + String(dev);
  mqtt.publish("homeassistant/sensor/zalogovniki_buf6/config", config.c_str(), true);
  Serial.println("Published: Buffer 6");
  delay(100);

  // IP Address
  config = "{\"name\":\"IP Address\",\"stat_t\":\"zalogovniki/ip\",\"icon\":\"mdi:ip-network\",\"uniq_id\":\"zalogovniki_ip\"" + String(dev);
  mqtt.publish("homeassistant/sensor/zalogovniki_ip/config", config.c_str(), true);
  Serial.println("Published: IP");
  delay(100);

  // WiFi Signal
  config = "{\"name\":\"WiFi Signal\",\"stat_t\":\"zalogovniki/wifi_signal\",\"unit_of_meas\":\"dBm\",\"dev_cla\":\"signal_strength\",\"icon\":\"mdi:wifi\",\"uniq_id\":\"zalogovniki_signal\"" + String(dev);
  mqtt.publish("homeassistant/sensor/zalogovniki_signal/config", config.c_str(), true);
  Serial.println("Published: Signal");
  delay(100);

  // SSID
  config = "{\"name\":\"WiFi SSID\",\"stat_t\":\"zalogovniki/wifi_ssid\",\"icon\":\"mdi:wifi-cog\",\"uniq_id\":\"zalogovniki_ssid\"" + String(dev);
  mqtt.publish("homeassistant/sensor/zalogovniki_ssid/config", config.c_str(), true);
  Serial.println("Published: SSID");
  delay(100);

  // Uptime
  config = "{\"name\":\"Uptime\",\"stat_t\":\"zalogovniki/uptime\",\"icon\":\"mdi:clock-outline\",\"uniq_id\":\"zalogovniki_uptime\"" + String(dev);
  mqtt.publish("homeassistant/sensor/zalogovniki_uptime/config", config.c_str(), true);
  Serial.println("Published: Uptime");
  delay(100);

  // Restart button belongs to the existing device, with a non-retained command.
  config = "{\"name\":\"Ponovni zagon\",\"cmd_t\":\"zalogovniki/restart/set\",\"pl_prs\":\"RESTART\",\"dev_cla\":\"restart\",\"ent_cat\":\"config\",\"uniq_id\":\"zalogovniki_restart\",\"avty_t\":\"zalogovniki/status\",\"qos\":0,\"retain\":false" + String(dev);
  if (!mqtt.publish("homeassistant/button/zalogovniki_restart/config", config.c_str(), true))
  {
    Serial.println("Restart Discovery failed; retrying after reconnect.");
    mqtt.disconnect();
    return;
  }

  Serial.println("MQTT Discovery published!");
  discoveryPending = false;
}

// Uptime
String getUptimeString()
{
  unsigned long seconds = millis() / 1000;
  unsigned long days = seconds / 86400;
  seconds = seconds % 86400;
  unsigned long hours = seconds / 3600;
  seconds = seconds % 3600;
  unsigned long minutes = seconds / 60;

  char buffer[32];
  sprintf(buffer, "%lud %luh %lum", days, hours, minutes);
  return String(buffer);
}

// Nariši ozadje
void drawBackground()
{
  lcdDrawBackground();
}

// Preveri WiFi
void checkWiFi()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("WiFi ni povezan; ponovni poskus v ozadju.");
    WiFi.reconnect();
  }
  // Keep temperatures visible during network outages; update only the icon.
  displayTemperatures();
}

// Branje temperatur
void readTemperatures()
{
  sensors.requestTemperatures(); // DallasTemperature waits for conversion by default.

  t1 = readTemperatureByRom(0);
  t3 = readTemperatureByRom(2);
  t4 = readTemperatureByRom(3);
  t5 = readTemperatureByRom(4);
  t2 = readTemperatureByRom(1);
  t6 = readTemperatureByRom(5);

  temperaturesSampled = true;
  sampledAt = millis();
  Serial.printf("T: %.1f %.1f %.1f %.1f %.1f %.1f\n", t1, t2, t3, t4, t5, t6);
}

// Publish temperatur
void publishMQTT()
{
  if (!mqtt.connected())
  {
    connectMQTT();
  }
  mqtt.loop();

  char buf[10];

  dtostrf(t1, 4, 1, buf);
  mqtt.publish(TOPIC_BUF1, buf);

  dtostrf(t2, 4, 1, buf);
  mqtt.publish(TOPIC_BUF2, buf);

  dtostrf(t3, 4, 1, buf);
  mqtt.publish(TOPIC_BUF3, buf);

  dtostrf(t4, 4, 1, buf);
  mqtt.publish(TOPIC_BUF4, buf);

  dtostrf(t5, 4, 1, buf);
  mqtt.publish(TOPIC_BUF5, buf);

  dtostrf(t6, 4, 1, buf);
  mqtt.publish(TOPIC_BUF6, buf);

  Serial.println("Temperature published");
}

// Publish sistem info
void publishSystemInfo()
{
  if (!mqtt.connected())
  {
    connectMQTT();
  }
  mqtt.loop();

  String ip = WiFi.localIP().toString();
  mqtt.publish(TOPIC_IP, ip.c_str());

  char signal[10];
  sprintf(signal, "%d", WiFi.RSSI());
  mqtt.publish(TOPIC_SIGNAL, signal);

  String ssid = WiFi.SSID();
  mqtt.publish(TOPIC_SSID, ssid.c_str());

  String uptime = getUptimeString();
  mqtt.publish(TOPIC_UPTIME, uptime.c_str());

  Serial.println("System info published:");
  Serial.printf("  IP: %s\n", ip.c_str());
  Serial.printf("  Signal: %d dBm\n", WiFi.RSSI());
  Serial.printf("  SSID: %s\n", ssid.c_str());
  Serial.printf("  Uptime: %s\n", uptime.c_str());
}

// Prikaži temperature
void displayTemperatures()
{
  const float values[6] = {t1, t2, t3, t4, t5, t6};
  lcdRender(values, temperaturesSampled, WiFi.status() == WL_CONNECTED);
}

// ============ SETUP ============
void setup()
{
  Serial.begin(115200);
  Serial.println("\n\nZalogovniki LCD Start-------------------------------------------");

  tft.init();
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK);
  tft.setSwapBytes(true);

  showSplashScreen();
  delay(1000);

  updateProgressBar(20, "Loading SPIFFS...");
  filesystemAvailable = SPIFFS.begin(false); // Never format an existing filesystem on mount failure.
  Serial.println(filesystemAvailable ? "SPIFFS OK" : "SPIFFS unavailable; using embedded assets.");
  lcdInitialize(tft, filesystemAvailable);
  delay(500);

  updateProgressBar(40, "TFT Ready...");
  delay(500);

  connectWiFi();
  updateProgressBar(60, "WiFi Connected");
  delay(500);

  updateProgressBar(70, "Initializing Sensors...");
  sensors.begin();
  initializeSensorDiagnostics();
  // Keep each sensor's existing resolution; correcting ROM order must not change it.
  delay(500);

  updateProgressBar(80, "Connecting MQTT...");
  connectMQTT();
  delay(500);

  updateProgressBar(85, "Publishing Discovery...");
  publishDiscovery();
  delay(500);

  // *** PUBLISH SYSTEM INFO TAKOJ OB ZAGONU - 87% ***
  updateProgressBar(80, "Publishing System Info...");
  publishSystemInfo(); // DODAJ TO
  delay(500);

  updateProgressBar(95, "Setting up OTA...");
  ArduinoOTA.setHostname("zalogovniki-lcd");
  ArduinoOTA.setPassword(OTA_PASSWORD);

  ArduinoOTA.onStart([]()
                     {
    String type = (ArduinoOTA.getCommand() == U_FLASH) ? "sketch" : "filesystem";
    setDeviceRestartBlocked(RestartBlock::Ota, true);
    Serial.println("Start updating " + type);
    
    tft.fillScreen(TFT_BLUE);
    tft.setTextColor(TFT_WHITE);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("OTA UPDATE...", 240, 140, 4);
    tft.drawString("Please wait", 240, 180, 3); });

  ArduinoOTA.onEnd([]()
                   {
    Serial.println("\nOTA End");
    tft.fillScreen(TFT_GREEN);
    tft.setTextColor(TFT_WHITE);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("UPDATE COMPLETE", 240, 140, 4);
    tft.drawString("Rebooting...", 240, 180, 3); });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
                        {
    unsigned int percent = (progress / (total / 100));
    Serial.printf("Progress: %u%%\r", percent);
    
    int barWidth = 400;
    int barHeight = 30;
    int barX = (480 - barWidth) / 2;
    int barY = 220;
    
    tft.drawRect(barX, barY, barWidth, barHeight, TFT_WHITE);
    tft.fillRect(barX + 2, barY + 2, (barWidth - 4) * percent / 100, barHeight - 4, TFT_GREEN);
    
    tft.setTextColor(TFT_WHITE, TFT_BLUE);
    tft.setTextDatum(MC_DATUM);
    char buf[10];
    sprintf(buf, "%u%%", percent);
    tft.drawString(buf, 240, 270, 4); });

  ArduinoOTA.onError([](ota_error_t error)
                     {
    Serial.printf("Error[%u]: ", error);
    setDeviceRestartBlocked(RestartBlock::Ota, false);
    tft.fillScreen(TFT_RED);
    tft.setTextColor(TFT_WHITE);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("OTA ERROR!", 240, 160, 4);
    lcdRestoreAt = millis() + 1500; });

  ArduinoOTA.begin();
  Serial.println("OTA ready");
  delay(500);

  setupAppWeb(server, statusSnapshot, redrawLcd, filesystemAvailable, sensorSnapshot);

  updateProgressBar(100, "Complete!");
  delay(1000);

  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(1);

  drawBackground();
  readTemperatures();
  displayTemperatures();

  Serial.println("Setup complete! Firmware " FIRMWARE_VERSION);
}

// ============ LOOP ============
unsigned long lastTempUpdate = 0;
unsigned long lastSysUpdate = 0;
unsigned long lastWiFiCheck = 0;
const unsigned long TEMP_UPDATE_INTERVAL = 5000;
const unsigned long SYS_UPDATE_INTERVAL = 60000;
const unsigned long WIFI_CHECK_INTERVAL = 10000;

void loop()
{
  ArduinoOTA.handle();
  server.handleClient();
  handleDeviceRestart();
  if (lcdRestoreAt && static_cast<int32_t>(millis() - lcdRestoreAt) >= 0)
  {
    lcdRestoreAt = 0;
    redrawLcd();
  }

  if (millis() - lastWiFiCheck >= WIFI_CHECK_INTERVAL)
  {
    lastWiFiCheck = millis();
    checkWiFi();
  }

  if (!mqtt.connected())
  {
    connectMQTT();
  }
  mqtt.loop();

  if (mqtt.connected() && discoveryPending)
    publishDiscovery();

  if (millis() - lastTempUpdate >= TEMP_UPDATE_INTERVAL)
  {
    lastTempUpdate = millis();

    readTemperatures();
    publishMQTT();
    displayTemperatures();
  }

  if (millis() - lastSysUpdate >= SYS_UPDATE_INTERVAL)
  {
    lastSysUpdate = millis();
    publishSystemInfo();
  }

  delay(100);
}
