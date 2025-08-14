// TODO:
// - Ping ThingSpeak Generic HTTPS - May need to upgrade Firmware to support TLS. FW updated;
// - Disable Wi-Fi - DONE;
// - Disable Bluetooth - DONE;
// - Check/disable GPS - my A7670SA may even have GPS;
// - ESP32 deep sleep;
// - Modem off while sleep;
// - Optimizations;

#include "utilities.h"

// Set serial for debug console (to the Serial Monitor, default speed 115200)
#define SerialMon Serial

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
HttpClient          http(client, server, port);

// #define LED_PIN     12
#define GYRO_PIN      2
#define SIREN_PIN     4

void setup() {
  // Set LED OFF
  // pinMode(LED_PIN, OUTPUT);
  // digitalWrite(LED_PIN, HIGH);

  // Init gyro and siren pin as output
  pinMode(GYRO_PIN, OUTPUT);
  pinMode(SIREN_PIN, OUTPUT);

  // Set console baud rate
  SerialMon.begin(115200);
  delay(1000);

  SerialMon.println("Hello A7670SA!!!");

  // Disable Wi-Fi and Bluetooth
  SerialMon.println(F("Disabling Wi-Fi and Bluetooth..."));
  esp_wifi_stop();
  esp_bt_controller_disable();

  // modem
  SerialMon.println(F("Powering modem on..."));

  // SerialMon.println("Wait...");

  /* Set Power control pin output
  * * @note      Known issues, ESP32 (V1.2) version of T-A7670, T-A7608,
  *            when using battery power supply mode, BOARD_POWERON_PIN (IO12) must be set to high level after esp32 starts, otherwise a reset will occur.
  * */
  pinMode(BOARD_POWERON_PIN, OUTPUT);
  digitalWrite(BOARD_POWERON_PIN, HIGH);

  // Set modem reset
  pinMode(MODEM_RESET_PIN, OUTPUT);
  digitalWrite(MODEM_RESET_PIN, !MODEM_RESET_LEVEL);

  // Pull down DTR to ensure the modem is not in sleep state
  pinMode(MODEM_DTR_PIN, OUTPUT);
  digitalWrite(MODEM_DTR_PIN, LOW);

  // Turn on modem
  pinMode(BOARD_PWRKEY_PIN, OUTPUT);
  digitalWrite(BOARD_PWRKEY_PIN, LOW);
  delay(100);
  digitalWrite(BOARD_PWRKEY_PIN, HIGH);
  delay(MODEM_POWERON_PULSE_WIDTH_MS);
  digitalWrite(BOARD_PWRKEY_PIN, LOW);

  // Set GSM module baud rate
  SerialAT.begin(MODEM_BAUDRATE, SERIAL_8N1, MODEM_RX_PIN, MODEM_TX_PIN);
  delay(6000);

  // Restart takes quite some time
  // To skip it, call init() instead of restart()
  SerialMon.println("Restating modem...");
  modem.restart();
  // modem.init();

  // Disable GPS
  // modem.disableGPS();

  delay(200);

  String name = modem.getModemName();
  SerialMon.println("Modem Name: " + name);

  String modemInfo = modem.getModemInfo();
  SerialMon.println("Modem Info: " + modemInfo);

  // Check SIM status
  SerialMon.print("SIM Status: ");
  SerialMon.println(modem.getSimStatus());

  String res;
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

  // GPRS connection parameters are usually set after network registration
  if (modem.isGprsConnected()) {
    SerialMon.println("GPRS already connected!");
  } else {
    SerialMon.print(F("Connecting to "));
    SerialMon.print(apn);
    if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
      SerialMon.println(" fail");
      delay(10000);
      return;
    }
    SerialMon.println(" success");
  }

  // Add debugging for local IP
  IPAddress local = modem.localIP();
  SerialMon.print("Local IP: ");
  SerialMon.println(local);

  // Now try HTTP request
  // http.setTimeout(30000);
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

  String body = http.responseBody();
  SerialMon.println(F("Response:"));
  SerialMon.println(body);

  JSONVar myObject = JSON.parse(body);

  bool visual = myObject["data"]["visual"];
  bool sound = myObject["data"]["sound"];

  // ϟ Turn ON/OFF the gyroflex and siren
  digitalWrite(GYRO_PIN, visual);
  digitalWrite(SIREN_PIN, sound);

  SerialMon.println("readings: ");
  SerialMon.println("sound: " + String(sound));
  SerialMon.println("visual: " + String(visual));

  // Shutdown
  // http.stop();vs
  // SerialMon.println(F("Server disconnected"));

  // modem.gprsDisconnect();
  // SerialMon.println(F("GPRS disconnected"));

  // // Power down the modem using AT command first
  // SerialMon.println(F("Powering down modem with AT command..."));
  // modem.sendAT("+CPOWD=1");
  // modem.waitResponse();

  // // Then turn off the modem power
  // SerialMon.println(F("Powering off modem..."));
  // modemPowerOff();
  // SerialMon.println(F("Modem off"));

  // TODO:
  // - check this out:
  // modem.setNetworkActive();
  // modem.setNetworkDeactivate();

  // Wait 5 seconds for complete power down
  // delay(5000);

  // sleep for 5 minutes
  // ESP.deepSleep(300e6);

  // wait for some time (1 second - testing...)
  delay(5000);
}
