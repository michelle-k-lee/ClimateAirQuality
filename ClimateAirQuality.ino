/*
  Air Quality and Pollution Particulate Tracking,
  Comparisons Between Current and Past Temperature Data,
  Information about Climate Change (Temperature Change) Predictions by 2040.
*/

// Loads libraries
#include <ESP8266WiFi.h>    
#include <ESP8266HTTPClient.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Adafruit_MPL115A2.h>

//****** These libraries are for the OLED Display
#include <SPI.h> // Loads SPI (Serial Peripheral Interface) library; synchronous serial data protocol used by microcontrollers
#include <Wire.h> // Loads Wire library; allows communication with I2C/TWI devices
#include <Adafruit_GFX.h> // Loads Adafruit_GFX library, for Adafruit LCD and OLEDs
#include <Adafruit_SSD1306.h> // Loads Adafruit_SSD1306 library, for monochrome 128x64 and 128x32 OLEDs

//****** This is for the OLED Display
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels

//****** This is for the OLED Display
// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#include "config.h" // include the configuration of credentials in the config.h file

#define MQTT_ENABLE 1  // for testing the sensor, set to 1 to enable posting data to the server

// Create objects
WiFiClient espClient;
PubSubClient mqtt(espClient);
Adafruit_MPL115A2 mpl115a2;
StaticJsonDocument<256> outputDoc; // Specifies JSON document that we'll be outputting to for MQTT
StaticJsonDocument<256> outputDoc2; // Specifies JSON document that we'll be outputting to for MQTT
StaticJsonDocument<256> outputDoc3; // Specifies JSON document that we'll be outputting to for MQTT
StaticJsonDocument<5000> doc; // Specifies JSON document with byte size

typedef struct { // Create a struct to hold data
  String aqi;     // For each name:value pair coming in from the service, create a slot in our structure to hold our data
  String ozone;
  String sulfurdiox;
  String nitrodiox;
} CurrAirQualityData;     // This gives the data structure a name, CurrAirQualityData

CurrAirQualityData currairqual; // The CurrAirQualityData type has been created, but not an instance of that type, so this creates variable "currairqual" of type CurrAirQualityData

typedef struct { // Create a struct to hold data
  String bestcase;     // For each name:value pair coming in from the service, create a slot in our structure to hold our data
  String worstcase;
} ClimateData;     // This gives the data structure a name, ClimateData

ClimateData climate; // The ClimateData type has been created, but not an instance of that type, so this creates variable "climate" of type ClimateData

typedef struct { // Create a struct to hold data
  String thetimenow;     // For each name:value pair coming in from the service, create a slot in our structure to hold our data
} TimeData;     // This gives the data structure a name, TimeData

TimeData thetime; // The TimeData type has been created, but not an instance of that type, so this creates variable "thetime" of type TimeData

typedef struct { // Create a struct to hold data
  String histtempmin;     // For each name:value pair coming in from the service, create a slot in our structure to hold our data
  String histtempmax;
} HistoricalData;     // This gives the data structure a name, HistoricalData

HistoricalData historical; // The HistoricalData type has been created, but not an instance of that type, so this creates variable "historical" of type HistoricalData

char mac[6]; //A MAC address is a 'truly' unique ID for each device, lets use that as our 'truly' unique user ID!!!
char buffer1[256];   // stores the data for the JSON document
char buffer2[256];   // stores the data for the JSON document
char buffer3[256];   // stores the data for the JSON document

unsigned long timer;  // a timer for the MQTT server; we don't want to flood it with data

int theAQI = 0; // Declares integer variable theAQI (air quality index value)

String rightNow = ""; // Declares string variable rightNow (for the current date)

int delayTime = 1000;  // Define delayTime to be 1 second

int motionSensor = 16;  // Define motionSensor (PIR motion sensor) pin as pin 16

float celsiusbest = 0; // Variable for climate change temp change prediction, best case in C
float celsiusworst = 0;  // Variable for climate change temp change prediction, worst case in C
float fahrenheitbest = (celsiusbest * 1.8); // Variable for climate change temp change prediction, best case in F
float fahrenheitworst = (celsiusworst * 1.8); // Variable for climate change temp change prediction, worst case in F

float temperatureC = 0; // For current temp in C
float temperatureF = 0; // For current temp in F

float historicalminF = 0; // For historical min temp in F
float historicalmaxF = 0; // For historical max temp in F

