
/* functions */

void addNewHolding(String tickerId, float amount, String currency, String curSymb) {
  int index = getNextFreeHoldingIndex();
  if (index > -1) {
    holdings[index].tickerId = tickerId;
    holdings[index].amount = amount;
    holdings[index].currency = currency;
    holdings[index].curSymb = curSymb;
    holdings[index].inUse = true;
  }
}

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


void connectWifi() {
  Serial.println("Connecting as wifi client...");
  WiFi.disconnect();
  WiFi.begin ( ssid, password );
  int connRes = WiFi.waitForConnectResult();
  Serial.print ( "connRes: " );
  Serial.println ( connRes );
}

void NTPsync() {
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

void displayHolding(int index, String addString) {

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




