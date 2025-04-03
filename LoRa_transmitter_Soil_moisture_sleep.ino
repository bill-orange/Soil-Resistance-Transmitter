/* William E. Webb (c) 2025 MIT license

   LoRa Soil Moisture Transmitter program using AT commands for ESP32
   ** Transmitter **

   CRC code uses the work of Rob Tillaart under the MIT License
   Software Serial uses the work of Peter Lerup under the GNU General Public License v2.1
  ----------------------------------------------------------------------------------------
  William Webb (c) 2025
  04/02/2025 Working - Tested
  */

#include <esp_sleep.h>
#include <CRC8.h>            // CRC8 library
#include <CRC.h>             // CRC library

#define LED_PIN 21              // only likely correct for T8 board
#define DURATION 200           // how long to display message and allow for Tx Rx change
#define ADDRESS 1               // Address of receiving LoRa.  Must be in same Network
#define loRABAUD 115200         // set for IDE value
#define IDEBAUD 115200          // Bsaud rate for ID system Monitor
#define MYPORT_TX 22            // Transmit pin for software serial
#define MYPORT_RX 27           // Receive pin for software serial
#define SENSORIN 34             // Moisture Sensor digital
#define ANALOG_PIN 35           // Moisture Sensor Analog
#define RxBufferSize 2048       // Increase buffer for stability
#define uS_TO_S_FACTOR 1000000  // Conversion factor for microseconds to seconds
#define TIME_TO_SLEEP 1800      // Time in seconds (1/2 hour)
//#define TIME_TO_SLEEP 20  // Time in seconds (20 seconds)


bool debug = true;  // Print extra stuff for testing

bool highLow = false;                   // Blink light for receiving data
int CRCReceived = 0;                    // For received CRC8
int CRCTransmitted = 0;                 // For transmitted CRC8
String incomingstring = "VOID";         // Unparsed LoRa message
String lora_trans_prefix = "AT+SEND=";  // LoRa orefix send without CR/LF
String incomingstring_check = " ";      // Keep the incoming checksone string local

//                        Constructors

CRC8 crc;  // CRC calculator Constructor 8-Bit


/*-----------------------------------------------SETUP ----------------------------------------*/

void setup() {

  Serial.begin(IDEBAUD);
  Serial2.begin(loRABAUD, SERIAL_8N1, MYPORT_RX, MYPORT_TX);
  Serial.setRxBufferSize(RxBufferSize);
  Serial2.setRxBufferSize(RxBufferSize);

  pinMode(LED_PIN, OUTPUT);  // Initialize LED it will blink with sucessful data exchange

  Serial.println();

  Serial.println();
  Serial.println("RYLR998 LoRa Transmitter Demo");
  Serial.println(F(__FILE__ " " __DATE__ " " __TIME__));
  Serial.println();

  Serial2.println("AT+RESET");
  delay(1000);
  Serial2.println("AT");
   
   do {
    if (Serial2.available()) {
      incomingstring_check = Serial2.readString();
    }
    //Serial.print(".");
  } while (incomingstring_check.indexOf("+OK") == -1);  // Confusing.. This is a double negative

  Serial.print("Test before startup: ");
  Serial.println(incomingstring_check);



  // Check if the wakeup reason is from a timer
  if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER) {
    Serial.println("Woke up from deep sleep");
  } else {
    Serial.println("Fresh boot");
  }

  // Run your soilSend function
  smartBlink(200);
  soilSend();

  // Put the ESP32 into deep sleep
  Serial.println("Going to sleep for an half an hour...");
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  esp_deep_sleep_start();


  
}

/*---------------------------------------------- LOOP ------------------------------------------------*/

void loop() {
/*
  //  transmit phrase and environmental info
  soilSend();  // Transmits environmental data
  delay(10000);
  yield();  // Housekeeping
  */
}
/*---------------------------------------------- Temp and Pressure Send ------------------------------------------------*/

void soilSend() {

  // Sends and receives environmental data with CRC8 check
  String value = moistureSensor();
  Serial.print(" Moisture Sensor Analog value 0-100: ");
  Serial.println(value);
  CRCTransmitted = DataTransmitter(value);  // Sends phrase and calculates decimal CRC8
  CRCReceived = DataReceiver();             // Gets CRC8 from receiver
  CRC8Compare();                            // Compares CRCs and blinks LED if a match
}

/*---------------------------------------------- CRC CAlculator ------------------------------------------------*/

uint8_t CRCCalculator(String message) {

  // Function to calculate CRC8 returns uint8_t

  crc.reset();  // see crc library examples on Github
  char char_array[message.length()];
  message.toCharArray(char_array, message.length());
  crc.add((uint8_t*)char_array, message.length());
  return crc.calc();
}

/*---------------------------------------------- Data Transmitter ------------------------------------------------*/

