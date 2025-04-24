/* LoRa prottype program for soil measurment ESP32 only
                **Receiver**
  ------------------------------------------
  William Webb (c) 2025 MIT license

  03/24/2025 Just a prototype, not for release
  03/25/2025 added snooze function
  04/01/2025 added built-in map function for scaling
 */

#include <CRC8.h>  // CRC8 library
#include <CRC.h>   // CRC library

String incomingstring = "void";         // Unparsed LoRa message
String message = "void";                // Parsed LoRa message
String signal_st = "void";              // Signal strenght in dB for some reason the word signal
                                        // is reserved in the ESP32 compiler
String sn = "00";                       // Signal-to-Noise Ratio
String lora_trans_prefix = "AT+SEND=";  // LoRa prefix send without CR/LF

#define DISPLAYBAUD 115200  // set for LoRa communication/#define DISPLAYBAUD 115200
#define IDEBAUD 115200      // set for IDE communication
#define ADDRESS 2           // Address of receiving LoRa.  Must be in same Network
#define LED_PIN 21          // only likely correct for T8 board
#define MYPORT_TX 22        // Transmit pin for serial
#define MYPORT_RX 27        // Receive pin for serial

//------------------------------------------- CRC Initialization

CRC8 crc;  // Instance of CRC calculator

/*-----------------------------------------------SETUP ----------------------------------------*/

void setup() {

  Serial.begin(IDEBAUD);  // Does not have to be the same as LoRa baud
  Serial2.begin(DISPLAYBAUD, SERIAL_8N1, MYPORT_RX, MYPORT_TX);
  delay(3000);
  Serial.println("Setup Running.......");
  pinMode(LED_PIN, OUTPUT);  // Blink LED on data received

  Serial.println("RYLR998 LORA Soil Receiver");
  Serial.println(F(__FILE__ " " __DATE__ " " __TIME__));

  String incomingstring_check = " ";  // Keep the incoming checksone string local

  sleepMode(0);  // Want the transmitting LoRA to be awake
  delay(2000);

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
}

/*---------------------------------------------- LOOP ------------------------------------------------*/
void loop() {

  digitalWrite(LED_PIN, LOW);  // turn the LED OFF waiting for data

  //sleepMode(0);
  //delay(2000);
  if (Serial2.available()) {
    incomingstring = Serial2.readString();

    if (incomingstring.indexOf("+OK") == -1) {  // Strip out all the +OK
      digitalWrite(LED_PIN, HIGH);              // turn the LED  ON processing data
      parseMessage();                           // break unformation in incoming string in to useful Strings
      DataTransmitter(message);                 // Now that we have the message we can transmitt the CRC

      int integerMessage = message.toInt();
      integerMessage = integerMessage / 3.5;
      //integerMessage = map(integerMessage, 800, 2000, 175, 1000);
      Serial.print("                                    Soil Resistance 0-1000 ohms-meter: ");
      Serial.println(integerMessage);
      Serial.print("                                    Signal_strengh: ");
      Serial.print(signal_st);
      Serial.print("dB");

    } else {
      Serial.print("rejected message: ");
      Serial.println(incomingstring);
    }
  }
  //sleep(1);
  //Serial.print(".");
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

/*---------------------------------------------- CRC CAlculator ------------------------------------------------*/

uint8_t CRCCalculator(String message) {

  // CRC8 calculator see libraries examples on Github

  crc.reset();
  char char_array[message.length() + 1];
  message.toCharArray(char_array, message.length());
  crc.add((uint8_t*)char_array, message.length());
  return crc.calc();
}

/*---------------------------------------------- Data Transmitter ------------------------------------------------*/

void DataTransmitter(String message) {

  uint8_t crc = CRCCalculator(message);

  String CRCMessage = String(crc);

  String sendIt = lora_trans_prefix + ADDRESS + "," + CRCMessage.length() + "," + CRCMessage;
  Serial.println(" ");
  Serial.println(sendIt);
  int sendIt_len = sendIt.length() + 1;
  char sendIt_array[sendIt_len];
  sendIt.toCharArray(sendIt_array, sendIt_len);
  Serial2.write(sendIt_array);
  Serial2.write(13);
  Serial2.write(10);



  /*Serial2.print(lora_trans_prefix);
  Serial2.print(ADDRESS);
  Serial2.print(",");
  Serial2.print(CRCMessage.length());
  Serial2.print(",");
  Serial2.println(CRCMessage);*/



  String incomingstring_check = " ";  // Keep the incoming checksone string local

  do {
    if (Serial2.available()) {
      incomingstring_check = Serial2.readString();
    }

  } while (incomingstring_check.indexOf("+OK") == -1);  // Confusing.. This is a double negative

  Serial.println("  ");
  Serial.print("Test before Transmit: ");
  Serial.println(incomingstring_check);

  yield();  // Don't want a timeout
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