String theyear = "";

int redPin = 14; // Define red RGB LED pin as pin 14
int greenPin = 12; // Define green RGB LED pin as pin 12
int bluePin = 13; // Define blue RGB LED pin as pin 13

void setup() {
  pinMode(motionSensor, INPUT); // Declare PIR motion sensor as an input

  Serial.begin(115200);
  Serial.println();
  Serial.print("This board is running: ");
  Serial.println(F(__FILE__));

  mpl115a2.begin();

  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3C for 128x32
    Serial.println(F("SSD1306 allocation failed"));
    for (;;); // Don't proceed, loop forever
  }
  // set up the OLED display
  // by default, we'll generate the high voltage from the 3.3v line internally
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3C (for the 128x32)
  display.setTextSize(1);                     // Normal 1:1 pixel scale
  display.setTextColor(SSD1306_WHITE);        // Draw white text
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Started up.");
  delay(2000); // Delay of 2 seconds

  if (MQTT_ENABLE) { // If MQTT posting is enabled
    setup_wifi(); // Connect to WiFi
    mqtt.setServer(mqtt_server, 1883); // Uses the MQTT server address defined in the config file, port 1883 (not secure, sends in plain text)
    //mqtt.setCallback(callback); // Sets the callback function so we can receive MQTT messages
  }

  timer = millis(); // start the timer

  // When testing put API calls in setup, not loop... learned this the hard way and got locked out of the Weatherbit API for the day
  // So I placed this here
  // However, if I had plenty of API calls
  // and wanted to get lots of updates throughout the day then I would comment them out here
  // and uncomment them below in the loop and set more of a delay within the loop
  getClimate(); // Calls getClimate function
  getCurrAirQuality(); // Calls getCurrAirQuality function
  getTheTime(); // Calls the getTheTime function
  getHistorical(); // Calls the getHistorical function
  checkSensor();    // Check the temp sensor

  pinMode(redPin, OUTPUT);
  pinMode(greenPin, OUTPUT);
  pinMode(bluePin, OUTPUT);
}

