/*******************************************************************
    A project to display crypto currency prices using an ESP8266

    Main Hardware:
    - NodeMCU Development Board (Any ESP8266 dev board will work)
    - OLED I2C Display (SH1106)

    Written by Brian Lough
    https://www.youtube.com/channel/UCezJOfu7OtqGzd5xrP3q6WA

    Modified by gn
 *******************************************************************/

// ----------------------------
// Standard Libraries - Already Installed if you have ESP8266 set up
// ----------------------------

#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <Wire.h>

#include <WiFiUdp.h>

#include <NTPClient.h>
#include <time.h>

// ----------------------------
// Additional Libraries - each one of these will need to be installed.
// ----------------------------

#include <CoinMarketCapApi.h>
// For Integrating with the CoinMarketCap.com API
// Available on the library manager (Search for "CoinMarket")
// https://github.com/witnessmenow/arduino-coinmarketcap-api

#include "SSD1306.h"
// The driver for the OLED display
// Available on the library manager (Search for "oled ssd1306")
// https://github.com/squix78/esp8266-oled-ssd1306

#include <ArduinoJson.h>
// Required by the CoinMarketCapApi Library for parsing the response
// Available on the library manager (Search for "arduino json")


// ----------------------------
// Configurations - Update these
// ----------------------------

// Pins based on your wiring
#define SCL_PIN D5
#define SDA_PIN D3

// CoinMarketCap's limit is "no more than 10 per minute"
// Make sure to factor in if you are requesting more than one coin.
// We'll request a new value just before we change the screen so it's the most up to date
unsigned long screenChangeDelay = 8000; // Every 10 seconds

// Have tested up to 10, can probably do more
#define MAX_HOLDINGS 10

#define CURRENCY "usd" //See CoinMarketCap.com for currency options (usd, gbp etc)
#define CURRENCY_SYMBOL "$" // Euro doesn't seem to work, $ and ÂŁ do

// You also need to add your crypto currecnies in the setup function

// ----------------------------
// End of area you need to change
// ----------------------------


WiFiClientSecure client;
CoinMarketCapApi api(client);

SSD1306   display(0x3c, SDA_PIN, SCL_PIN);

unsigned long screenChangeDue;

struct Holding {
  String tickerId;
  float amount;
  bool inUse;
  String currency;
  String curSymb;
  CMCTickerResponse lastResponse;
};

Holding holdings[MAX_HOLDINGS];

int currentIndex = -1;
String ipAddressString;

void addNewHolding(String tickerId, float amount = 0, String currency = "usd", String curSymb = "$") {
  int index = getNextFreeHoldingIndex();
  if (index > -1) {
    holdings[index].tickerId = tickerId;
    holdings[index].amount = amount;
    holdings[index].currency = currency;
    holdings[index].curSymb = curSymb;
    holdings[index].inUse = true;
  }
}

WiFiUDP ntpUDP;

NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", 3600, 60000);

int getYear() {
  time_t rawtime = timeClient.getEpochTime();
  struct tm * ti;
  ti = localtime (&rawtime);
  int year = ti->tm_year + 1900;

  return year;
}

int getMonth() {
  time_t rawtime = timeClient.getEpochTime();
  struct tm * ti;
  ti = localtime (&rawtime);
  int month = (ti->tm_mon + 1) < 10 ? 0 + (ti->tm_mon + 1) : (ti->tm_mon + 1);

  return month;
}

int getDate() {
  time_t rawtime = timeClient.getEpochTime();
  struct tm * ti;
  ti = localtime (&rawtime);
  int date = (ti->tm_mday) < 10 ? 0 + (ti->tm_mday) : (ti->tm_mday);

  return date;
}

int wifiConnect(char ssid[], char password[]) {
  // Try to connect ssid/password wifi

  // Announce connecting
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_16);
  display.drawString(64, 0, F("Connecting Wifi:"));
  display.setFont(ArialMT_Plain_10);
  display.drawString(64, 18, ssid);
  display.display();

  // Set WiFi to station mode and disconnect from an AP if it was Previously
  // connected
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  // Attempt to connect to Wifi network:
  Serial.print(F("Connecting Wifi: "));
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  // Just wait for 12s or connect
  for (int i = 0; (i < 24) and (WiFi.status() != WL_CONNECTED); i++)
  {
    Serial.print(".");
    delay(500);
  };

  // Check if connected to wifi
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("");
    Serial.println(F("WiFi connected"));
    Serial.println(F("IP address: "));
    IPAddress ip = WiFi.localIP();
    Serial.println(ip);
    ipAddressString = ip.toString();

    // Announce connection details
    display.clear();
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.setFont(ArialMT_Plain_16);
    //  display.drawString(64, 0, F("WiFi connected:")+(char*)(ssid));
    display.drawString(64, 0, "WiFi connected:" + (String)(ssid));
    display.setFont(ArialMT_Plain_10);
    display.drawString(64, 18, ipAddressString);
    display.display();
  } else  // Failed to connect
  {
    Serial.println("");
    Serial.println(F("Failed to connect to"));
    Serial.println(ssid);

    // Announce connection details
    display.clear();
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.setFont(ArialMT_Plain_16);
    display.drawString(64, 0, F("Failed to connect to"));
    display.setFont(ArialMT_Plain_10);
    display.drawString(64, 18, ssid);
    display.display();

  };

  return (WiFi.status() == WL_CONNECTED);
}

