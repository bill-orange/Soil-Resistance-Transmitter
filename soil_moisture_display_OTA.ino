/* LoRa prottype program for soil measurment ESP32 only
                **Receiver**
  ------------------------------------------
  William Webb (c) 2025 MIT license

  04/04/2025 First test
 */

#include <WiFi.h>               // Needed for Version 3 Board Manager
#include <WiFiMulti.h>          // Needed for more than one possible SSDI
#include "FS.h"                 // For OTA Card
#include "SD.h"                 // For OTA Card
#include <TFT_eSPI.h>           // Hardware-specific library
TFT_eSPI tft = TFT_eSPI();      // For Display
#include <HTTPClient.h>         // Get Web Data
#include <WiFiClient.h>         // For OTA
#include <WebServer.h>          // For OTA
#include <ElegantOTA.h>         // OTA Library ESP32 compatable
#include "support_functions.h"  // Process PNG for TFT display
#include "secrets.h"            //  Just what the name implies plus location info

SPIClass hspi(HSPI);  // Start SPI instance

String incomingstring = "void";  // Unparsed LoRa message
String message = "void";         // Parsed LoRa message
String stringMessage = "Pending";       // Contains soil resistance
String signal_st = "void";       // Signal strenght in dB for some reason the word signal
                                 // is reserved in the ESP32 compiler
String sn = "00";                // Signal-to-Noise Ratio

#define __FILENAME__ (strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 : __FILE__)
#define DISPLAYBAUD 115200  // set for LoRa communication/#define DISPLAYBAUD 115200
#define IDEBAUD 115200      // set for IDE communication
#define ADDRESS 2           // Address of receiving LoRa.  Must be in same Network
#define LED_PIN 21          // only likely correct for T8 board
#define MYPORT_TX 22        // Transmit pin for serial
#define MYPORT_RX 27        // Receive pin for serial

// REASSIGN_PINS for uSD
int sck = 14;
int miso = 2;
int mosi = 15;
int cs = 13;

const float limit = 275;
unsigned long ota_progress_millis = 0;  // OTA timer

WiFiMulti wifiMulti;   // Constructor for multiple wifi ssid
WebServer server(80);  // OTA web server instance

/*-------------------------------- Display Graphics ----------------------------------------*/
void showGraphic(String(image), int(relayState)) {
  uint32_t t = millis();

  setPngPosition(0, 0);
  String githubURL = GITHUBURL + image;
  const char *URLStr = githubURL.c_str();
  Serial.println(URLStr);
  load_png(URLStr);
  t = millis() - t;
  Serial.print(t);
  Serial.println(" ms to load URL");
}

/*-----------------------------------------------SETUP ----------------------------------------*/

void setup() {

  Serial.begin(IDEBAUD);  // Does not have to be the same as LoRa baud
  Serial2.begin(DISPLAYBAUD, SERIAL_8N1, MYPORT_RX, MYPORT_TX);
  delay(3000);
  Serial.println("Setup Running.......");
  pinMode(LED_PIN, OUTPUT);  // Blink LED on data received

  hspi.begin(sck, miso, mosi, cs);
  delay(2000);

  tft.begin();
  tft.fillScreen(0);
  tft.setRotation(2);
  fileInfo();
  connectToWifiNetwork();

  Serial.println("RYLR998 LORA Soil Moisture Display");
  Serial.println(F(__FILE__ " " __DATE__ " " __TIME__));

  configureServer();          // Setup the server route
  ElegantOTA.begin(&server);  // Start ElegantOTA
  ElegantOTA.onStart(onOTAStart);
  ElegantOTA.onProgress(onOTAProgress);
  ElegantOTA.onEnd(onOTAEnd);
  server.begin();

  Serial.println("HTTP server started");

  String incomingstring_check = " ";  // Keep the incoming checksone string local

  Serial2.println("AT+RESET");
  delay(2000);
  Serial2.println("AT");
  do {

    if (Serial2.available()) {
      incomingstring_check = Serial2.readString();
    }

  } while (incomingstring_check.indexOf("+OK") == -1);  // Confusing.. This is a double negative

  Serial.print("Test before startup: ");
  Serial.println(incomingstring_check);
  Serial.println("------------------------------------------------------------------------");
  Serial.println(" Setup Complete ................ Getting Data");
  delay(3000);
  
  showGraphic("wet.png", 1);
  String limitString = String(int(limit-1));
  displayText(limitString);
  delay(3000);
  showGraphic("dry.png", 1);
  limitString = String(int(limit+1));
  displayText(limitString);
  delay(3000);
  showGraphic("wait_for_it.png", 1);
}

