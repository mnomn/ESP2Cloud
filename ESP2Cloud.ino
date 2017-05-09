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
#include <ESPCoolStuff.h> // https://github.com/mnomn/ESPCoolStuff
#include "DHT.h"

// Connect this pin to GND to reset web config
int resetPin = 4; // Called D2 on Wemos

#define USE_DHT
#ifdef USE_DHT
//#define DHTTYPE DHT11   // DHT 11
//#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321
#define DHTTYPE DHT21 // DHT 21 (AM2301)
#define POWER_PIN 5   /* WeMos D1. Used to power sensor only when device is on */
//#define DHT_PIN 4   /* Wemos D2. Data pin of sensor */
#define DHT_DATA_PIN 0 /* Wemos D3. Has pull up on board */
#endif

//////////////////////////////////////
// No need to edit below /////////////
// To measure battery levl
ADC_MODE(ADC_VCC);

const char *URL_KEY = "Url*|url";           // Do not edit. Configure in web UI
const char *HEADER_KEY = "Header";          // Do not edit. Configure in web UI
const char *INTERVAL_KEY = "Interval(min)"; // Do not edit. Configure in web UI

ESP8266WebServer server(80);
String parameters[] = {URL_KEY, HEADER_KEY, INTERVAL_KEY};
ESPWebConfig espConfig(NULL, parameters, 3);
unsigned long lastButtonTime = 0;
#define MODE_CALLING 0
#define MODE_RESETTING 1
#define MODE_CONFIG 2
#define MODE_CALL_COMPLETED 2
int mode = -1;
float temp = 0;
float humid = 0;

ESPCoolStuff espStuff;

void postToCloud(int v1, float t, float h)
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

void setup()
{
  Serial.begin(115200);
  while (!Serial)
  {
    delay(1);
  }

  pinMode(resetPin, INPUT_PULLUP);
  if (HIGH == digitalRead(resetPin)) {
    espStuff.SleepCheck();
  }

#ifdef USE_DHT
  pinMode(POWER_PIN, OUTPUT);    // sets the digital pin as output
  digitalWrite(POWER_PIN, HIGH); // sets the LED on
  delay(10);                     // delay(yield) now and then to be nuce to the wofo stack.
  DHT dht(DHT_DATA_PIN, DHTTYPE);
  delay(10);
  dht.begin();
#endif

  // espWebConfig is configuring the button as input
  Serial.println(F("Starting ..."));
  espConfig.setHelpText("Set URL and (optional) header, for example <br>"
                        "URL=<b>http://things.ubidots.com/api/v1.6/devices/{LABEL_DEVICE}/</b> <br>"
                        "Header=<b>X-Auth-Token: xxxx</b>");
  if (espConfig.setup(server))
  {
    Serial.println(F("Normal boot"));
    Serial.println(WiFi.localIP());
    mode = MODE_CALLING;
#ifdef USE_DHT
    temp = dht.readTemperature();
    delay(10); // Yeild to Wifi
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
#endif
  }
  else
  {
    Serial.println(F("Config mode"));
    mode = MODE_CONFIG;
  }
  server.begin();
}

void loop()
{
  server.handleClient();

  if (LOW == digitalRead(resetPin))
  {
    espConfig.clearConfig();
    mode = MODE_RESETTING;
    ESP.restart();
  }

  if (mode == MODE_CALLING)
  {
    // Calculate sleep
    char *sleepMinStr = espConfig.getParameter(INTERVAL_KEY);
    Serial.print("SleepMin ");
    Serial.println(sleepMinStr);
    uint32_t sleepMin = 60; // Default 60 min sleep
    if (sleepMinStr)
    {
      sleepMin = atoi(sleepMinStr);
    }

    delay(10);
    uint16_t vcc = ESP.getVcc();
    delay(10);

    mode = MODE_CALL_COMPLETED;
#ifdef USE_DHT
    // Read temperature as Celsius (the default)
    digitalWrite(POWER_PIN, LOW); // sets the LED on
#endif
    postToCloud(vcc, temp, humid);
    delay(10);
    espStuff.SleepSetMinutes(sleepMin);
  }
}
