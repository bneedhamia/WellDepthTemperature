/*
   Arduino Sketch for a Sparkfun ESP8266 Thing Dev board
   to estimate the height of the water in our well tank
   and send this estimate to a web server.
   Water height is estimated using temperature differences read
   from a vertical line of MAX31820 temperature sensors
   glued to the tank exterior.

   Copyright (c) 2017 Bradford Needham
   { @bneedhamia , https://www.needhamia.com }

   Licensed under GPL V2
   a copy of which should have been supplied with this file.
*/
//345678901234567890123456789312345678941234567895123456789612345678971234567898

/*
   Flags to change the program behavior:
   PRINT_1_WIRE_ADDRESSES = uncomment this line to only
     print all the temperature sensors' 1-wire addresses,
     so you can copy them into the sensorAddress[] initializer.
     Comment this line out for normal Sketch behavior.
*/
//#define PRINT_1_WIRE_ADDRESSES true

#include <ESP8266WiFi.h> // Defines WiFi, the WiFi controller object
#include <ESP8266HTTPClient.h>
#include <EEPROM.h>   // NOTE: ESP8266 EEPROM library differs from Arduino's.
#include <OneWire.h>  // https://github.com/PaulStoffregen/OneWire
#include <DallasTemperature.h> // https://github.com/milesburton/Arduino-Temperature-Control-Library

/*
   Hardware information:

   PIN_LED_L = LOW to light the on-board LED.
     The _L suffix indicates that the pin is Active-Low.
     Active-Low because that's how the ESP8266 Thing Dev board
     on-board LED is wired.
   PIN_ONE_WIRE = the 1-Wire bus data line that all the temperature sensors
     are connected to.

   NUM_SENSORS = number of temperature sensors wired to the 1-Wire bus.

   Note about the ESP8266 Thing Dev board I/O pins: several pins are unsuitable
   to be a 1-wire bus:
     0 is connected to an on-board 10K ohm pull-up resistor.
     2 is connected to an on-board 10K ohm pull-up resistor.
     5 is connected to an on-board LED ohm and 220 ohm resistor to Vcc.
     15 is connected to an on-board 10K pull-down resistor.
     16 is not (currently) supported by the OneWire library, because
       that pin requires special treatment in the low-level I/O code.
*/
const int PIN_LED_L = 5;
const int PIN_ONE_WIRE = 4;

const int NUM_SENSORS = 12;

/*
 * Time (milliseconds) per attempt to read and report the temperatures.
 * Set this to, say 100L * 5 for temperature testing (e.g., to see which
 * sensor is which by heating one and seeing which sensor temperature changes).
 * Set this to over 5 minutes (1000L * 60L * 5) for normal operation,
 * so you don't overwhelm the server.
 */
const long MS_PER_TEMPERATURE_REQUEST = 1000L * 5;

/*
   The resolution (number of bits) to set the temperature sensors to.
   12 is the maximum; 9 is the minimum.
   Higher resolution takes longer (see the Max31820 datasheet for details)
   but is more precise.
*/
const uint8_t MAX31820_RESOLUTION_BITS = 12;

/*
   The EEPROM layout, starting at START_ADDRESS, is:
   String[EEPROM_WIFI_SSID_INDEX] = WiFi SSID. A null-terminated string
   String[EEPROM_WIFI_PASS_INDEX] = WiFi Password. A null-terminated string 1
   String[EEPROM_WIFI_TIMEOUT_SECS_INDEX = WiFi connection timeout, in seconds,
     as an Ascii string.
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
const int EEPROM_WIFI_TIMEOUT_SECS_INDEX = 2;

/*
   The states of the state machine that loop() runs.

   state = the current state of the machine. A STATE_* value.
   stateBegunMs = if needed, the time (millis() we entered this state.

   STATE_ERROR = an unrecoverable error has occurred. We stop.
   STATE_WAITING_FOR_TEMPERATURES = we've issued a command to the sensors
     to read the temperature, and are waiting for the time to read
     their responses.
     For example, a 12-bit temperature read requires 750ms.
     stateBegunMs = time (millis()) we entered this state.
   STATE_WAITING_FOR_NEXT_READ = we're waiting to request temperatures again.
     stateBegunMs = time (millis()) we entered this state.
*/

const uint8_t STATE_ERROR = 0;
const uint8_t STATE_WAITING_FOR_TEMPERATURES = 1;
const uint8_t STATE_WAITING_FOR_NEXT_READ = 2;
uint8_t state;
unsigned long stateBegunMs = 0L;

/*
   httpGet = the object that controls the WiFi stack.
   pHttpStream = if non-null, the stream of data from our Http Get request
   wire = The 1-Wire interface manager.

   NOTE: because the ESP8266 WiFi operates in the background,
     loop() must not take a long time.
     In particular, never (even in setup()) call any delay() greater than
     a few hundred milliseconds.
*/
HTTPClient httpGet;
WiFiClient *pHttpStream = 0;
OneWire wire(PIN_ONE_WIRE);
DallasTemperature wireDevices(&wire);

