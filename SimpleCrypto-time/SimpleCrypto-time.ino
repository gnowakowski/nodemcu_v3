/*******************************************************************
    A project to display crypto currency prices using an ESP8266

    Main Hardware:
    - NodeMCU Development Board (Any ESP8266 dev board will work)
    - OLED I2C Display (SH1106)

    Written by Brian Lough
    https://www.youtube.com/channel/UCezJOfu7OtqGzd5xrP3q6WA

    Modified by gn
 *******************************************************************/

/*
   This example serves a "hello world" on a WLAN and a SoftAP at the same time.
   The SoftAP allow you to configure WLAN parameters at run time. They are not setup in the sketch but saved on EEPROM.

   Connect your computer or cell phone to wifi network ESP_ap with password 12345678. A popup may appear and it allow you to go to WLAN config. If it does not then navigate to http://192.168.4.1/wifi and config it there.
   Then wait for the module to connect to your wifi and take note of the WLAN IP it got. Then you can disconnect from ESP_ap and return to your regular WLAN.

   Now the ESP8266 is in your network. You can reach it through http://192.168.x.x/ (the IP you took note of) or maybe at http://esp8266.local too.

   This is a captive portal because through the softAP it will redirect any http request to http://192.168.4.1/
*/

// ----------------------------
// Standard Libraries - Already Installed if you have ESP8266 set up
// ----------------------------

#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <Wire.h>

#include <WiFiUdp.h>

#include <NTPClient.h>
#include <time.h>

#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <ESP8266mDNS.h>
#include <EEPROM.h>

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

/*
   This part comes from Captive Portal Advanced

*/

/* Set these to your desired softAP credentials. They are not configurable at runtime */
const char *softAP_ssid = "ESP_ap";
const char *softAP_password = "12345678";

/* hostname for mDNS. Should work at least on windows. Try http://esp8266.local */
const char *myHostname = "esp8266";

/* Don't set this wifi credentials. They are configurated at runtime and stored on EEPROM */
char ssid[32] = "";
char password[32] = "";

// DNS server
const byte DNS_PORT = 53;
DNSServer dnsServer;

// Web server
ESP8266WebServer server(80);

/* Soft AP network parameters */
IPAddress apIP(192, 168, 4, 1);
IPAddress netMsk(255, 255, 255, 0);


/** Should I connect to WLAN asap? */
boolean connect;

/** Last time I tried to connect to WLAN */
long lastConnectTry = 0;

/** Current WLAN status */
int status = WL_IDLE_STATUS;

/*
   The above part comes from Captive Portal Advanced

*/


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
#define CURRENCY_SYMBOL "$" // Euro doesn't seem to work, $ and Ä‚â€šÄąďż˝ do

// You also need to add your crypto currecnies in the setup function

// ----------------------------
// End of area you need to change
// ----------------------------


/* global variables */

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

WiFiUDP ntpUDP;

NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", 3600, 60000);

int ConnectedorCaptivity = false;

char buffer[5];

int curMonth = -1;  // -1 means not set
int curDate;
int curHours;
int curMinutes;

/* Prototypes */

void addNewHolding(String tickerId, float amount = 0, String currency = "usd", String curSymb = "$");
void displayHolding(int index, String addString = "");

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

  // Try to recover WLAN Credentials
  loadCredentials(); // Load WLAN credentials from network
  connect = strlen(ssid) > 0; // Request WLAN connect if there is a SSID

  // Connect to Wifi
  if (WiFi.status() != WL_CONNECTED) {
    wifiConnect(ssid,password);
  };

  // Set up Captivity
  if (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println();
    Serial.print(F("Configuring access point..."));
    displayMessage(F("Configuring access point..."));
    /* You can remove the password parameter if you want the AP to be open. */
    WiFi.softAPConfig(apIP, apIP, netMsk);
    WiFi.softAP(softAP_ssid, softAP_password);
    delay(500); // Without delay I've seen the IP address blank
    Serial.print("AP IP address: ");
    Serial.println(WiFi.softAPIP());
  
    /* Setup the DNS server redirecting all the domains to the apIP */  
    dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
    dnsServer.start(DNS_PORT, "*", apIP);
  
    /* Setup web pages: root, wifi config pages, SO captive portal detectors and not found. */
    server.on("/", handleRoot);
    server.on("/wifi", handleWifi);
    server.on("/wifisave", handleWifiSave);
    server.on("/generate_204", handleRoot);  //Android captive portal. Maybe not needed. Might be handled by notFound handler.
    server.on("/fwlink", handleRoot);  //Microsoft captive portal. Maybe not needed. Might be handled by notFound handler.
    server.onNotFound ( handleNotFound );
    server.begin(); // Web server start
    Serial.println(F("HTTP server started"));
    loadCredentials(); // Load WLAN credentials from network
    connect = strlen(ssid) > 0; // Request WLAN connect if there is a SSID
    ConnectedorCaptivity = true;
  } else // connected!
  {
    NTPsync();
    ConnectedorCaptivity = false;    
  };
}

void loop() {

  if ( not ConnectedorCaptivity ) {

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
          Serial.println(String(ESP.getFreeHeap()));
  
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
  } else // Captivity
  {
    if (connect) {
      Serial.println ( F("Connect requested") );
      connect = false;
      connectWifi();
      lastConnectTry = millis();
      if (WiFi.status() == WL_CONNECTED) { 
        ConnectedorCaptivity = false;
        ESP.restart();
      };
    }
    {
      int s = WiFi.status();
      if (s == 0 && millis() > (lastConnectTry + 60000) ) {
        /* If WLAN disconnected and idle try to connect */
        /* Don't set retry time too low as retry interfere the softAP operation */
        connect = true;
      }
      if (status != s) { // WLAN status change
        Serial.print ( F("Status: ") );
        Serial.println ( s );
        status = s;
        if (s == WL_CONNECTED) {
          /* Just connected to WLAN */
          Serial.println ( "" );
          Serial.print ( F("Connected to ") );
          Serial.println ( ssid );
          Serial.print ( F("IP address: ") );
          Serial.println ( WiFi.localIP() );
  
          // Setup MDNS responder
          if (!MDNS.begin(myHostname)) {
            Serial.println(F("Error setting up MDNS responder!"));
          } else {
            Serial.println(F("mDNS responder started"));
            // Add service to MDNS-SD
            MDNS.addService("http", "tcp", 80);
          }
        } else if (s == WL_NO_SSID_AVAIL) {
          WiFi.disconnect();
        }
      }
    }
    // Do work:
    //DNS
    dnsServer.processNextRequest();
    //HTTP
    server.handleClient();    
  }

}



