/*
 * Send data from ESP8266 to cloud
 * Sketch will wake up ever X minutes, read a value and POST
 * it to a URL.
 * 
 *  Tested OK:
 *    ubidots, IFTTT, your own server...).
 * 
 *  Tested NOT OK:
 *    Adafriuit.IO, requires one parameter per feed ()
 * 
 * The minutes and URL is configurable. You can also specify a header
 * which some cloud providers use as an authentication key.
 */

#include <ESPWebConfig.h> // https://github.com/mnomn/ESPWebConfig
#include <ESPXtra.h> // https://github.com/mnomn/ESPXtra
#include <ArduinoOTA.h>

// #define E2C_TEST_MODE 1

// After boot, connect this pin to GND reset web config or start OTA
int resetPin = 0; // Wemos D3
// Blink led depending on state
int ledPin = 2; // on Wemos D4,  GPIO2

// Define modes and states
// Modes are fixet for the dvice, states change during execution
#define E2C_ADC_MODE_NONE 0
#define E2C_ADC_MODE_BATTERY 1
#define E2C_ADC_MODE_A0 2

#define E2C_TH_MODE_NONE 0
#define E2C_TH_MODE_DTH 1
#define E2C_TH_MODE_I2C 2

#define E2C_STATE_STARTING 0
#define E2C_STATE_RESET_CONFIG 1
#define E2C_STATE_CONFIGURE 2
#define E2C_STATE_MEASURE 3
#define E2C_STATE_POST_DATA 4
#define E2C_STATE_SLEEP 5
#define E2C_STATE_STAY_AWAKE 6
#define E2C_STATE_OTA 7

/**
 == STATES ==
 E2C_STATE_STARTING
 <configured> - N -> E2C_STATE_CONFIGURE
  Y
  <reset pressed(p)> -Y-> E2C_STATE_OTA -> <longpress> Y -> E2C_STATE_RESET_CONFIG
  N
  E2C_STATE_MEASURE
  <p> -Y-> E2C_STATE_OTA -> <longpress> Y -> E2C_STATE_RESET_CONFIG
  N
  E2C_STATE_POST_DATA
  <p> -Y-> E2C_STATE_OTA -> <longpress> Y -> E2C_STATE_RESET_CONFIG
  N
  sleep
*/

#define E2C_ADC_MODE E2C_ADC_MODE_BATTERY
#define E2C_TH_MODE E2C_TH_MODE_I2C

#if E2C_ADC_MODE == E2C_ADC_MODE_BATTERY
  ADC_MODE(ADC_VCC);
#endif

/////// DTH devices //////////////
#if E2C_TH_MODE == E2C_TH_MODE_DTH
#include <DHT.h>
//#define DHTTYPE DHT11   // DHT 11
//#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321
#define E2C_DHTTYPE DHT21 // DHT 21 (AM2301)
//#define DHT_PIN 4   /* Wemos D2. Data pin of sensor */
#define E2C_DHT_DATA_PIN 0 /* Wemos D3. Has pull up on board */
#define E2C_POWER_PIN 5   /* D1 on WeMos */
#endif

#if E2C_TH_MODE == E2C_TH_MODE_I2C
#include <AM2320.h>
#include <Wire.h>
AM2320 th;
#define E2C_POWER_PIN 14 /* D5 on WeMos */
// #define E2C_GND_PIN 12
// Pins on sensor: VCC, SDA, GND, SCL
// Pins on ESP8266: SDA GPIO4 D2 on wemos
//                  SCL GPIO5 D1 on wemos
#endif

/////// I2C devices //////////////

#define SOME_PRINTING 1

const char *URL_KEY = "Url*|url";           // Do not edit. Configure in web UI
const char *HEADER_KEY = "Header";          // Do not edit. Configure in web UI
const char *INTERVAL_KEY = "Interval(min)"; // Do not edit. Configure in web UI

ESP8266WebServer server(80);
String parameters[] = {URL_KEY, HEADER_KEY, INTERVAL_KEY};
ESPWebConfig espConfig(NULL, parameters, 3);
unsigned long lastButtonTime = 0;

int state = -1;
float vcc = -1;
float temp = 0;
float humid = 0;

ESPXtra espx;

