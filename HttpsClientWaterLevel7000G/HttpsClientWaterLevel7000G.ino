// TODO:
// - Ping ThingSpeak Generic - DONE;
// - Disable Wi-Fi - DONE;
// - Disable Bluetooth - DONE;
// - Check/disable GPS - DONE;
// - ESP32 deep sleep - DONE;
// - Water Level - DONE;
// - Water Level Switch (use less battery) - DONE;
// - errors inside loop() may deep-sleep again instead of retry; this ensure the modem is proper re-initialized - DONE;
// - do not send "0" readings;
// - more optimizations;

// Select your modem:
#define TINY_GSM_MODEM_SIM7000SSL

// Set serial for debug console (to the Serial Monitor, default speed 115200)
#define SerialMon Serial

// Set serial for AT commands (to the module)
#define SerialAT Serial1

// Increase RX buffer to capture the entire response
// Chips without internal buffering (A6/A7, ESP8266, M590)
// need enough space in the buffer for the entire response
// else data will be lost (and the http library will fail).
#define TINY_GSM_RX_BUFFER 1024

// See all AT commands, if wanted
// #define DUMP_AT_COMMANDS

// Define the serial console for debug prints, if needed
#define TINY_GSM_DEBUG SerialMon
// #define LOGGING  // <- Logging is for the HTTP library

// Your GPRS credentials, if any
const char apn[] = "iot.datatem.com.br";
const char gprsUser[] = "datatem";
const char gprsPass[] = "datatem";

// Server details
const char server[] = "api.thingspeak.com";
const int port = 443;

const char* writeAPIKey = "RXF4B2VYF3HJIDNB";

#include "esp_wifi.h"
#include "esp_bt.h"

#include <TinyGsmClient.h>
#include <ArduinoHttpClient.h>

#ifdef DUMP_AT_COMMANDS
#include <StreamDebugger.h>
StreamDebugger debugger(SerialAT, SerialMon);
TinyGsm        modem(debugger);
#else
TinyGsm        modem(SerialAT);
#endif

TinyGsmClientSecure client(modem);
HttpClient    http(client, server, port);

#define UART_BAUD   115200
#define PIN_TX      27
#define PIN_RX      26
#define PWR_PIN     4
#define LED_PIN     12
#define LEVEL_PIN   2
#define SWITCH_PIN  0

// Sleep duration
#define SLEEP_DURATION_ON_SUCCESS 300e6 // 5 minutes
#define SLEEP_DURATION_ON_ERROR   60e6  // 1 minute

void modemPowerOn() {
  pinMode(PWR_PIN, OUTPUT);
  digitalWrite(PWR_PIN, LOW);
  delay(1000); // Datasheet Ton mintues = 1S
  digitalWrite(PWR_PIN, HIGH);
}

void modemPowerOff() {
  pinMode(PWR_PIN, OUTPUT);
  digitalWrite(PWR_PIN, LOW);
  delay(1200); // Datasheet Ton mintues = 1.2S
  digitalWrite(PWR_PIN, HIGH);
}

void modemHardReset() {
  SerialMon.println(F("Performing modem hard reset..."));
  modemPowerOff();
  delay(5000); // Wait 5 seconds for complete power down
  modemPowerOn();
  delay(5000); // Wait 5 seconds for complete power up
}

void shutdownAndDeepSleep(bool error = false) {
  http.stop();
  SerialMon.println(F("Server disconnected"));

  modem.gprsDisconnect();
  SerialMon.println(F("GPRS disconnected"));

  // Power down the modem using AT command first
  SerialMon.println(F("Powering down modem with AT command..."));
  modem.sendAT("+CPOWD=1");
  modem.waitResponse();

  // Then turn off the modem power
  SerialMon.println(F("Powering off modem pin..."));
  modemPowerOff();

  // Set this pin to LOW to avoid the modem to turn ON after esp32 enter deep sleep
  digitalWrite(PWR_PIN, LOW);

  SerialMon.println(F("Modem off"));

  if (error) {
    SerialMon.println(F("Entering deep sleep due to error..."));
    ESP.deepSleep(SLEEP_DURATION_ON_ERROR);
  } else {
    SerialMon.println(F("Entering deep sleep for success..."));
    ESP.deepSleep(SLEEP_DURATION_ON_SUCCESS);
  }
}

unsigned long readWaterLevel() {
  // power on MaxSonar
  digitalWrite(SWITCH_PIN, HIGH);
  delay(5000);

  int readings = 6;
  unsigned long maxDuration = 0;

  // take 6 measurements and goes with the higher
  for (int i = 0; i < readings; i++) {
    unsigned long duration = pulseIn(LEVEL_PIN, HIGH);
    if (duration > maxDuration) {
      maxDuration = duration;
    }
    delay(1000);
  }

  // power off MaxSonar
  digitalWrite(SWITCH_PIN, LOW);

  return maxDuration;
}