void loop() {
  if (millis() - timer > 60000) { //see if 1 min has elapsed since last message
    long state = digitalRead(motionSensor);
    if (state == HIGH) {
      Serial.println("Motion detected!");

      // This part is relevant to the OLED Display
      display.clearDisplay(); // Clears the display
      display.setCursor(0, 0); // Tells the cursor where to start, at the top-left corner
      display.print("Current Temp: "); // Prints "Temp: " on the OLED display
      display.print(temperatureF); // Prints the temp in F on the OLED display
      display.println(" F");// Prints " F" plus a carriage return and newline on the OLED display
      display.print("1 Yr Ago Min: "); // Prints "Historical Temp Min: " on the OLED display
      display.print(historicalminF); // Prints the temp in F on the OLED display
      display.println(" F");// Prints " F" plus a carriage return and newline on the OLED display
      display.print("1 Yr Ago Max: "); // Prints "Historical Temp Min: " on the OLED display
      display.print(historicalmaxF); // Prints the temp in F on the OLED display
      display.println(" F");// Prints " F" plus a carriage return and newline on the OLED display
      display.display();

      if (theAQI < 51) {
        // Air Quality is good
        Serial.println("Green: Air quality is good!"); // Print in serial monitor and go to new line
        setColor(80, 0, 80);  // Sets LED color to green
        delay(delayTime); // Delay of 1 second
      }
      else if (theAQI > 51 <= 100) {
        // Air Quality is moderate
        Serial.println("Yellow: Air quality is moderate"); // Print in serial monitor and go to new line
        setColor(0, 0, 255);  // Sets LED color to yellow
        delay(delayTime); // Delay of 1 second
      }
      else if (theAQI > 101 <= 150) {
        // Air Quality is unhealthy for sensitive groups
        Serial.println("Purple: Air quality"); // Print in serial monitor and go to new line
        setColor(0, 255, 0);  // Sets LED color to purple
        delay(delayTime); // Delay of 1 second
      }
      else if (theAQI > 151) {
        // Air Quality is unhealthy
        Serial.println("Red: Air quality "); // Print in serial monitor and go to new line
        setColor(0, 255, 255);  // Sets LED color to red
        delay(delayTime); // Delay of 1 second
      }

      // Introduce a delay of 10 seconds
      delay(10000);
    }
    else {
      Serial.println("No motion detected.");
      delay(10000);
    }

    outputDoc["CurrentTemperature_F"] = temperatureF; // Outputs "CurrentTemperature_F" and the current temp to the MQTT server
    outputDoc["HistoricalMinTemperature_F"] = historicalminF; // Outputs the historical temp min in F to the MQTT server
    outputDoc["HistoricalMaxTemperature_F"] = historicalmaxF; // Outputs the historical temp max in F to the MQTT server
    serializeJson(outputDoc, buffer1);
    outputDoc2["AQI"] = theAQI; // Outputs "AQI" and the current Air Quality Index to the MQTT server
    serializeJson(outputDoc2, buffer2);
    outputDoc3["BestCaseScenario"] = fahrenheitbest; // Outputs "BestCaseScenario" and the best case predicted change temp to the MQTT server
    outputDoc3["WorstCaseScenario"] = fahrenheitworst; // Outputs "WorstCaseScenario" and the worst case predicted change temp to the MQTT server
    serializeJson(outputDoc3, buffer3);

    timer = millis(); // reset a 5-minute timer

    if (MQTT_ENABLE) {  // if posting is turned on, post the data
      if (!mqtt.connected()) { // If not connected, then...
        reconnect(); // Reconnect
      }
      mqtt.publish(feed1, buffer1);  // post the data to feed1, as defined in the config file
      Serial.println("Posted:"); // Prints "Posted:" and starts next printing on a new line
      serializeJsonPretty(outputDoc, Serial);
      Serial.println(); // Starts on new line of serial monitor

      mqtt.publish(feed2, buffer2);  // post the data to feed2, as defined in the config file
      Serial.println("Posted:"); // Prints "Posted:" and starts next printing on a new line
      serializeJsonPretty(outputDoc2, Serial);
      Serial.println(); // Starts on new line of serial monitor

      mqtt.publish(feed3, buffer3);  // post the data to feed2, as defined in the config file
      Serial.println("Posted:"); // Prints "Posted:" and starts next printing on a new line
      serializeJsonPretty(outputDoc3, Serial);
      Serial.println(); // Starts on new line of serial monitor

      mqtt.loop(); //this keeps the mqtt connection 'active'
    }

    //getCurrAirQuality(); // Calls the getCurrAirQuality function

    Serial.print("The date and time is: "); // Prints "The date and time is: " in serial monitor
    Serial.println(F(__DATE__ " " __TIME__)); // Prints the date and time and goes to new line
    Serial.println(); // Starts on new line of serial monitor
    Serial.println("--- Air Quality Info ---"); //
    Serial.println("Here's the current air quality (AQI): " + currairqual.aqi); // Prints current air quality in serial monitor and goes to new line
    Serial.println("The following three measurements are in micrograms per cubic meter: "); // Prints in serial monitor
    Serial.println("The ground-level ozone concentration is: " + currairqual.ozone); // Prints ozone info in serial monitor and goes to new line
    Serial.println("The surface nitrogen dioxide concentration is: " + currairqual.nitrodiox); // Prints nitrogen dioxide info in serial monitor and goes to new line
    Serial.println("The surface sulfur dioxide concentration is: " + currairqual.sulfurdiox); // Prints sulfur dioxide info in serial monitor and goes to new line
    Serial.println(); // Starts on new line of serial monitor

    //getClimate(); // Calls the getClimate function

    Serial.println("--- Climate Change Info ---"); // Starts on new line of serial monitor
    Serial.println("The best case predicted temperature increase in Celsius from 2020-2039 is: " + climate.bestcase); // Prints best case predicted temp increase in C in serial monitor and goes to new line
    Serial.println("The worst case predicted temperature increase in Celsius from 2020-2039 is: " + climate.worstcase); // Prints worst case predicted temp increase in C in serial monitor and goes to new line
    Serial.println("The best case predicted temperature increase in F from 2020-2039 is: " + String(fahrenheitbest)); // Prints best case predicted temp increase in F in serial monitor and goes to new line
    Serial.println("The worst case predicted temperature increase in F from 2020-2039 is: " + String(fahrenheitworst)); // Prints worst case predicted temp increase in F in serial monitor and goes to new line
    Serial.println(); // Starts on new line of serial monitor

    //getTheTime(); // Calls the getTheTime function

    Serial.println("--- Date and Time Info ---"); // Starts on new line of serial monitor
    Serial.println("Here's the time and date: " + thetime.thetimenow); // Prints current date and time in serial monitor and goes to new line
    Serial.println("Here's just the date: " + rightNow); // Prints current date in serial monitor and goes to new line
    Serial.println(); // Starts on new line of serial monitor

    //getHistorical(); // Calls the getHistorical function

    Serial.println("--- Historical Temp Info ---"); // Starts on new line of serial monitor
    Serial.print("Here's the historical low in F for 1 year ago: "); // Prints in serial monitor
    Serial.println(historicalminF);
    Serial.print("Here's the historical high in F for 1 year ago: "); // Prints in serial monitor
    Serial.println(historicalmaxF);
    Serial.println(); // Starts on new line of serial monitor

    delay(5000); // 5 second delay between requests
  }
}

