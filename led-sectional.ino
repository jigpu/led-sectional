#include <ESP8266WiFi.h>
#include <FastLED.h>
#include <vector>
using namespace std;

#define FASTLED_ESP8266_RAW_PIN_ORDER

#define WIND_THRESHOLD 25 // Maximum windspeed for green, otherwise the LED turns yellow
#define LOOP_INTERVAL 5000 // ms - interval between brightness updates and lightning strikes
#define DO_LIGHTNING true // Lightning uses more power, but is cool.
#define DO_WINDS true // color LEDs for high winds
#define REQUEST_INTERVAL 900000 // How often we update. In practice LOOP_INTERVAL is added. In ms (15 min is 900000)

#define USE_LIGHT_SENSOR false // Set USE_LIGHT_SENSOR to true if you're using any light sensor.
// Set LIGHT_SENSOR_TSL2561 to true if you're using a TSL2561 digital light sensor.
// Kits shipped after March 1, 2019 have a digital light sensor. Setting this to false assumes an analog light sensor.
#define LIGHT_SENSOR_TSL2561 false

const char ssid[] = "EDITME"; // your network SSID (name)
const char pass[] = "EDITME"; // your network password (use for WPA, or use as key for WEP)

#define DATA_PIN    14 // Kits shipped after March 1, 2019 should use 14. Earlier kits us 5.
#define LED_TYPE    WS2811
#define COLOR_ORDER RGB
#define BRIGHTNESS 20 // 20-30 recommended. If using a light sensor, this is the initial brightness on boot.

/* This section only applies if you have an ambient light sensor connected */
#if USE_LIGHT_SENSOR
/* The sketch will automatically scale the light between MIN_BRIGHTNESS and
MAX_BRIGHTNESS on the ambient light values between MIN_LIGHT and MAX_LIGHT
Set MIN_BRIGHTNESS and MAX_BRIGHTNESS to the same value to achieve a simple on/off effect. */
#define MIN_BRIGHTNESS 20 // Recommend values above 4 as colors don't show well below that
#define MAX_BRIGHTNESS 20 // Recommend values between 20 and 30

// Light values are a raw reading for analog and lux for digital
#define MIN_LIGHT 16 // Recommended default is 16 for analog and 2 for lux
#define MAX_LIGHT 30 // Recommended default is 30 to 40 for analog and 20 for lux

#if LIGHT_SENSOR_TSL2561
#include <Adafruit_Sensor.h>
#include <Adafruit_TSL2561_U.h>
#include <Wire.h>
Adafruit_TSL2561_Unified tsl = Adafruit_TSL2561_Unified(TSL2561_ADDR_FLOAT, 12345);
#else
#define LIGHTSENSORPIN A0 // A0 is the only valid pin for an analog light sensor
#endif

#endif
/* ----------------------------------------------------------------------- */