// Transmits message string on second serial port

int DataTransmitter(String message) {

  // This next bit causes the sketch to wait until tne message was sent

  String incomingstring_check = " ";  // Keep the incoming checksone string local

  String sendIt = lora_trans_prefix + ADDRESS + "," + message.length() + "," + message;
  Serial.println(sendIt);

  int sendIt_len = sendIt.length() + 1;
  char sendIt_array[sendIt_len];
  sendIt.toCharArray(sendIt_array, sendIt_len);
  Serial2.write(sendIt_array);
  Serial2.write(13);
  Serial2.write(10);

  uint8_t crc = CRCCalculator(message);                   // Calculate CRC8
  int crcInt = (String(crc)).toInt();                     // Change to INT for use with the compare later
  Serial.print("CRC in decimal of the last message **");  // Prings below are diagnostic
  Serial.print(message);
  Serial.print("** is: ");
  Serial.println(crc);

  do {
    if (Serial2.available()) {
      incomingstring_check = Serial2.readString();
    }
  } while (incomingstring_check.indexOf("+OK") == -1);  // Confusing.. This is a double negative
  Serial.print("Test after Transmit: ");
  Serial.println(incomingstring_check);
  yield();  // Don't want a timeout

  return crcInt;  // return a CRC8 INT
}

/*---------------------------------------------- Data Receiver ------------------------------------------------*/

int DataReceiver() {

  // This function receives the CRC from the Receiver on Serial2
  delay(DURATION);  // Needed to be ready to receive
  if (Serial2.available()) {
    incomingstring = Serial2.readString();  // Get checksum
    Serial.print ("incoming String: ");Serial.println (incomingstring);
  }

  int crcReceived = parseMessage(incomingstring);  // Parse out CRC from incoming string
  Serial.print(" checksum of message in decimal is: ");
  Serial.println(crcReceived);
  incomingstring = "-1 ";  // Flush old value

  return (crcReceived);  // Return CEC8 as an INT
}

/*------------------------------- Parse Message ------------------------------------*/
int parseMessage(String incomingstring) {
  //Serial.println(incomingstring);
  // Pretty standard C++ string parser for comma delimited string

  int first = incomingstring.indexOf(",");
  int second = incomingstring.indexOf(",", first + 1);
  int third = incomingstring.indexOf(",", second + 1);

  //int fourth = incomingstring.indexOf(",", third + 1);
  //int fifth = incomingstring.indexOf("\r", fourth + 1);
  //String message = incomingstring.substring(second + 1, third);
  //String signal_st = incomingstring.substring(third + 1, fourth);
  //String sn = incomingstring.substring(fourth + 1, fifth);

  return incomingstring.substring(second + 1, third).toInt();  // Return received CRC8 stripped of ther info
}

/*------------------------------- CRC8 Compare ------------------------------------*/

void CRC8Compare() {

  // this fuction does a very straightforward compare of the two CRC8s and blinks the LED

  if (CRCTransmitted == CRCReceived) {

    Serial.println("                                   Great .......... CRC MATCH!");

    if (highLow) {
      digitalWrite(LED_PIN, HIGH);  // turn the LED on (HIGH is the voltage level)
      highLow = !highLow;
    } else {
      digitalWrite(LED_PIN, LOW);  // turn the LED off by making the voltage LOW
      highLow = !highLow;
    }

  }

  else {
    Serial.print(CRCTransmitted);
    Serial.print(" CRC Transmit vs ");
    Serial.print(CRCReceived);
    Serial.print(" CRCReceive");
    Serial.println("                                                          Bah! .......... Failed!");
    //delay(DURATION);  // if no LED Blink we still need a delay
  }
}

/*-------------------------------------- Blink---------------------------------------------*/
static void smartBlink(unsigned long ms) {
  unsigned long start = millis();

  do {
    digitalWrite(LED_PIN, HIGH);  // turn the LED on (HIGH is the voltage level)

  } while (millis() - start < ms);
  digitalWrite(LED_PIN, LOW);  // turn the LED off by making the voltage LOW
}

/*-------------------------------------- Moisture Sensor ---------------------------------------------*/
String moistureSensor() {
  int analogValue = analogRead(ANALOG_PIN);  // Read the analog value

  analogValue = constrain(analogValue, 0, 2630);
  analogValue = map(analogValue, 0, 2630, 0, 2000);
  return String(analogValue);
}

/*-------------------------------------- RYL988 Sleep Mode ---------------------------------------------*/

void sleepMode(int snooze) {

  if (snooze == 0) {
    DataTransmitter("AT+MODE=0");
    Serial.println("                      REL988 out of sleep");
    delay(2000);
  } else {
    DataTransmitter("AT+MODE=1");
    Serial.println("                      REL988 in sleep");
    delay(2000);
  }
}
/*-----------------------------------------------------------------------------------*/