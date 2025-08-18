// TODO:
// - Ping Cloud Function - DONE;
// - Disable Wi-Fi - DONE;
// - Disable Bluetooth - DONE;
// - Check/disable GPS - DONE;
// - ESP32 deep sleep - DONE;
// - Giroflex ON/OFF;
// - Optimizations;

// BUGS:
// - bug, it stoped to work but still with batery power;
// - started to work again after batery removed/re-inserted;
// - may turn modem off/on on setup()? (hard reset);
// - also sometimes the modem is not turning off before deep-sleep;

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
// #define TINY_GSM_RX_BUFFER 2048

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
const char server[] = "us-central1-prototype-alert-dc.cloudfunctions.net";
const int port = 443;

#include "esp_wifi.h"
#include "esp_bt.h"

#include <TinyGsmClient.h>
#include <ArduinoHttpClient.h>
#include <Arduino_JSON.h>

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
#define GYRO_PIN    2

void modemPowerOn() {
  pinMode(PWR_PIN, OUTPUT);
  digitalWrite(PWR_PIN, LOW);
  delay(1000); //Datasheet Ton mintues = 1S
  digitalWrite(PWR_PIN, HIGH);
}

void modemPowerOff() {
  pinMode(PWR_PIN, OUTPUT);
  digitalWrite(PWR_PIN, LOW);
  delay(1500); //Datasheet Ton mintues = 1.2S
  digitalWrite(PWR_PIN, HIGH);
}

void modemHardReset() {
  SerialMon.println(F("Performing modem hard reset..."));
  modemPowerOff();
  delay(5000); // Wait 5 seconds for complete power down
  modemPowerOn();
  delay(5000); // Wait 5 seconds for complete power up
}

void setup() {
  // Set LED OFF
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);

  // Init gyro pin as output
  pinMode(GYRO_PIN, OUTPUT);

  // Set console baud rate
  SerialMon.begin(115200);
  delay(1000);

  // Disable Wi-Fi and Bluetooth
  SerialMon.println(F("Disabling Wi-Fi and Bluetooth..."));
  esp_wifi_stop();
  esp_bt_controller_disable();

  // modem
  SerialMon.println(F("Power modem on..."));
  // TODO:
  // - if stall again, test with hard reset;
  // modemHardReset();
  modemPowerOn();

  // Wait 5 seconds for complete power up
  delay(5000);

  // SerialMon.println("Wait...");

  // Set GSM module baud rate
  SerialAT.begin(UART_BAUD, SERIAL_8N1, PIN_RX, PIN_TX);
  delay(6000);

  // Restart takes quite some time
  // To skip it, call init() instead of restart()
  SerialMon.println("Restating modem...");
  // modem.restart();
  modem.init();

  // update internal clock
  // modem.sendAT("+CCLK=\"25/08/13,21:17:00\"");

  // Disable GPS
  modem.disableGPS();

  // Set SIM7000G GPIO4 LOW ,turn off GPS power
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
    delay(10000);
    return;
  }
  SerialMon.println(" success");

  if (modem.isNetworkConnected()) {
    SerialMon.println("Network connected");
  }

  // TODO:
  // - skip modem.gprsConnect if modem.isGprsConnected();

  // debug:
  if (modem.isGprsConnected()) {
    SerialMon.println("GPRS already connected!");
  }

  // GPRS connection parameters are usually set after network registration
  SerialMon.print(F("Connecting to "));
  SerialMon.print(apn);
  if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
    SerialMon.println(" fail");
    delay(10000);
    return;
  }
  SerialMon.println(" success");

  if (modem.isGprsConnected()) {
    SerialMon.println("GPRS connected");
  }

  // keep modem clock updated
  // modem.sendAT("+CLTS=1");
  // modem.waitResponse();

  // Add debugging for local IP
  IPAddress local = modem.localIP();
  SerialMon.print("Local IP: ");
  SerialMon.println(local);

  // Now try HTTP request
  http.setTimeout(60000);
  http.connectionKeepAlive(); // this may be needed for HTTPS

  // Construct the resource URL
  String station_id = "cwb-1";
  String resource = String("/read_station?station=") + station_id;

  SerialMon.print(F("Performing HTTPS GET request... "));
  int err = http.get(resource);
  if (err != 0) {
    SerialMon.print(F("failed to connect, error: "));
    SerialMon.println(err);
    delay(10000);
    return;
  }

  int status = http.responseStatusCode();
  SerialMon.print(F("Response status code: "));
  SerialMon.println(status);
  if (!status) {
    delay(10000);
    return;
  }

  // TODO:
  // - actual gyroflex command;
  // - actual honk;
  // - sleep less / tuned for demonstration;

  String body = http.responseBody();
  SerialMon.println(F("Response:"));
  SerialMon.println(body);
  
  JSONVar myObject = JSON.parse(body);

  bool sound = myObject["data"]["sound"];
  bool visual = myObject["data"]["visual"];

  if (visual) {
    digitalWrite(GYRO_PIN, HIGH);
  } else {
    digitalWrite(GYRO_PIN, LOW);
  }

  SerialMon.println("readings: ");
  SerialMon.println("sound: " + String(sound));
  SerialMon.println("visual: " + String(visual));

  // SerialMon.print(F("Body length is: "));
  // SerialMon.println(body.length());

  // Shutdown

  // http.stop();
  // SerialMon.println(F("Server disconnected"));

  // modem.gprsDisconnect();
  // SerialMon.println(F("GPRS disconnected"));

  // Power down the modem using AT command first
  // SerialMon.println(F("Powering down modem with AT command..."));
  // modem.sendAT("+CPOWD=1");
  // modem.waitResponse();

  // Then turn off the modem power
  // SerialMon.println(F("Powering off modem..."));
  // modemPowerOff();
  // SerialMon.println(F("Modem off"));

  // Wait 5 seconds for complete power down
  // delay(5000);

  // sleep for 5 minutes
  // ESP.deepSleep(300e6);

  // wait for some time (1 second - testing...)
  // delay(1000);
}