// This list contains the list of airports in the order that LEDs have
// been wired up. Its length must match the number of LEDs.
//
// The strings "VFR", "WVFR", "MVFR", "IFR", and "LIFR" may be used for
// a map key and will always be the associated color.
//
// The string "NULL" may be used for "no airport"
std::vector<String> airports({
  "LIFR", // 1
  "IFR", // 2
  "MVFR", // 3
  "WVFR", // 4
  "VFR", // 5
  "NULL", // 6
  "NULL", // 7
  "KMRY", // 8
  "KSNS", // 9
  "KCVH", // 10
  "KWVI", // 11
  "KE16", // 12
  "KRHV", // 13
  "KSJC", // 14
  "KNUQ", // 15
  "KPAO", // 16
  "KSQL", // 17
  "KHAF", // 18
  "KSFO", // 19
  "KOAK", // 20
  "KHWD", // 21
  "KLVK", // 22
  "KC83", // 23
  "NULL", // 24
  "KCCR", // 25
  "NULL", // 26
  "KDVO", // 27
  "KO69", // 28
  "KSTS", // 29
  "NULL", // 30
  "KAPC", // 31
  "KSUU", // 32
  "KVCB", // 33
  "KEDU", // 34
  "KSMF", // 35
  "KSAC", // 36
  "KMHR", // 37
  "KMCC", // 38
  "KLHM", // 39
  "KMYV", // 40
  "KBAB", // 41
  "NULL", // 42
  "KOVE", // 43
  "NULL", // 44
  "KCIC", // 45
  "NULL", // 46
  "KRBL", // 47
  "NULL", // 48
  "NULL", // 49
  "NULL", // 50
  "KGOO", // 51
  "KBLU", // 52
  "NULL", // 53
  "KTRK", // 54
  "KRNO", // 55
  "KCXP", // 56
  "KMEV", // 57
  "KTVL", // 58
  "NULL", // 59
  "NULL", // 60
  "KAUN", // 61
  "KPVF", // 62
  "KJAQ", // 63
  "KCPU", // 64
  "KO22", // 65
  "NULL", // 66
  "NULL", // 67
  "KSCK", // 68
  "KTCY", // 69
  "NULL", // 70
  "KMOD", // 71
  "NULL", // 72
  "KMER", // 73
  "MKCE", // 74
  "NULL", // 75
  "KMAE", // 76
  "NULL", // 77
  "KFAT", // 78
  "NULL", // 79
  "KNLC" // 80
});
std::vector<CRGB> leds;
std::vector<unsigned short int> lightningLeds;

#define DEBUG false

#define READ_TIMEOUT 15 // Cancel query if no data received (seconds)
#define WIFI_TIMEOUT 60 // in seconds
#define RETRY_TIMEOUT 15000 // in ms

#define SERVER "aviationweather.gov"
#define BASE_URI "/api/data/metar?format=json&ids="

boolean ledStatus = true; // used so leds only indicate connection status on first boot, or after failure
int loops = -1;

int status = WL_IDLE_STATUS;

#define WX_CATEGORY_MASK 0x000F
#define WX_CATEGORY_VFR  0x0001
#define WX_CATEGORY_MVFR 0x0002
#define WX_CATEGORY_IFR  0x0003
#define WX_CATEGORY_LIFR 0x0004
#define WX_FLAG_WINDY   0x0010
#define WX_FLAG_GUSTY   0x0020
#define WX_FLAG_TS      0x0040

void setup() {
  //Initialize serial and wait for port to open:
  Serial.begin(74880);
  //pinMode(D1, OUTPUT); //Declare Pin mode
  //while (!Serial) {
  //    ; // wait for serial port to connect. Needed for native USB
  //}

  pinMode(LED_BUILTIN, OUTPUT); // give us control of the onboard LED
  digitalWrite(LED_BUILTIN, LOW);

  #if USE_LIGHT_SENSOR
  #if LIGHT_SENSOR_TSL2561
  Wire.begin(D2, D1);
  if(!tsl.begin()) {
    /* There was a problem detecting the TSL2561 ... check your connections */
    Serial.println("Ooops, no TSL2561 detected ... Check your wiring or I2C ADDR!");
  } else {
    tsl.enableAutoRange(true);
    tsl.setIntegrationTime(TSL2561_INTEGRATIONTIME_13MS);
  }
  #else
  pinMode(LIGHTSENSORPIN, INPUT);
  #endif
  #endif

  // Initialize LEDs
  leds.resize(airports.size());
  FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds.data(), leds.size()).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(BRIGHTNESS);
}

void adjustBrightness() {
#if USE_LIGHT_SENSOR
  unsigned char brightness;
  float reading;

  #if LIGHT_SENSOR_TSL2561
  sensors_event_t event;
  tsl.getEvent(&event);
  reading = event.light;
  #else
  reading = analogRead(LIGHTSENSORPIN);
  #endif

  Serial.print("Light reading: ");
  Serial.print(reading);
  Serial.print(" raw, ");

  if (reading <= MIN_LIGHT) brightness = 0;
  else if (reading >= MAX_LIGHT) brightness = MAX_BRIGHTNESS;
  else {
    // Percentage in lux range * brightness range + min brightness
    float brightness_percent = (reading - MIN_LIGHT) / (MAX_LIGHT - MIN_LIGHT);
    brightness = brightness_percent * (MAX_BRIGHTNESS - MIN_BRIGHTNESS) + MIN_BRIGHTNESS;
  }

  Serial.print(brightness);
  Serial.println(" brightness");
  FastLED.setBrightness(brightness);
  FastLED.show();
#endif
}

