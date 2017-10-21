/*
   Sparkfun ESP8266 Thing Dev board Sketch
   to estimate the height of the water in our well tank
   and send this estimate to a web server.
*/
//345678901234567890123456789312345678941234567895123456789612345678971234567898

#include <ESP8266WiFi.h> // Defines WiFi, the WiFi controller object
#include <ESP8266HTTPClient.h>
#include <EEPROM.h>   // NOTE: ESP8266 EEPROM library differs from Arduino's.
#include <OneWire.h>  // https://github.com/PaulStoffregen/OneWire
#include <DallasTemperature.h> // from https://github.com/milesburton/Arduino-Temperature-Control-Library

/*
 * PIN_LED_L = LOW to light the on-board LED.
 *   The _L suffix indicates that the pin is Active-Low.
 *   Active-Low because that's how the ESP8266 Thing Dev board
 *   on-board LED is wired.
 * PIN_ONE_WIRE = the 1-Wire bus that all the temperature sensors
 *   are connected to.
 * 
 * Note about the ESP8266 Thing Dev board I/O pins: several pins are unsuitable
 * to be a 1-wire bus:
 *   0 is connected to an on-board 10K ohm pull-up resistor.
 *   2 is connected to an on-board 10K ohm pull-up resistor.
 *   5 is connected to an on-board LED ohm and 220 ohm resistor to Vcc.
 *   15 is connected to an on-board 10K pull-down resistor.
 *   16 is not (currently) supported by the OneWire library, because
 *     that pin requires special treatment in the low-level I/O code.
 */
const int PIN_LED_L = 5;
const int PIN_ONE_WIRE = 4;

/*
   The EEPROM layout, starting at START_ADDRESS, is:
   String[EEPROM_WIFI_SSID_INDEX] = WiFi SSID. A null-terminated string
   String[EEPROM_WIFI_PASS_INDEX] = WiFi Password. A null-terminated string 1
   TODO More to come: web site credentials.
   EEPROM_END_MARK

   To write these values, use the Sketch write_eeprom_strings.
   See https://github.com/bneedhamia/write_eeprom_strings
*/
const int START_ADDRESS = 0;      // The first EEPROM address to read from.
const byte EEPROM_END_MARK = 255; // marks the end of the data we wrote to EEPROM
const int EEPROM_MAX_STRING_LENGTH = 120; // max string length in EEPROM
const int EEPROM_WIFI_SSID_INDEX = 0;
const int EEPROM_WIFI_PASS_INDEX = 1;

/*
   httpGet = the object that controls the WiFi stack.
   pHttpStream = if non-null, the stream of data from our Http Get request
   wire = The 1-Wire interface manager.
*/
HTTPClient httpGet;
WiFiClient *pHttpStream = 0;
OneWire wire(PIN_ONE_WIRE);
DallasTemperature sensors(&wire);

/*
   WiFi access point parameters.

   wifiSsid = SSID of the network to connect to. Read from EEPROM.
   wifiPassword = Password of the network. Read from EEPROM.
 */
char *wifiSsid;
char *wifiPassword;

// Called once automatically on Reset.
void setup() {
  Serial.begin(9600);
  Serial.println(F("Reset."));

  // Set up all our pins.

  pinMode(PIN_LED_L, OUTPUT);
  digitalWrite(PIN_LED_L, HIGH); // ESP8266 Thing Dev LED is Active Low

  // read the wifi credentials from EEPROM, if they're there.
  wifiSsid = readEEPROMString(START_ADDRESS, 0);
  wifiPassword = readEEPROMString(START_ADDRESS, 1);
  if (wifiSsid == 0 || wifiPassword == 0) {
    Serial.println(F("EEPROM not initialized."));
    //TODO mark a state that loop should not execute.
    return;
  }

  // Do the one-time WiFi setup:
  WiFi.mode(WIFI_STA);    // Station (Client), not soft AP or dual mode.
  WiFi.setAutoConnect(false); // don't connect until I say
  WiFi.setAutoReconnect(true); // if the connection ever drops, reconnect.

  connectToAccessPoint();

  sensors.begin();
  uint8_t numDevices = sensors.getDeviceCount();
  Serial.print(numDevices);
  Serial.println(" temperature sensors responded.");

  DeviceAddress sensor0;
  if (!sensors.getAddress(sensor0, 0)) {
    Serial.println("Sensor[0] was not found.");
    //TODO set error flag.
    return;
  } else {
    Serial.print("First sensor found: ");
    printDeviceAddress(sensor0);
  }
}