void setup() {
  // Set LED OFF
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);

  // Set switch pin
  pinMode(SWITCH_PIN, OUTPUT);

  // Set console baud rate
  SerialMon.begin(115200);
  delay(1000);

  // Disable Wi-Fi and Bluetooth
  SerialMon.println(F("Disabling Wi-Fi and Bluetooth..."));
  esp_wifi_stop();
  esp_bt_controller_disable();

  // always hard reset modem
  modemHardReset();

  // Set GSM module baud rate
  SerialAT.begin(UART_BAUD, SERIAL_8N1, PIN_RX, PIN_TX);
  delay(6000);

  // Restart takes quite some time
  // To skip it, call init() instead of restart()
  SerialMon.println("Restating modem...");
  // modem.restart();
  modem.init();

  // update modem clock (if not exact at least 'more recent')
  modem.sendAT("+CCLK=\"25/08/20,19:52:00\"");

  // print modem clock
  String clock_res;
  modem.sendAT("+CCLK?");
  modem.waitResponse(1000L, clock_res);
  SerialMon.println(clock_res);

  // Disable GPS
  modem.disableGPS();

  // Set SIM7000G GPIO4 LOW, turn off GPS power
  // CMD:AT+SGPIO=0,4,1,0
  // Only in version 20200415 is there a function to control GPS power
  modem.sendAT("+SGPIO=0,4,1,0");
  if (modem.waitResponse(10000L) != 1) {
    SerialMon.println(" SGPIO=0,4,1,0 false ");
  }

  delay(200);

  String name = modem.getModemName();
  SerialMon.println("Modem Name: " + name);

  String modemInfo = modem.getModemInfo();
  SerialMon.println("Modem Info: " + modemInfo);

  // Check SIM status
  SerialMon.print("SIM Status: ");
  SerialMon.println(modem.getSimStatus());

  String res;
  // 1 CAT-M
  // 2 NB-IoT
  // 3 CAT-M and NB-IoT
  res = modem.setPreferredMode(2);
  SerialMon.print("setPreferredMode: ");
  SerialMon.println(res);

  // 2 Automatic
  // 13 GSM only
  // 38 LTE only
  // 51 GSM and LTE only
  res = modem.setNetworkMode(2);
  SerialMon.print("setNetworkMode: ");
  SerialMon.println(res);
}

void loop() {
  // modem.gprsConnect(apn, gprsUser, gprsPass);

  SerialMon.print("Signal quality: ");
  SerialMon.println(modem.getSignalQuality());

  SerialMon.print("Waiting for network...");
  if (!modem.waitForNetwork()) {
    SerialMon.println(" fail");
    // !!!
    shutdownAndDeepSleep(true);
  }
  SerialMon.println(" success");

  if (modem.isNetworkConnected()) {
    SerialMon.println("Network connected");
  }

  // GPRS connection parameters are usually set after network registration
  if (modem.isGprsConnected()) {
    SerialMon.println("GPRS already connected.");
  } else {
    SerialMon.print(F("Connecting to "));
    SerialMon.print(apn);
    if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
      SerialMon.println(" fail");
      // !!!
      shutdownAndDeepSleep(true);
    }
    SerialMon.println(" success");
  }

  if (modem.isGprsConnected()) {
    SerialMon.println("GPRS connected.");
  }

  // Now try HTTPS request
  http.setTimeout(30000);
  http.connectionKeepAlive(); // this may be needed for HTTPS

  unsigned long maxDuration = readWaterLevel();
  // convert to centimetres
  float distance_cm = (float)maxDuration / 10;

  SerialMon.print("Distance (cm): ");
  SerialMon.println(distance_cm);

  // Construct the resource URL
  String resource = String("/update?api_key=") + writeAPIKey + "&field1=" + String(distance_cm);

  SerialMon.print(F("Performing HTTPS GET request... "));
  int err = http.get(resource);
  if (err != 0) {
    SerialMon.print(F("failed to connect, error: "));
    SerialMon.println(err);
    // !!!
    shutdownAndDeepSleep(true);
  }

  int status = http.responseStatusCode();
  SerialMon.print(F("Response status code: "));
  SerialMon.println(status);
  if (!status) {
    delay(10000);
    return;
  }

  String body = http.responseBody();
  SerialMon.println(F("Response:"));
  SerialMon.println(body);

  // Shutdown and deep-sleep
  shutdownAndDeepSleep(false);
}