void reconnectWifi() {
  // Try to connect to any of known networks
  do
  {
    if (WiFi.status() != WL_CONNECTED) {
      wifiConnect ("hotspot_name1", "xxx");
    }

    if (WiFi.status() != WL_CONNECTED) {
      wifiConnect ("hotspot_name1", "xxx");
    }

    if (WiFi.status() != WL_CONNECTED) {
      wifiConnect ("hotspot_name1", "xxx");
    }

    if (WiFi.status() != WL_CONNECTED) {
      wifiConnect ("hotspot_name1", "xxx");
    }

  }
  while (WiFi.status() != WL_CONNECTED);

  displayMessage(F("Wifi Connected"));
}


void setup() {

  Serial.begin(115200);

  // ----------------------------
  // Holdings - Add your currencies here
  // ----------------------------
  // Go to the currencies coinmarketcap.com page
  // and take the tickerId from the URL (use bitcoin or ethereum as an example)

  addNewHolding("bitcoin");
  addNewHolding("ethereum");
  addNewHolding("golem-network-tokens", 0, "eth", "e");
  addNewHolding("streamr-datacoin");
  addNewHolding("litecoin");
  addNewHolding("ripple");
  addNewHolding("rlc");
  addNewHolding("omisego");

  // ----------------------------
  // Everything below can be thinkered with if you want but should work as is!
  // ----------------------------

  // Initialising the display
  display.init();

  // Connect to Wifi
  if (WiFi.status() != WL_CONNECTED) {
    reconnectWifi();
  };

  // NTP sync
  displayMessage("NTP syncing");
  timeClient.begin();

  // Just wait for 12s or connect
  for (int i = 0; (i < 24) and (!timeClient.forceUpdate()); i++)
  {
    Serial.print(".");  
    delay(500);
  }

}

int getNextFreeHoldingIndex() {
  for (int i = 0; i < MAX_HOLDINGS; i++) {
    if (!holdings[i].inUse) {
      return i;
    }
  }

  return -1;
}

int getNextIndex() {
  for (int i = currentIndex + 1; i < MAX_HOLDINGS; i++) {
    if (holdings[i].inUse) {
      return i;
    }
  }

  for (int j = 0; j <= currentIndex; j++) {
    if (holdings[j].inUse) {
      return j;
    }
  }

  return -1;
}

void displayHolding(int index, String addString = "") {

  CMCTickerResponse response = holdings[index].lastResponse;

  display.clear();

  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_16);
  display.drawString(64, 0, response.symbol);
  display.setFont(ArialMT_Plain_24);
  double price = response.price_currency;
  if (price == 0) {
    price = response.price_usd;
  }
  display.drawString(64, 20, formatCurrency(price, holdings[index].curSymb));
  display.setFont(ArialMT_Plain_16);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  //  display.drawString(64, 48, " 1h:" + String(response.percent_change_1h) + "%");
  if (addString != "")
  {
    display.drawString(64, 48, addString);
  }
  else
  {
    display.drawString(64, 48, "24h: " + String(response.percent_change_24h) + "%");
  }
  display.display();
}

void displayMessage(String message) {
  display.clear();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawStringMaxWidth(0, 0, 128, message);
  display.display();
  Serial.println(message);
}

String formatCurrency(float price, String curSymb ) {
  String formattedCurrency = curSymb;
  int pointsAfterDecimal = 6;
  if (price > 100) {
    pointsAfterDecimal = 0;
  } else if (price > 1) {
    pointsAfterDecimal = 2;
  }
  formattedCurrency.concat(String(price, pointsAfterDecimal));
  return formattedCurrency;
}

String formatCurrency(float price ) {
  String formattedCurrency = CURRENCY_SYMBOL;
  int pointsAfterDecimal = 6;
  if (price > 100) {
    pointsAfterDecimal = 0;
  } else if (price > 1) {
    pointsAfterDecimal = 2;
  }
  formattedCurrency.concat(String(price, pointsAfterDecimal));
  return formattedCurrency;
}

bool loadDataForHolding(int index) {
  int nextIndex = getNextIndex();
  if (nextIndex > -1 ) {
    holdings[index].lastResponse = api.GetTickerInfo(holdings[index].tickerId, holdings[index].currency);
    return holdings[index].lastResponse.error == "";
  }

  return false;
}

char buffer[5];

int curMonth = -1;  // -1 means not set
int curDate;
int curHours;
int curMinutes;

void loop() {

  curHours = timeClient.getHours();
  curMinutes = timeClient.getMinutes();

  // Cache Month and Date if not set or after midnight (up to 3 mins)
  if (
    ( (curHours == 0) and (curMinutes == 0) )
    or
    ( (curHours == 0) and (curMinutes == 1) )
    or
    ( (curHours == 0) and (curMinutes == 2) )
    or
    (curMonth == -1) // -1 means not set
  ) {
    curMonth = getMonth();
    curDate = getDate();
  }

  unsigned long timeNow = millis();
  if ((timeNow > screenChangeDue))  {
    currentIndex = getNextIndex();
    if (currentIndex > -1) {
      if (loadDataForHolding(currentIndex)) {

        // View memory usage - for debuging
        // Serial.println(String(ESP.getFreeHeap()));
        
        // show 24h for half time, show date/time for the other half
        displayHolding(currentIndex);
        delay(screenChangeDelay / 2);
        if ( (currentIndex & 0x01) == 0) { // for even CurrentIndex (every second show date instead of time)
          sprintf(buffer, "%02d:%02d", curHours, curMinutes);
        } else
        {
          sprintf(buffer, "%02d-%02d", curMonth, curDate);
        }
        displayHolding(currentIndex, buffer);
      } else {
        displayMessage(F("Error loading data."));
        if (WiFi.status() != WL_CONNECTED) {
          reconnectWifi();
        };
      }
    } else {
      displayMessage(F("No funds to display. Edit the setup to add them"));
    }
    screenChangeDue = timeNow + screenChangeDelay;
  }
}

