#include <WiFi.h>
#include <WiFiClient.h>
#include <SPIFFS.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <SPI.h>
#include <SD.h>
#include <HTTPClient.h>
#include <esp_int_wdt.h>
#include <esp_task_wdt.h>
#include "setup.hpp"
#include "webhandlers.hpp"



void setup(void)
{
  setupResetOutput();
  setupSerialDebug();
  setupSPIFFS();
  connectToWifiStation();
  hookUpWebHandlers();
  DBG_OUTPUT_PORT.println(F("Initialization done."));
}

void loop(void)
{
  webServer.handleClient();
}