void setColor(int red, int green, int blue) { // Function for setting the color value for each pin of the RGB LED
  digitalWrite(redPin, red);
  digitalWrite(greenPin, green);
  digitalWrite(bluePin, blue);
}

void checkSensor() {
  Serial.println("------------------------------"); // Prints "------------------------------" and starts next printing on a new line
  temperatureC = mpl115a2.getTemperature(); // Variable temperatureC has value of the temp reading from the sensor
  Serial.print("Temp (*C): "); Serial.print(temperatureC, 1); Serial.println(" *C");
  // Prints "Temp (*C): " + [the temperature reading value] + " *C" in the serial monitor

  temperatureF = (temperatureC * 1.8) + 32;
  Serial.print(temperatureF); // Print current temp in F in serial monitor
  Serial.println(" *F currently"); // Print in serial monitor
}

void getCurrAirQuality() { // Defines function getCurrAirQuality
  HTTPClient currairqualClient;
  Serial.println("Making HTTP request"); // Prints "Making HTTP request" in serial monitor and goes to new line
  currairqualClient.begin("http://api.weatherbit.io/v2.0/current/airquality?&city=Seattle,WA&key=APIkey");  // Connects to Weatherbit API

  int httpCode = currairqualClient.GET();

  Serial.println(httpCode);

  if (httpCode > 0) { // If the HTTP response status code is greater than 0, then...
    if (httpCode == 200) { // If the HTTP response status code is 200 (which is a successful status code), then
      Serial.println("Received HTTP payload."); // Prints "Received HTTP payload." in serial monitor and goes to new line
      //   alternatively use:  DynamicJsonDocument doc(1024); // specify JSON document and size(1024)
      //StaticJsonDocument<1024> doc; // Specifies JSON document with 1024 byte size
      String payload = currairqualClient.getString();
      Serial.println("Parsing..."); // Prints "Parsing..." in serial monitor and goes to new line
      deserializeJson(doc, payload);

      DeserializationError error = deserializeJson(doc, payload);
      // Test if parsing succeeds.
      if (error) { // If there's an error...
        Serial.print("deserializeJson() failed with error code "); // Prints "deserializeJson() failed with error code " in serial monitor
        Serial.println(error.c_str());
        Serial.println(payload); // Prints the payload (data that was requested from the API)
        return;
      }

      // Values as Strings
      currairqual.aqi = doc["data"][0]["aqi"].as<String>(); // Current AQI
      currairqual.ozone = doc["data"][0]["o3"].as<String>(); // Current ground-level ozone levels
      currairqual.nitrodiox = doc["data"][0]["no2"].as<String>(); // Current ground-level nitrogen dioxide levels
      currairqual.sulfurdiox = doc["data"][0]["so2"].as<String>(); // Current ground-level sulfur dioxide levels

      theAQI = currairqual.aqi.toInt(); // Makes value of theAQI what currairqual.aqi is (current air quality from API) as an integer from a string
    } else {
      Serial.println("Something went wrong with connecting to the endpoint."); // Prints "Something went wrong with connecting to the endpoint." in serial monitor and goes to new line
    }
  }
}