void loop() {
}

/*
 * Connect to the local WiFi Access Point.
 * The Auto Reconnect feature of the ESP8266 should keep us connected.
 */
void connectToAccessPoint() {
  Serial.print(F("Connecting to "));
  Serial.println(wifiSsid);

  WiFi.begin(wifiSsid, wifiPassword);
  // Wait for the connection or timeout.
  int wifiStatus = WiFi.status();
  while (wifiStatus != WL_CONNECTED)
  {
    delay(100);
    Serial.print(".");

    wifiStatus = WiFi.status();
  }

  Serial.println();

  Serial.print(F("Connected, IP address: "));
  Serial.println(WiFi.localIP());
}

/*
 * Prints a device address in a form that can be pasted into Arduino code
 * as an initializer of a DeviceAddress variable.
 */
void printDeviceAddress(DeviceAddress addr) {
  Serial.print("DeviceAddress x = {");
  for (int i = 0; i < 8; ++i) {
    if (i != 0) {
      Serial.print(", ");
    }
    Serial.print("0x");
    Serial.print(addr[i], HEX);
  }
  Serial.println("};");
}

/********************************
   From https://github.com/bneedhamia/write_eeprom_strings example
*/
/*
   Reads a string from EEPROM.  Copy this code into your program that reads EEPROM.

   baseAddress = EEPROM address of the first byte in EEPROM to read from.
   stringNumber = index of the string to retrieve (string 0, string 1, etc.)

   Assumes EEPROM contains a list of null-terminated strings,
   terminated by EEPROM_END_MARK.

   Returns:
   A pointer to a dynamically-allocated string read from EEPROM,
   or null if no such string was found.
*/
char *readEEPROMString(int baseAddress, int stringNumber) {
  int start;   // EEPROM address of the first byte of the string to return.
  int length;  // length (bytes) of the string to return, less the terminating null.
  char ch;
  int nextAddress;  // next address to read from EEPROM.
  char *result;     // points to the dynamically-allocated result to return.
  int i;


#if defined(ESP8266)
  EEPROM.begin(512);
#endif

  nextAddress = START_ADDRESS;
  for (i = 0; i < stringNumber; ++i) {

    // If the first byte is an end mark, we've run out of strings too early.
    ch = (char) EEPROM.read(nextAddress++);
    if (ch == (char) EEPROM_END_MARK) {
#if defined(ESP8266)
      EEPROM.end();
#endif
      return (char *) 0;  // not enough strings are in EEPROM.
    }

    // Read through the string's terminating null (0).
    int length = 0;
    while (ch != '\0' && length < EEPROM_MAX_STRING_LENGTH - 1) {
      ++length;
      ch = EEPROM.read(nextAddress++);
    }
  }

  // We're now at the start of what should be our string.
  start = nextAddress;

  // If the first byte is an end mark, we've run out of strings too early.
  ch = (char) EEPROM.read(nextAddress++);
  if (ch == (char) EEPROM_END_MARK) {
#if defined(ESP8266)
    EEPROM.end();
#endif
    return (char *) 0;  // not enough strings are in EEPROM.
  }

  // Count to the end of this string.
  length = 0;
  while (ch != '\0' && length < EEPROM_MAX_STRING_LENGTH - 1) {
    ++length;
    ch = EEPROM.read(nextAddress++);
  }

  // Allocate space for the string, then copy it.
  result = new char[length + 1];
  nextAddress = start;
  for (i = 0; i < length; ++i) {
    result[i] = (char) EEPROM.read(nextAddress++);
  }
  result[i] = '\0';

  return result;

}