void loop() {
  digitalWrite(LED_BUILTIN, LOW); // on if we're awake

  adjustBrightness();

  int c;
  loops++;
  Serial.print("Loop: ");
  Serial.println(loops);
  unsigned int loopThreshold = 1;
  if (DO_LIGHTNING || USE_LIGHT_SENSOR) loopThreshold = REQUEST_INTERVAL / LOOP_INTERVAL;

  // Connect to WiFi. We always want a wifi connection for the ESP8266
  if (WiFi.status() != WL_CONNECTED) {
    if (ledStatus) fill_solid(leds.data(), leds.size(), CRGB::Orange); // indicate status with LEDs, but only on first run or error
    FastLED.show();
    WiFi.mode(WIFI_STA);
    WiFi.hostname("LED Sectional " + WiFi.macAddress());
    //wifi_set_sleep_type(LIGHT_SLEEP_T); // use light sleep mode for all delays
    Serial.print("WiFi connecting..");
    WiFi.begin(ssid, pass);
    // Wait up to 1 minute for connection...
    for (c = 0; (c < WIFI_TIMEOUT) && (WiFi.status() != WL_CONNECTED); c++) {
      Serial.write('.');
      delay(1000);
    }
    if (c >= WIFI_TIMEOUT) { // If it didn't connect within WIFI_TIMEOUT
      Serial.println("Failed. Will retry...");
      fill_solid(leds.data(), leds.size(), CRGB::Orange);
      FastLED.show();
      ledStatus = true;
      return;
    }
    Serial.println("OK!");
    if (ledStatus) fill_solid(leds.data(), leds.size(), CRGB::Purple); // indicate status with LEDs
    FastLED.show();
    ledStatus = false;
  }

  // Do some lightning
  if (DO_LIGHTNING && lightningLeds.size() > 0) {
    std::vector<CRGB> lightning(lightningLeds.size());
    for (unsigned short int i = 0; i < lightningLeds.size(); ++i) {
      unsigned short int currentLed = lightningLeds[i];
      lightning[i] = leds[currentLed]; // temporarily store original color
      leds[currentLed] = CRGB::White; // set to white briefly
      Serial.print("Lightning on LED: ");
      Serial.println(currentLed);
    }
    delay(25); // extra delay seems necessary with light sensor
    FastLED.show();
    delay(25);
    for (unsigned short int i = 0; i < lightningLeds.size(); ++i) {
      unsigned short int currentLed = lightningLeds[i];
      leds[currentLed] = lightning[i]; // restore original color
    }
    FastLED.show();
  }

  if (loops >= loopThreshold || loops == 0) {
    loops = 0;
    if (DEBUG) {
      fill_gradient_RGB(leds.data(), leds.size(), CRGB::Red, CRGB::Blue); // Just let us know we're running
      FastLED.show();
    }

    Serial.println("Getting METARs ...");
    if (getMetars()) {
      Serial.println("Refreshing LEDs.");
      FastLED.show();
      if ((DO_LIGHTNING && lightningLeds.size() > 0) || USE_LIGHT_SENSOR) {
        Serial.println("There is lightning or we're using a light sensor, so no long sleep.");
        digitalWrite(LED_BUILTIN, HIGH);
        delay(LOOP_INTERVAL); // pause during the interval
      } else {
        Serial.print("No lightning; Going into sleep for: ");
        Serial.println(REQUEST_INTERVAL);
        digitalWrite(LED_BUILTIN, HIGH);
        delay(REQUEST_INTERVAL);
      }
    } else {
      digitalWrite(LED_BUILTIN, HIGH);
      delay(RETRY_TIMEOUT); // try again if unsuccessful
    }
  } else {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(LOOP_INTERVAL); // pause during the interval
  }
}