void setup()
{
  Serial.begin(74880);
  while (!Serial)
  {
    delay(1);
  }

  // If register says sleep we will not come back.
  espx.SleepCheck();
  yield();

#if E2C_TH_MODE == E2C_TH_MODE_I2C
  // Set SDA and SCL HIGH, so no external pull-ups are needed
  pinMode(SDA, INPUT_PULLUP);
  pinMode(SCL, INPUT_PULLUP);
#endif

#ifdef E2C_POWER_PIN
  pinMode(E2C_POWER_PIN, OUTPUT);
  digitalWrite(E2C_POWER_PIN, HIGH);
#endif
#ifdef E2C_GND_PIN
  pinMode(E2C_GND_PIN, OUTPUT);
  digitalWrite(E2C_GND_PIN, LOW);
#endif
  yield();
  Serial.print("Power pin ");
  Serial.println(E2C_POWER_PIN);

#if E2C_TH_MODE == E2C_TH_MODE_DTH
  DHT dht(E2C_DHT_DATA_PIN, E2C_DHTTYPE);
  yield();
  dht.begin();
  Serial.println(F("Using DTH)"));
#endif

#if E2C_TH_MODE == E2C_TH_MODE_I2C
  Wire.begin();
  Serial.println("Using I2C");
#endif

  // espWebConfig is configuring the button as input
  espConfig.setHelpText((char*) "Set URL and (optional) header, for example <br>"
                        "URL=<b>http://things.ubidots.com/api/v1.6/devices/{LABEL_DEVICE}/</b> <br>"
                        "Header=<b>X-Auth-Token: xxxx</b>");
  if (espConfig.setup(server))
  {
    Serial.println(F("E2C_STATE_STARTING"));
    state = E2C_STATE_STARTING;
  }
  else
  {
    Serial.println(F("E2C_STATE_CONFIGURE"));
    state = E2C_STATE_CONFIGURE;
  }
  Serial.println(WiFi.localIP());
  otaSetupCallbacks();
  server.begin();
}

void loop()
{
  server.handleClient();
  int press = espx.ButtonPressed(resetPin);
  // Long press: Reset config
  if (press == 2 && state != E2C_STATE_RESET_CONFIG) {
    espConfig.clearConfig();
    state = E2C_STATE_RESET_CONFIG;
    Serial.println(F("E2C_STATE_RESET_CONFIG"));
  }
  // Short press: OTA
  if (press == 1 && state != E2C_STATE_OTA  && state != E2C_STATE_RESET_CONFIG) {
    Serial.println("Begin OTA");
    ArduinoOTA.begin();
    pinMode(ledPin, OUTPUT);
    state = E2C_STATE_OTA;
    Serial.println(F("E2C_STATE_OTA"));
  }

  if (state == E2C_STATE_OTA) {
    digitalWrite(ledPin, (millis()/200)%2==1?HIGH:LOW);
    ArduinoOTA.handle();
    return;
  }
  if (state == E2C_STATE_RESET_CONFIG) {
    digitalWrite(ledPin, LOW);
    // Wait for user to restart
    return;
  }

  if (state == E2C_STATE_STARTING)
  {
    state = E2C_STATE_MEASURE;
    Serial.println(F("E2C_STATE_MEASURE"));
    #if E2C_ADC_MODE != E2C_ADC_MODE_NONE
    measureAnalog();
    #endif
    yield();
    #if E2C_TH_MODE == E2C_TH_MODE_DTH
    measureDTH();
    #else
    measureI2C();
    #endif
    #ifdef E2C_POWER_PIN
    digitalWrite(E2C_POWER_PIN, LOW);
    #endif
    return;
  }

  if(state == E2C_STATE_MEASURE) {
    state = E2C_STATE_POST_DATA;
    Serial.println(F("E2C_STATE_POST_DATA"));
    postToCloud(vcc, temp, humid);
    return;
  }

  if(state == E2C_STATE_POST_DATA) {
    state = E2C_STATE_SLEEP;
    Serial.println(F("E2C_STATE_SLEEP"));
    // Calculate sleep
    char *sleepMinStr = espConfig.getParameter(INTERVAL_KEY);
    Serial.print("SleepMin ");
    Serial.println(sleepMinStr);
    uint32_t sleepMin = 60; // Default 60 min sleep
    if (sleepMinStr && *sleepMinStr)
    {
      sleepMin = atoi(sleepMinStr);
    }
#ifdef E2C_TEST_MODE
    Serial.print("Test mode, no sleep");
   return;
#else
      espx.SleepSetMinutes(sleepMin);
#endif
  }

  // for testing, keep device awake
  if (state == E2C_STATE_SLEEP) {
    state = E2C_STATE_STAY_AWAKE;
    Serial.println(F("E2C_STATE_SLEEP (test mode)"));
  }

} // loop