/*
   WiFi access point parameters.

   wifiSsid = SSID of the network to connect to. Read from EEPROM.
   wifiPassword = Password of the network. Read from EEPROM.
   wifiTimeoutMs = Wifi connection timeout, in milliseconds. Read from EEPROM.
*/
char *wifiSsid;
char *wifiPassword;
long wifiTimeoutMs;

/*
   sensorAddress[] = the 1-wire address of each sensor on the 1-wire bus,
   in order of distance from the controller ([0] = closest).
   Note: this order is not the order the sensors are returned by a search().

   I created this table by:
   1) Uncommenting PRINT_1_WIRE_ADDRESSES (above)
   2) Running the Sketch, and copying and pasting the output here.
   3) Commenting out PRINT_1_WIRE_ADDRESSES (above)
   4) Starting the Sketch.
   5) In turn, heating (or cooling) one sensor and seeing which position
     that sensor is in, based on the printed temperatures.
   6) rearranging the rows here to order by distance from the processor.
*/
DeviceAddress sensorAddress[NUM_SENSORS] = {
  {0x28, 0xC6, 0x8A, 0xA8, 0x7, 0x0, 0x0, 0x8A},
  {0x28, 0x52, 0x93, 0xA8, 0x7, 0x0, 0x0, 0x68},
  {0x28, 0x5A, 0xC6, 0xA8, 0x7, 0x0, 0x0, 0x8E},
  {0x28, 0xD2, 0xBB, 0xA8, 0x7, 0x0, 0x0, 0x44},
  {0x28, 0xB8, 0xB8, 0xA8, 0x7, 0x0, 0x0, 0x6},
  {0x28, 0x76, 0xCA, 0xA8, 0x7, 0x0, 0x0, 0x64},
  {0x28, 0xD6, 0xCE, 0xA8, 0x7, 0x0, 0x0, 0x27},
  {0x28, 0xC8, 0xCC, 0xA8, 0x7, 0x0, 0x0, 0xEC},
  {0x28, 0x48, 0xCE, 0xA8, 0x7, 0x0, 0x0, 0x85},
  {0x28, 0x40, 0x90, 0xA8, 0x7, 0x0, 0x0, 0x13},
  {0x28, 0xF9, 0x8D, 0xA8, 0x7, 0x0, 0x0, 0x12},
  {0x28, 0xAB, 0xA8, 0xA8, 0x7, 0x0, 0x0, 0x78}
};

// Called once automatically on Reset.
void setup() {
  Serial.begin(9600);
  delay(100); // give the Serial port time to power up.
  Serial.println(F("Reset."));

  // Set up our pins.

  pinMode(PIN_LED_L, OUTPUT);
  digitalWrite(PIN_LED_L, HIGH); // ESP8266 Thing Dev LED is Active Low
  // PIN_ONE_WIRE is managed by the OneWire library.

  wireDevices.begin();  // searches the 1-wire bus for devices.

#if defined(PRINT_1_WIRE_ADDRESSES)
  // Report the device addresses, in no particular order, and quit.
  print1WireAddresses();

  state = STATE_ERROR;
  return;
#endif

  /*
   * Read the wifi parameters from EEPROM, if they're there.
   * Convert the timeout from a string and Seconds
   *   to a long number of milliseconds.
   */
  wifiSsid = readEEPROMString(START_ADDRESS, EEPROM_WIFI_SSID_INDEX);
  wifiPassword = readEEPROMString(START_ADDRESS, EEPROM_WIFI_PASS_INDEX);
  char *timeoutText = readEEPROMString(START_ADDRESS
    , EEPROM_WIFI_TIMEOUT_SECS_INDEX);
  if (wifiSsid == 0 || wifiPassword == 0 || timeoutText == 0) {
    Serial.println(F("EEPROM not initialized."));

    state = STATE_ERROR;
    return;
  }
  wifiTimeoutMs = atol(timeoutText) * 1000L;
  if (wifiTimeoutMs == 0L) {
    Serial.print(F("Garbled EEPROM WiFi timeout: "));
    Serial.println(timeoutText);

    state = STATE_ERROR;
    return;
  }

  // Do the one-time WiFi setup and connection
  WiFi.mode(WIFI_STA);    // Station (Client), not soft AP or dual mode.
  WiFi.setAutoConnect(false); // don't connect until I say
  WiFi.setAutoReconnect(true); // if the connection ever drops, reconnect.
  if (!connectToAccessPoint(wifiSsid, wifiPassword, wifiTimeoutMs)) {
    state = STATE_ERROR;
    return;
  }

  /*
     Set up the temperature sensors:
     Set the digital temperature resolution.
     Set the library to have requestTemperatures() return immediately
       rather than waiting the (too long for our WiFi) conversion time.
  */
  wireDevices.setResolution(MAX31820_RESOLUTION_BITS);
  wireDevices.setWaitForConversion(false);

  // Start the first temperature reading.
  wireDevices.requestTemperatures();
  state = STATE_WAITING_FOR_TEMPERATURES;
  stateBegunMs = millis();
}