int metarToWxInt(String airport, int wind, int gusts, float visib, int ceiling, String wxstring) {
  int wxInt = 0;

  Serial.print(airport);
  Serial.print(": VIS ");
  Serial.print(visib);
  Serial.print(" CEIL ");
  Serial.print(ceiling);
  Serial.print(" ");
  Serial.print(wind);
  Serial.print("G");
  Serial.print(gusts);
  Serial.print("kts WX: ");
  Serial.println(wxstring);

  if (visib < 1 || ceiling < 500) wxInt |= WX_CATEGORY_LIFR;
  else if (visib <= 3 || ceiling <= 1000) wxInt |= WX_CATEGORY_IFR;
  else if (visib <= 5 || ceiling <= 3000) wxInt |= WX_CATEGORY_MVFR;
  else wxInt |= WX_CATEGORY_VFR;

  if (wind > WIND_THRESHOLD) wxInt |= WX_FLAG_WINDY;
  if (gusts > WIND_THRESHOLD) wxInt |= WX_FLAG_GUSTY;
  if (wxstring.indexOf("TS") != -1) {
    Serial.println("... found lightning!");
    wxInt |= WX_FLAG_TS;
  }

  return wxInt;
}

String readUntil(BearSSL::WiFiClientSecure *client, String stop) {
  uint32_t t = millis();
  String buf = "";
  char c;

  while (client->connected()) {
    if ((millis() - t) >= (READ_TIMEOUT * 1000)) {
      break;
    }

    c = client->read();
    yield(); // Otherwise the WiFi stack can crash
    if (c < 0) {
      continue;
    }
    if (stop.indexOf(c) >= 0) {
      return buf;
    }

    buf += c;
    t = millis(); // Reset timeout clock
  }
  Serial.println("---Timeout---");
  fill_solid(leds.data(), leds.size(), CRGB::Cyan); // indicate status with LEDs
  FastLED.show();
  ledStatus = true;
  client->stop();
  return "";
}

int readCeiling(BearSSL::WiFiClientSecure *client) {
  String key, value;
  int ceiling = INT_MAX;
  bool is_ceiling = false;

  value = readUntil(client, "{]");
  if (value == "]") {
    return ceiling;
  }

  while (true) {
    value = readUntil(client, "\"]");
    if (value == "]") {
      break;
    }
    key = readUntil(client, "\"");
    (void)readUntil(client, ":");
    if (key == NULL) {
      break;
    }

    value = readUntil(client, "\",[}");
    value.trim();
    if (value == "\"") {
      value = readUntil(client, "\"");
      (void)readUntil(client, ",");
    }

    if (key == "cover") {
      is_ceiling = (value == "SCT" || value == "OVC" || value == "OVX");
    }
    if (key == "base") {
      if (is_ceiling && value.toInt() < ceiling) {
        ceiling = value.toInt();
      }
    }
  }

  return ceiling;
}

int readSingleMetar(BearSSL::WiFiClientSecure *client) {
  String airport, wind, gusts, wxstring, visib;
  int ceiling;
  String key, value;

  (void)readUntil(client, "{");
  while (true) {
    (void)readUntil(client, "\"");
    key = readUntil(client, "\"");
    (void)readUntil(client, ":");
    if (key == NULL) {
      break;
    }

    value = readUntil(client, "\",[}");
    value.trim();
    if (value == "\"") {
      value = readUntil(client, "\"");
      (void)readUntil(client, ",");
    }
    else if (value == "[") {
      // Only the "cloud" key is allowed to be an array.
      // We'll handle this seperately:
      ceiling = readCeiling(client);
      continue;
    }
    else if (value == "}") {
      // We've reached the end of the metar object
      int wxInt = metarToWxInt(airport, wind.toInt(), gusts.toInt(), visib.toFloat(), ceiling, wxstring);
      for (int i = 0; i < airports.size(); i++) {
        if (airports[i] == airport) {
          doColor(airport, i, wxInt);
        }
      }
      return 0;
    }

    if (key == "icaoId")        { airport = value;  }
    else if (key == "wspd")     { wind = value;     }
    else if (key == "wgst")     { gusts = value;    }
    else if (key == "wxString") { wxstring = value; }
    else if (key == "visib")    { visib = value;    }
  }

  // Ran out of data
  return -1;
}