/*---------------------------------------------- LOOP ------------------------------------------------*/
void loop() {

  server.handleClient();
  ElegantOTA.loop();

  digitalWrite(LED_PIN, LOW);  // turn the LED OFF waiting for data

  if (Serial2.available()) {
    incomingstring = Serial2.readString();

    if (incomingstring.indexOf("+OK") == -1) {  // Strip out all the +OK
      digitalWrite(LED_PIN, HIGH);              // turn the LED  ON processing data
      parseMessage();                           // break unformation in incoming string in to useful Strings
      //DataTransmitter(message);                 // Now that we have the message we can transmitt the CRC

      float floatMessage = message.toFloat();
      floatMessage = floatMessage / 3.5;
      stringMessage = String((int)floatMessage);
      Serial.print("                                    Soil Resistance 0-1000 ohms-meter: ");
      Serial.println(floatMessage);
      Serial.print("                                    Signal_strengh: ");
      Serial.print(signal_st);
      Serial.print("dB");
      if (floatMessage > limit) {
        showGraphic("dry.png", 1);
      } else if (floatMessage <= limit) {
        showGraphic("wet.png", 1);
        displayText(stringMessage);
      } else {
        showGraphic("wait_for_it.png", 1);
        displayText(stringMessage);
      }

    } else {
      Serial.print("rejected message: ");
      Serial.println(incomingstring);
      showGraphic("error.png", 1);
    }
  }

  yield();  // Housekeeping
}

/*------------------------------- Parse Message ------------------------------------*/
void parseMessage() {

  // Very standard C++ parser for comma delimited String

  int first = incomingstring.indexOf(",");
  int second = incomingstring.indexOf(",", first + 1);
  int third = incomingstring.indexOf(",", second + 1);
  int fourth = incomingstring.indexOf(",", third + 1);
  int fifth = incomingstring.indexOf("\r", fourth + 1);

  message = incomingstring.substring(second + 1, third);
  signal_st = incomingstring.substring(third + 1, fourth);
  sn = incomingstring.substring(fourth + 1, fifth);
}

/*---------------------------- File information  ------------------------------------------*/
void fileInfo() {  // uesful to figure our what software is running


  tft.fillScreen(TFT_BLUE);
  tft.setTextColor(TFT_WHITE);  // Print to TFT display, White color
  tft.setTextSize(1);
  tft.drawString("        Soil Moisture Receiver", 8, 60);
  tft.drawString("        with Graphics", 30, 70);
  tft.setTextSize(1);
  tft.drawString(__FILENAME__, 35, 110);
  tft.drawString(__DATE__, 45, 140);
  tft.drawString(__TIME__, 140, 140);
  delay(3000);
}

/*-------------------------------- Connect to the Wifi network ------------------------------------*/
void connectToWifiNetwork() {  // Boilerplate from example (mostly)

  delay(10);
  WiFi.mode(WIFI_STA);
  wifiMulti.addAP(ssid_1, password_1);
  wifiMulti.addAP(ssid_2, password_2);

  // WiFi.scanNetworks will return the number of networks found
  int n = WiFi.scanNetworks();
  Serial.println("scan done");
  if (n == 0) {
    Serial.println("no networks found");
    showGraphic("error.png", 1);
  } else {
    Serial.print(n);
    Serial.println(" networks found");
    for (int i = 0; i < n; ++i) {
      // Print SSID and RSSI for each network found
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print(WiFi.SSID(i));
      Serial.print(" (");
      Serial.print(WiFi.RSSI(i));
      Serial.print(")");
      Serial.println((WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? " " : "*");
      delay(10);
    }
  }

  // Connect to Wi-Fi using wifiMulti (connects to the SSID with strongest connection)
  Serial.println("Connecting Wifi...");
  if (wifiMulti.run() == WL_CONNECTED) {
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
    WiFi.setAutoReconnect(true);  // Reconnect if disconnected
    WiFi.persistent(true);        // Reconnect if disconnected
    showGraphic("WiFiConnected.png", 1);
  } else {  // Handle error
    showGraphic("error.png", 1);
    delay(6000);
  }
}

/*-------------------------------------------------------Display Test --------------------------------------*/

void displayText(String textString) {

  tft.setTextColor(TFT_BLACK);  // Print to TFT display, White color
  tft.setTextSize(3);
  tft.drawString(textString, 90, 60);
}

/*---------------------------- Helper Functions for OTA----------------------------------------*/
void onOTAStart() {
  // Log when OTA has started
  Serial.println("OTA update started!");
  // <Add your own code here>
}

void onOTAProgress(size_t current, size_t final) {
  // Log every 1 second
  if (millis() - ota_progress_millis > 1000) {
    ota_progress_millis = millis();
    Serial.printf("OTA Progress Current: %u bytes, Final: %u bytes\n", current, final);
  }
}

void onOTAEnd(bool success) {
  // Log when OTA has finished
  if (success) {
    Serial.println("OTA update finished successfully!");
  } else {
    Serial.println("There was an error during OTA update!");
  }
  // <Add your own code here>
}

/*----------------------------Send message to web page----------------------------------------*/
void configureServer() {
  
  /*server.on("/", []() {
    server.send(200, "text/plain", "Hi! This is Soil Moisture Receiver. use /update for UPDATE \n\n");
  }); */
  server.on("/", [&stringMessage]() {  // Capture by reference
    String message = "Hi! This is Soil Moisture Receiver. \n\n Use /update for UPDATE \n\n Current Soil Resistance (Ohm Meter): "
                     + stringMessage;
                    
    server.send(200, "text/plain", message);
  });
  yield();
}