void getClimate() { // Defines function getClimate
  HTTPClient climateClient;
  Serial.println("Making HTTP request"); // Prints "Making HTTP request" in serial monitor and goes to new line
  climateClient.begin("http://climatedataapi.worldbank.org/climateweb/rest/v1/country/annualanom/tas/2020/2039/USA");  // Connects to The World Bank API

  int httpCode = climateClient.GET();

  Serial.println(httpCode);

  if (httpCode > 0) { // If the HTTP response status code is greater than 0, then...
    if (httpCode == 200) { // If the HTTP response status code is 200 (which is a successful status code), then
      Serial.println("Received HTTP payload."); // Prints "Received HTTP payload." in serial monitor and goes to new line
      //   alternatively use:  DynamicJsonDocument doc(1024); // specify JSON document and size(1024)
      //StaticJsonDocument<5000> doc; // Specifies JSON document with 5000 byte size
      String payload = climateClient.getString();
      Serial.println("Parsing..."); // Prints "Parsing..." in serial monitor and goes to new line
      deserializeJson(doc, payload);

      DeserializationError error = deserializeJson(doc, payload);
      // Test if parsing succeeds.
      if (error) { // If there's an error...
        Serial.print("deserializeJson() failed with error code "); // Prints "deserializeJson() failed with error code " in serial monitor
        Serial.println(error.c_str());
        Serial.println(payload); // Prints the payload (data that was requested from the API)
        return;
      }

      // Values as Strings
      climate.bestcase = doc[0]["annualData"][0].as<String>(); // Projected best case scenario prediction of temp increase in C
      climate.worstcase = doc[27]["annualData"][0].as<String>(); // Projected worst case scenario prediction of temp increase in C

      celsiusbest = climate.bestcase.toFloat(); // Convert to float and have variable value change
      celsiusworst = climate.worstcase.toFloat(); // Convert to float and have variable value change
      fahrenheitbest = (celsiusbest * 1.8); // calc for Fahrenheit (best case scenario projection) and change variable value
      fahrenheitworst = (celsiusworst * 1.8); // calc for Fahrenheit (worst case scenario projection) and change variable value

    } else {
      Serial.println("Something went wrong with connecting to the endpoint."); // Prints "Something went wrong with connecting to the endpoint." in serial monitor and goes to new line
    }
  }
}

void getTheTime() { // Defines function getTheTime
  HTTPClient currTimeClient;
  Serial.println("Making HTTP request"); // Prints "Making HTTP request" in serial monitor and goes to new line
  currTimeClient.begin("http://worldclockapi.com/api/json/pst/now");  // Connects to time and date API

  int httpCode = currTimeClient.GET();

  Serial.println(httpCode);

  if (httpCode > 0) { // If the HTTP response status code is greater than 0, then...
    if (httpCode == 200) { // If the HTTP response status code is 200 (which is a successful status code), then
      Serial.println("Received HTTP payload."); // Prints "Received HTTP payload." in serial monitor and goes to new line
      //   alternatively use:  DynamicJsonDocument doc(1024); // specify JSON document and size(1024)
      //StaticJsonDocument<1024> doc; // Specifies JSON document with 1024 byte size
      String payload = currTimeClient.getString();
      Serial.println("Parsing..."); // Prints "Parsing..." in serial monitor and goes to new line
      deserializeJson(doc, payload);

      DeserializationError error = deserializeJson(doc, payload);
      // Test if parsing succeeds.
      if (error) { // If there's an error...
        Serial.print("deserializeJson() failed with error code "); // Prints "deserializeJson() failed with error code " in serial monitor
        Serial.println(error.c_str());
        Serial.println(payload); // Prints the payload (data that was requested from the API)
        return;
      }

      // Values as Strings
      thetime.thetimenow = doc["currentDateTime"].as<String>(); // Current date and time

      rightNow = thetime.thetimenow; // Makes value of variable rightNow what thetime.thetimenow is
      rightNow.remove(10); // Only keep the first 10 characters of variable rightNow to keep just the date
      //theyear =
    } else {
      Serial.println("Something went wrong with connecting to the endpoint."); // Prints "Something went wrong with connecting to the endpoint." in serial monitor and goes to new line
    }
  }
}