bool getMetars(){
  lightningLeds.clear(); // clear out existing lightning LEDs since they're global
  fill_solid(leds.data(), leds.size(), CRGB::Black); // Set everything to black just in case there is no report
  uint32_t t;
  String airportString = "";
  bool firstAirport = true;
  for (int i = 0; i < airports.size(); i++) {
    if (airports[i] != "NULL" && airports[i] != "VFR" && airports[i] != "MVFR" && airports[i] != "WVFR" && airports[i] != "IFR" && airports[i] != "LIFR") {
      if (firstAirport) {
        firstAirport = false;
        airportString = airports[i];
      } else airportString = airportString + "," + airports[i];
    }
  }

  BearSSL::WiFiClientSecure client;
  client.setInsecure();
  Serial.println("\nStarting connection to server...");
  // if you get a connection, report back via serial:
  if (!client.connect(SERVER, 443)) {
    Serial.println("Connection failed!");
    client.stop();
    return false;
  } else {
    Serial.println("Connected ...");
    Serial.print("GET ");
    Serial.print(BASE_URI);
    Serial.print(airportString);
    Serial.println(" HTTP/1.1");
    Serial.print("Host: ");
    Serial.println(SERVER);
    Serial.println("User-Agent: LED Map Client");
    Serial.println("Connection: close");
    Serial.println();
    // Make a HTTP request, and print it to console:
    client.print("GET ");
    client.print(BASE_URI);
    client.print(airportString);
    client.println(" HTTP/1.1");
    client.print("Host: ");
    client.println(SERVER);
    client.println("User-Agent: LED Sectional Client");
    client.println("Connection: close");
    client.println();
    client.flush();
    t = millis(); // start time
    FastLED.clear();

    Serial.print("Getting data");

    while (!client.connected()) {
      if ((millis() - t) >= (READ_TIMEOUT * 1000)) {
        Serial.println("---Timeout---");
        client.stop();
        return false;
      }
      Serial.print(".");
      delay(1000);
    }

    Serial.println();

    while (readSingleMetar(&client)) {
      // Nothing
    }
  }

  // Do the key LEDs now if they exist
  for (int i = 0; i < airports.size(); i++) {
    // Use this opportunity to set colors for LEDs in our key then build the request string
    if (airports[i] == "VFR") doColor(airports[i], i, WX_CATEGORY_VFR);
    else if (airports[i] == "WVFR") doColor(airports[i], i, WX_CATEGORY_VFR | WX_FLAG_WINDY);
    else if (airports[i] == "MVFR") doColor(airports[i], i, WX_CATEGORY_MVFR);
    else if (airports[i] == "IFR") doColor(airports[i], i, WX_CATEGORY_IFR);
    else if (airports[i] == "LIFR") doColor(airports[i], i, WX_CATEGORY_LIFR);
  }

  client.stop();
  return true;
}

void doColor(String identifier, unsigned short int led, int condition) {
  CRGB color = CRGB::Black;
  switch (condition & WX_CATEGORY_MASK) {
    case WX_CATEGORY_LIFR: color = CRGB::Magenta; break;
    case WX_CATEGORY_IFR: color = CRGB::Red; break;
    case WX_CATEGORY_MVFR: color = CRGB::Blue; break;
    case WX_CATEGORY_VFR: color = CRGB::Green; break;
  }

  if (condition & WX_CATEGORY_MASK == WX_CATEGORY_VFR) {
    bool wind_warning = condition & (WX_FLAG_WINDY | WX_FLAG_GUSTY);
    if (DO_WINDS && wind_warning) {
      color = CRGB::Yellow;
    }
  }

  if (condition & WX_FLAG_TS) lightningLeds.push_back(led);

  leds[led] = color;
}