void measureAnalog()
{
  #if E2C_ADC_MODE == E2C_ADC_MODE_BATTERY
    vcc = ESP.getVcc();
    Serial.print(" Battery ");
  #elif E2C_ADC_MODE == E2C_ADC_MODE_A0
    vcc = analogRead(A0);
    Serial.print(" Analog: ");
  #endif
    Serial.println(vcc);
}

/** Set global temp and humid variables
*/
#if E2C_TH_MODE == E2C_TH_MODE_DTH
void measureDTH()
{
  // If TH mode is not DTH, do nothing
  temp = dht.readTemperature();
  yield(); // Yield to Wifi
  humid = dht.readHumidity();
  int retry = 3;
  while (retry-- && (isnan(temp) || isnan(humid)))
  {
    delay(1000);
    if (isnan(temp))
    {
      Serial.println("T is nan");
      temp = dht.readTemperature();
    }
    if (isnan(humid))
    {
      Serial.println("H is nan");
      humid = dht.readHumidity();
    }
  }
}
#endif

/** Set global temp and humid variables
*/
void measureI2C()
{
  int th_err = th.Read();
  Serial.print(" TH Read ");
  Serial.println(th_err);

  if (th_err == 0) {
    temp = th.t;
    humid = th.h;
    Serial.print(" humidity: ");
    Serial.print(th.h);
    Serial.print("%, temperature: ");
    Serial.print(th.t);
    Serial.println("*C");
  }

}

void postToCloud(float v1, float t, float h)
{
  String Url = espConfig.getParameter(URL_KEY);
  String Header = espConfig.getParameter(HEADER_KEY);
  if (Header) {
    Header.trim();
  }

  String host;
  String path;

  WiFiClient client;
  // Remove schema:// from url
  int ix = Url.indexOf(String("://"));
  if (ix >= 0)
  {
    Url = Url.substring(ix + 3);
  }

  ix = Url.indexOf('/');
  if (ix > 0)
  {
    host = Url.substring(0, ix);
    path = Url.substring(ix);
  }
  else
  {
    host = Url;
    path = "/";
  }

  Serial.println(F("Connect to host"));
  if (client.connect(host.c_str(), 80))
  {

    String postData = "{\"value1\":";
    postData += v1;
    if (!(t == 0 && h == 0))
    {
      // If both t and h are 0, they are disabled.
      if (!isnan(t))
      {
        postData += ", \"value2\":\"";
        postData += t;
        postData += "\"";
      }
      if (!isnan(h))
      {
        postData += ", \"value3\":\"";
        postData += h;
        postData += "\"";
      }
    }
    postData += "}";

    String httpData = "POST ";
    httpData += path;
    httpData += " HTTP/1.1\r\nHost: ";
    httpData += host;
    httpData += "\r\nContent-Type: application/json\r\n";
    if (Header.length() > 0)
    {
      Header.trim();
      httpData += Header;
      httpData += "\r\n";
    }
    httpData += "Content-Length:";
    httpData += postData.length();
    httpData += "\r\nConnection: close\r\n\r\n"; // End of header
    httpData += postData;

    Serial.println(F("Sending a request"));
    Serial.println(httpData);
    client.println(httpData);

    Serial.println(F("Read response"));
    // Read response
    int max_lines = 15;
    while (client.connected())
    {
      if (client.available())
      {
        String line = client.readStringUntil('\n');
        if (max_lines-- > 0) {
          Serial.println(line.substring(0,1024));
        }
      }
    }
    Serial.println(F("Done!"));
    client.stop();
  }
}

void otaSetupCallbacks()
{
  ArduinoOTA.onStart(onOTAStart);
  ArduinoOTA.onError(onOTAError);
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
}

void onOTAStart()
{
  Serial.println(F("OTA Start"));
}

void onOTAError(ota_error_t error)
{
  Serial.print(F("OTA Error"));
  Serial.println(error);
}