void getHistorical() { // Defines function getTheTime
  HTTPClient historicalClient;
  Serial.println("Making HTTP request"); // Prints "Making HTTP request" in serial monitor and goes to new line
  historicalClient.begin("http://api.weatherbit.io/v2.0/history/daily?postal_code=98004&country=US&start_date=2019-08-21&end_date=2019-08-22&key=APIkey&units=I");  // Connects to Weatherbit API for historical daily temp
  // I tried a bunch of things involving the current time from the World Clock API and inserting the date for the start date and end date
  // into the URL for the historical temp request as ("http://thefirstpartofURL.com/" + thevariable + "thenextpartsofURL" + othervariable + "otherURLparts"),
  // using some string to int, with subtractions and additions, then back to string conversions
  // but I couldn't get it quite right and it was taking a ton of time,
  // so I made the request statically but would want to be able to live-update this to make this dynamically generated
  // A future feature!

  int httpCode = historicalClient.GET();

  Serial.println(httpCode);

  if (httpCode > 0) { // If the HTTP response status code is greater than 0, then...
    if (httpCode == 200) { // If the HTTP response status code is 200 (which is a successful status code), then
      Serial.println("Received HTTP payload."); // Prints "Received HTTP payload." in serial monitor and goes to new line
      //   alternatively use:  DynamicJsonDocument doc(1024); // specify JSON document and size(1024)
      //StaticJsonDocument<1024> doc; // Specifies JSON document with 1024 byte size
      String payload = historicalClient.getString();
      Serial.println("Parsing..."); // Prints "Parsing..." in serial monitor and goes to new line
      deserializeJson(doc, payload);

      DeserializationError error = deserializeJson(doc, payload);
      // Test if parsing succeeds.
      if (error) { // If there's an error...
        Serial.print("deserializeJson() failed with error code "); // Prints "deserializeJson() failed with error code " in serial monitor
        Serial.println(error.c_str());
        Serial.println(payload); // Prints the payload (data that was requested from the API)
        return;
      }

      // Values as Strings because the "slots" in WeatherData are strings
      historical.histtempmin = doc["data"][0]["min_temp"].as<String>(); // Historical temperature min
      historical.histtempmax = doc["data"][0]["max_temp"].as<String>(); // Historical temperature max

      historicalminF = historical.histtempmin.toFloat(); // Convert to float and have value of variable historicalminF value of thetime.thetimenow
      historicalmaxF = historical.histtempmax.toFloat(); // Convert to float and have value of variable historicalmaxF value of thetime.thetimenow

    } else {
      Serial.println("Something went wrong with connecting to the endpoint."); // Prints "Something went wrong with connecting to the endpoint." in serial monitor and goes to new line
    }
  }
}

/////SETUP_WIFI/////
void setup_wifi() {
  delay(10); // Delay of 10 ms
  // We start by connecting to a WiFi network
  Serial.println(); // Starts on new line in serial monitor
  Serial.print("Connecting to "); // Prints "Connecting to " in serial monitor
  Serial.println(wifi_ssid); // Prints WiFi network name in serial monitor and goes to new line
  WiFi.begin(wifi_ssid, wifi_password); // Uses WiFi network name and password to connect
  while (WiFi.status() != WL_CONNECTED) { // While WiFi isn't connected...
    delay(500); // Wait .5 seconds
    Serial.print("."); // Print "."
  }
  Serial.println(""); // Prints a blank line and starts on a new line in serial monitor
  Serial.println("WiFi connected."); // Prints "WiFi connected." in serial monitor
  String temp = WiFi.macAddress();  //get the unique MAC address to use as MQTT client ID, a 'truly' unique ID.
  temp.toCharArray(mac, 6);         //.macAddress() returns a byte array 6 bytes representing the MAC address
}                                     //5C:CF:7F:F0:B0:C1 for example

/////CONNECT/RECONNECT///// Monitor the connection to MQTT server, if down, reconnect
void reconnect() {
  // Loop until we're reconnected
  while (!mqtt.connected()) { // While not connected,
    Serial.print("Attempting MQTT connection..."); // Prints "Attempting MQTT connection..." in serial monitor
    if (mqtt.connect(mac, mqtt_user, mqtt_password)) { //<<---using MAC as client ID, always unique!!!
      Serial.println("Connected!"); // Prints "Connected!" in serial monitor and goes to new line

      /// Subscribe to feed!
      //mqtt.subscribe(feed1); // feed specified in config.h
      //mqtt.subscribe(feed2); // feed specified in config.h
      //mqtt.subscribe(feed3); // feed specified in config.h

    } else {
      Serial.print("failed, rc="); // Prints "failed, rc=" in serial monitor
      Serial.print(mqtt.state()); // Prints the MQTT state in serial monitor
      Serial.println(" try again in 5 seconds"); // Prints " try again in 5 seconds" in serial monitor and goes to new line
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}