// Called repeatedly, automatically.
void loop() {
  long nowMs;
  boolean succeeded; // a general flag for remembering a failure.

  nowMs = millis();

  switch (state) {
    case STATE_ERROR:
      // Blink the led.
      if (nowMs % 1000 < 500) {
        digitalWrite(PIN_LED_L, LOW);
      } else {
        digitalWrite(PIN_LED_L, HIGH);
      }
      break;

    case STATE_WAITING_FOR_TEMPERATURES:
      if ((long) (nowMs - stateBegunMs)
          < wireDevices.millisToWaitForConversion(MAX31820_RESOLUTION_BITS)) {
        return; // wait more.
      }

      // The temperatures are ready. Read them.
      succeeded = true;
      for (int i = 0; i < NUM_SENSORS; ++i) {
        float tempC = wireDevices.getTempC(sensorAddress[i]);
        if (tempC <= DEVICE_DISCONNECTED_C) {
          succeeded = false;
        }

        // for now, just report the temperature
        if (i != 0) {
          Serial.print(", ");
        }
        Serial.print(tempC, 1); // output 20.5 vs. 20.54
      }
      Serial.println();

      if (succeeded) {
        // report the temperatures to the web server.
        // Maybe always report them, converting the error value to a better one.
      }

      // Wait until it's time to request the temperatures again.
      state = STATE_WAITING_FOR_NEXT_READ;
      stateBegunMs = nowMs;

      break;

    case STATE_WAITING_FOR_NEXT_READ:
      if ((long) (nowMs - stateBegunMs) < MS_PER_TEMPERATURE_REQUEST) {
        return; // wait more.
      }

      // Request the sensors to read their temperatures.
      wireDevices.requestTemperatures();
      state = STATE_WAITING_FOR_TEMPERATURES;
      stateBegunMs = millis();
      break;

    default:
      Serial.print("unknown state: ");
      Serial.println(state);
      state = STATE_ERROR;
      break;
  }
}

/*
   Prints the 1-wire address of each device found on the 1-wire bus.
   The output is in a form that can be copied and pasted into an array
   initializer.

   Returns the number of devices on the bus if successful,
   0 if there is an error.
*/
uint8_t print1WireAddresses() {
  uint8_t numDevices;

  numDevices = wireDevices.getDeviceCount();
  Serial.print(numDevices);
  Serial.println(" temperature sensors responded:");

  DeviceAddress address;
  for (int i = 0; i < numDevices; ++i) {
    if (!wireDevices.getAddress(address, i)) {
      Serial.print("Sensor[");
      Serial.print(i);
      Serial.println("] was not found.");
      return 0;
    }
    Serial.print("  ");
    print1WireAddress(address);
    if (i < numDevices - 1) {
      Serial.print(",");
    }
    Serial.println();
  }

  return numDevices;
}

/*
   Prints a device address in a form that can be pasted into Arduino code
   as an initializer of a DeviceAddress variable.
*/
void print1WireAddress(DeviceAddress addr) {
  Serial.print("{");
  for (int i = 0; i < 8; ++i) {
    if (i != 0) {
      Serial.print(", ");
    }
    Serial.print("0x");
    Serial.print(addr[i], HEX);
  }
  Serial.print("}");
}

/*
   Connect to the local WiFi Access Point.
   The Auto Reconnect feature of the ESP8266 should keep us connected.

   ssid = the WiFi Access Point to connect to.
   pass = the password for that Access Point.
   timeoutMs = maximum time (milliseconds) to wait for the connection.

   Returns true if successful; false if a nonrecoverable error occurred.
*/
boolean connectToAccessPoint(char *ssid, char *pass, long timeoutMs) {
  unsigned long startTimeMs;  // time (millis()) when we started connecting.
  unsigned long now;  // the current time (millis())

  Serial.print(F("Connecting to "));
  Serial.println(ssid);

  startTimeMs = millis();
  WiFi.begin(ssid, wifiPassword);
  
  // Wait for the connection or timeout.
  int wifiStatus = WiFi.status();
  while (wifiStatus != WL_CONNECTED)
  {
    now = millis();

    if (wifiStatus == WL_NO_SSID_AVAIL) {
      // AP is offline.
      Serial.print(F("Unable to find "));
      Serial.println(ssid);
      return false;
    }
    if (wifiStatus == WL_CONNECT_FAILED) {
      // bad password
      Serial.print(F("Password incorrect for "));
      Serial.println(ssid);
      return false;
    }
    if ((long) (now - startTimeMs) > timeoutMs) {
      // Connection has taken too long
      Serial.print(F("Timeout connecting to "));
      Serial.println(ssid);
      return false;
    }

    /*
       The WiFi stack is still trying to connect.
       Give it a moment and check again.
    */

    delay(100);
    wifiStatus = WiFi.status();
  }

  Serial.print(F("Connected, IP address: "));
  Serial.println(WiFi.localIP());

  return true;
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
