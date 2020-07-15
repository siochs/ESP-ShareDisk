#define DBG_OUTPUT_PORT Serial
#define ONBOARD_LED     2
#define RESET_OUTPUT    33  // is wired to EN pin

#include "secrets.hpp"
#include "driver/periph_ctrl.h"
#include "esp_wifi.h"
const char* wifiSsid = SECRET_WIFI_SSID;
const char* wifiPassword = SECRET_WIFI_PASSWORD;
const char* wifiHost = SECRET_WIFI_HOSTNAME;

void hardRestart()
{
  // prevents sporadic reconnect failures after restart.
  esp_wifi_disconnect();
  esp_wifi_stop();
  esp_wifi_deinit();  
  
  // none of the folowing does not release the SPI bus correctly
  /*esp_task_wdt_init(1, true);
  esp_task_wdt_add(NULL);
  while (true)
    ;
  esp_restart();*/
  // also on soft reset the onboard LED which lights wenn the sd card was claimed
  // remanins on, indicating that on soft reset not all peripherals are cleared.
  // therefore doing it the hard way :-(
  pinMode(RESET_OUTPUT, OUTPUT);
  digitalWrite(RESET_OUTPUT, LOW);
  sleep(1);
}

void turnOnOnboadLed()
{
  pinMode(ONBOARD_LED, OUTPUT);
  digitalWrite(ONBOARD_LED, HIGH);
}

void setupSerialDebug()
{
  DBG_OUTPUT_PORT.begin(115200);
  DBG_OUTPUT_PORT.setDebugOutput(true);
  DBG_OUTPUT_PORT.println(F("Serial debug line initialized."));
}

void connectToWifiStation()
{
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiSsid, wifiPassword);
  while (WiFi.status() != WL_CONNECTED)
  {
    DBG_OUTPUT_PORT.print(F("Attempt to connect to "));
    DBG_OUTPUT_PORT.print(wifiSsid);
    DBG_OUTPUT_PORT.println(F("..."));
    WiFi.setHostname(SECRET_WIFI_HOSTNAME);
    delay(100);
  }
  DBG_OUTPUT_PORT.print(F("WiFi is connected to station. I am "));
  DBG_OUTPUT_PORT.print(WiFi.localIP());
  DBG_OUTPUT_PORT.print(F(" or hostname "));
  DBG_OUTPUT_PORT.println(WiFi.getHostname());
}

void setupSPIFFS()
{
  DBG_OUTPUT_PORT.println(F("Mount SPIFFS"));
  if (!SPIFFS.begin())
  {
    DBG_OUTPUT_PORT.println(F("Could not mount SPIFFS"));
    while (true)
      delay(1000);
  }
}

void setupResetOutput()
{
  // use input pullup, so that the serial reset does not need to overcome the ESP32's output current
  pinMode(RESET_OUTPUT, INPUT_PULLUP);
}