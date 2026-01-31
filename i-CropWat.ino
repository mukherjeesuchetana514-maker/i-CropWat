
#define BLYNK_PRINT Serial
#define BLYNK_TEMPLATE_ID "TMPL3LEYAZRZi"
#define BLYNK_TEMPLATE_NAME "iCropWat"

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <time.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>

// ===== USER SETTINGS =====
char BLYNK_AUTH[] = "YOUR_BLYNK_AUTH_TOKEN";
const char* WIFI_SSID = "YOUR_WIFI_NAME";
const char* WIFI_PASS = "YOUR_WIFI_PASSWORD";
const char* OWM_APIKEY = "YOUR_WEATHER_API_KEY";// WeatherAPI key (fill this)

// Hardware pins
#define DHTPIN    4
#define DHTTYPE   DHT11
#define SOIL_PIN  34
#define RELAY_PIN 5   // Active-LOW relay (LOW = ON, HIGH = OFF)

// LCD
LiquidCrystal_I2C lcd(0x27, 20, 4);

// DHT
DHT dht(DHTPIN, DHTTYPE);

// Menu buttons (LOW when pressed)
#define MENU_BTN   12
#define UP_BTN     14
#define DOWN_BTN   26
#define SELECT_BTN 25

// Plant lists + thresholds
String cropNames[10]   = {"Wheat","Rice","Maize","Barley","Oats","Millet","Soybean","Cotton","Sugarcane","Potato"};
String flowerNames[10] = {"Rose","Tulip","Lily","Sunflower","Orchid","Daisy","Marigold","Jasmine","Lotus","Hibiscus"};
int    cropThresholds[10]   = {40,45,50,55,35,60,50,45,40,55};
int    flowerThresholds[10] = {35,40,45,50,38,42,48,43,39,44};
int    defaultThreshold     = 40;

// Soil ADC calibration (tune to your sensor)
const float dryValue = 4095.0;  // ADC at bone-dry
const float wetValue = 1806.0;  // ADC fully wet

// Timers (ms)
const unsigned long SENSOR_INTERVAL   = 2000UL;    // 2s sensor read
const unsigned long WEATHER_INTERVAL  = 600000UL;  // 10min weather update
const unsigned long LCD_ALT_INTERVAL  = 30000UL;   // 30s alternate display

unsigned long lastSensorMillis = 0;
unsigned long lastWeatherMillis = 0;
unsigned long lastLCDAltMillis = 0;

// Menu state
bool inMenu = true;
int  menuStep = 0;          // 0 select category, 1 select plant
int  categoryIndex = 0;     // 0 crops, 1 flowers, 2 default
int  plantIndex = 0;        // 0..9
unsigned long lastDebounce = 0;
const unsigned long DEBOUNCE_DELAY = 200;

// After selection this becomes true and LCD alternation starts
bool menuCompleted = false;

// Blink feature
unsigned long lastBlinkMillis = 0;
const unsigned long BLINK_INTERVAL = 500;
bool blinkState = false;

// Weather / location vars
String cityName = "Unknown";
float prob_by_max = 0.0;        // 0..100 (max PoP across tomorrow's slots)
float prob_cumulative = 0.0;    // 0..100 (1 - product(1-PoP))
float expected_rain = 0.0;      // mm (expected sum)
String rainType = "NoRain";   // NoRain / Light / Heavy
String rainTime = "";         // HH:MM of first/likely rain (local)

// Pump control
bool pumpRunning = false;

// thresholds priorities
const float CRITICAL_SOIL = 20.0; // if soil <= this => immediate water
const float SAFE_SOIL     = 90.0; // if soil >= this => stop pump

// Weather probability thresholds (adjustable)
const float PROB_REDUCE_THRESHOLD = 50.0; // if >=50 => show REDUCE (informative)
const float PROB_SKIP_THRESHOLD   = 70.0; // if >=70 => skip watering (if soil not critical)

// Forward declarations
void handleMenu();
void sensorAndControlTask();
void startPump();
void stopPump();
void fetchLocationAndWeather();
void displaySensorView();
void displayWeatherView();

// ---------- Helpers ----------
// Buttons are LOW when pressed (INPUT_PULLUP)
bool buttonPressed(int pin, unsigned long holdTimeMs = 0) {
  if (digitalRead(pin) == LOW) {
    unsigned long pressedAt = millis();
    while (digitalRead(pin) == LOW) {
      if (holdTimeMs && (millis() - pressedAt >= holdTimeMs)) break;
      delay(1);
    }
    if (millis() - lastDebounce > DEBOUNCE_DELAY) {
      lastDebounce = millis();
      return true;
    }
  }
  return false;
}

float readSoilPercentOnce() {
  int raw = analogRead(SOIL_PIN); // 0..4095
  float pct = ((raw - dryValue) * 100.0) / (wetValue - dryValue);
  return constrain(pct, 0.0, 100.0);
}

// Pads or truncates to exactly 20 chars and prints row
void lcdRowPrint(int row, String s) {
  if ((int)s.length() > 20) s = s.substring(0,20);
  while ((int)s.length() < 20) s += ' ';
  lcd.setCursor(0,row);
  lcd.print(s);
}

// Compose a fixed-width number (right-aligned within width)
String fwInt(int val, int width) {
  String s = String(val);
  while ((int)s.length() < width) s = " " + s;
  return s;
}

// Portable replacement for timegm() to normalize struct tm -> time_t in UTC
time_t myTimegm(struct tm *tm) {
  char* tz = getenv("TZ");
  setenv("TZ", "UTC0", 1);
  tzset();
  time_t ret = mktime(tm);
  if (tz) setenv("TZ", tz, 1); else unsetenv("TZ");
  tzset();
  return ret;
}

// ---------- Setup ----------
void setup() {
  Serial.begin(115200);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH); // Active-LOW: HIGH = OFF at boot

  pinMode(MENU_BTN,   INPUT_PULLUP);
  pinMode(UP_BTN,     INPUT_PULLUP);
  pinMode(DOWN_BTN,   INPUT_PULLUP);
  pinMode(SELECT_BTN, INPUT_PULLUP);

  lcd.init();
  lcd.backlight();
  dht.begin();

  // WiFi connect (no LCD messages)
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) {
    delay(250);
  }
  if (WiFi.status() == WL_CONNECTED) Serial.println("WiFi OK");
  else Serial.println("WiFi not connected");

  Blynk.begin(BLYNK_AUTH, WIFI_SSID, WIFI_PASS);

  // initial weather fetch (does nothing if WiFi not connected)
  fetchLocationAndWeather();

  lastSensorMillis  = millis();
  lastWeatherMillis = millis();
  lastLCDAltMillis  = millis();
}

// ---------- Main loop ----------
void loop() {

  unsigned long now = millis();

  Blynk.run();

  // Menu
  if (!menuCompleted) {
    if (inMenu) handleMenu();
  } else {
    // periodic sensor & pump decision
    if (now - lastSensorMillis >= SENSOR_INTERVAL) {
      lastSensorMillis = now;
      sensorAndControlTask();
    }
    // periodic weather
    if (now - lastWeatherMillis >= WEATHER_INTERVAL) {
      lastWeatherMillis = now;
      fetchLocationAndWeather();
    }
    // LCD alternation
    if (now - lastLCDAltMillis >= LCD_ALT_INTERVAL) {
      lastLCDAltMillis = now;
      static bool showWeather = false;
      showWeather = !showWeather;
      if (showWeather) displayWeatherView();
      else             displaySensorView();
    }
  }

  // Blink logic for pump status
  if (pumpRunning && now - lastBlinkMillis >= BLINK_INTERVAL) {
    lastBlinkMillis = now;
    blinkState = !blinkState;
    String row3 = blinkState ? "PUMP ACTIVE " : " ";
    lcdRowPrint(3, row3);
  }
  

  // Re-enter menu only if MENU held for 500 ms (prevents false triggers)
  if (buttonPressed(MENU_BTN, 500)) {
    inMenu = true;
    menuCompleted = false;
    menuStep = 0;
    lcd.clear();
    delay(150);
  }
}

// ---------- Menu ----------
void handleMenu() {
  lcd.clear();
  if (menuStep == 0) {
    // Category
    lcdRowPrint(0, "Select Category:");
    String cats[3] = {"Crops","Flowers","Default"};
    for (int i=0;i<3;i++){
      String line = (i==categoryIndex ? "> " : "  ");
      line += cats[i];
      lcdRowPrint(i+1, line);
    }

    unsigned long tstart = millis();
    while (millis() - tstart < 250) {
      if (buttonPressed(UP_BTN))   { categoryIndex = (categoryIndex+2)%3; break; }
      if (buttonPressed(DOWN_BTN)) { categoryIndex = (categoryIndex+1)%3; break; }
      if (buttonPressed(SELECT_BTN)) {
        if (categoryIndex == 2) {
          // Default → selection complete
          menuCompleted = true; inMenu = false;
          lcd.clear();
          sensorAndControlTask();  // immediate decision
          displaySensorView();
          lastLCDAltMillis = millis();
        } else {
          menuStep = 1; // go select plant
        }
        return;
      }
    }
    delay(120);
  }
  else if (menuStep == 1) {
    // Plant
    lcdRowPrint(0, "Select Plant:");
    String *plist = (categoryIndex==0) ? cropNames : flowerNames;
    for (int i=0;i<3;i++){
      int idx = (plantIndex + i) % 10;
      String line = (i==0 ? "> " : "  ");
      line += plist[idx];
      lcdRowPrint(i+1, line);
    }

    unsigned long tstart = millis();
    while (millis() - tstart < 250) {
      if (buttonPressed(UP_BTN))   { plantIndex = (plantIndex+9)%10; break; }
      if (buttonPressed(DOWN_BTN)) { plantIndex = (plantIndex+1)%10; break; }
      if (buttonPressed(SELECT_BTN)) {
        menuCompleted = true; inMenu = false;
        lcd.clear();
        sensorAndControlTask(); // immediate decision
        displaySensorView();
        lastLCDAltMillis = millis();
        return;
      }
    }
    delay(100);
  }
}

// ---------- Sensor & Pump ----------
void sensorAndControlTask() {
  float soilPct = readSoilPercentOnce();

  // DHT read (for display only)
  float airH = dht.readHumidity();
  float temp = dht.readTemperature();
  bool dhtOk = !(isnan(airH) || isnan(temp));
  (void)dhtOk; (void)temp;

  // Threshold for selected plant
  int threshold = defaultThreshold;
  if (categoryIndex == 0)      threshold = cropThresholds[plantIndex];
  else if (categoryIndex == 1) threshold = flowerThresholds[plantIndex];

  // Emergency priority
  if (soilPct <= CRITICAL_SOIL) {
    startPump();
    return;
  }

  // Safe cutoff
  if (soilPct >= SAFE_SOIL) {
    stopPump();
    return;
  }

  // Normal decision (soil-primary, weather-secondary)
  if (soilPct < threshold) {
    // If rain probability is high enough, skip watering (if soil not critical)
    if (prob_by_max >= PROB_SKIP_THRESHOLD) {
      // Skip watering due to high chance of rain
      stopPump();
      Serial.printf("Skipping pump: prob_by_max=%.1f >= %.1f\n", prob_by_max, PROB_SKIP_THRESHOLD);
    } else {
      // Otherwise, water (pump ON)
      startPump();
    }
  } else {
    stopPump();
  }

  // Update Blynk
  Blynk.virtualWrite(V2, soilPct);
  Blynk.virtualWrite(V3, pumpRunning ? 1 : 0);
  if (dhtOk) {
    Blynk.virtualWrite(V0, temp);
    Blynk.virtualWrite(V1, airH);
  }
  // Example: call this periodically (e.g., in loop())
  if (WiFi.status() == WL_CONNECTED) {
    Blynk.virtualWrite(V4, 1); // 1 = connected
  } else {
    Blynk.virtualWrite(V4, 0); // 0 = not connected
  }

}



// ---------- Pump control (Active-LOW) ----------
void startPump() {
  digitalWrite(RELAY_PIN, LOW); // ON (active LOW)
  pumpRunning = true;
  Serial.println("Pump ON");
}
void stopPump() {
  digitalWrite(RELAY_PIN, HIGH); // OFF
  pumpRunning = false;
  Serial.println("Pump OFF");
}

// ---------- Weather & Location (WeatherAPI) ----------
void fetchLocationAndWeather() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected; can't fetch weather");
    return;
  }

  // WeatherAPI forecast (auto-detect location by IP)
  // NOTE: replace OWM_APIKEY with your WeatherAPI key above
  String url = String("http://api.weatherapi.com/v1/forecast.json?key=") + String(OWM_APIKEY) + "&q=auto:ip&days=2&aqi=no&alerts=no";

  HTTPClient http;
  http.begin(url);
  int code = http.GET();
  if (code <= 0) {
    Serial.printf("WeatherHTTP failed: %d\n", code);
    http.end();
    return;
  }
  String payload = http.getString();
  http.end();

  // Parse JSON
  DynamicJsonDocument doc2(131072);
  DeserializationError err2 = deserializeJson(doc2, payload);
  if (err2) {
    Serial.println("Weather parse error");
    return;
  }

  // Location
  cityName = doc2["location"]["name"] | String("Unknown");

  // Forecast: tomorrow = forecast.forecastday[1]
  JsonArray fdays = doc2["forecast"]["forecastday"];
  if (fdays.size() < 2) {
    prob_by_max = 0; prob_cumulative = 0; expected_rain = 0; rainType = "NoRain"; rainTime = "";
    Serial.println("No tomorrow forecast available");
    return;
  }

  JsonObject tomorrow = fdays[1];
  JsonArray hours = tomorrow["hour"];

  // Compute from hourly slots
  prob_by_max = 0.0;
  expected_rain = 0.0;
  prob_cumulative = 0.0;
  float prod_no_rain = 1.0;
  int rainySlots = 0;
  int totalSlots = 0;
  rainTime = "";
  float bestPop = -1.0;

  for (JsonObject hour : hours) {
    totalSlots++;
    int pop = -1;
    if (hour.containsKey("chance_of_rain")) {
      pop = hour["chance_of_rain"].as<int>(); // 0..100
    } else if (hour.containsKey("will_it_rain")) {
      // fallback (0/1)
      pop = hour["will_it_rain"].as<int>() * 100;
    }
    float rainVol = hour.containsKey("precip_mm") ? hour["precip_mm"].as<float>() : 0.0f;
    const char* desc = "";
    if (hour.containsKey("condition") && hour["condition"].is<JsonObject>()) desc = hour["condition"]["text"] | "";

    if (pop >= 0) {
      if ((float)pop > prob_by_max) prob_by_max = (float)pop;
      prod_no_rain *= (1.0f - ((float)pop / 100.0f));
      if ((float)pop > bestPop) {
        bestPop = (float)pop;
        // hour["time"] e.g. "2025-08-19 15:00"
        String t = hour["time"].as<String>();
        int sp = t.indexOf(' ');
        if (sp >= 0) t = t.substring(sp+1);
        rainTime = t;
      }
    } else {
      if (rainVol > 0.0) {
        rainySlots++;
        if (rainTime == "") {
          String t = hour["time"].as<String>();
          int sp = t.indexOf(' ');
          if (sp >= 0) t = t.substring(sp+1);
          rainTime = t;
        }
      }
    }
    expected_rain += ((pop>=0) ? ((float)pop / 100.0f) * rainVol : rainVol);
  }

  if (bestPop >= 0.0) {
    prob_cumulative = (1.0f - prod_no_rain) * 100.0f;
  } else {
    if (totalSlots > 0) {
      prob_by_max = ((float)rainySlots / (float)totalSlots) * 100.0f;
      prob_cumulative = prob_by_max;
    } else {
      prob_by_max = prob_cumulative = 0.0f;
    }
  }

  // classify
  if (prob_by_max >= 60.0 || expected_rain >= 5.0)      rainType = "Heavy";
  else if (prob_by_max >= 30.0 || expected_rain >= 1.0)  rainType = "Light";
  else                                                   rainType = "NoRain";

  Serial.printf("WeatherAPI [%s]: probMax=%.1f%% cumul=%.1f%% exp=%.2fmm time=%s\n",
                rainType.c_str(), prob_by_max, prob_cumulative, expected_rain, rainTime.c_str());
}

// ---------- Displays ----------
void displaySensorView() {
  float soilPct = readSoilPercentOnce();
  float airH = dht.readHumidity();
  float temp = dht.readTemperature();
  bool dhtOk = !(isnan(airH) || isnan(temp));

  int threshold = defaultThreshold;
  String plantName = "Default";
  if (categoryIndex == 0) { threshold = cropThresholds[plantIndex];   plantName = cropNames[plantIndex]; }
  else if (categoryIndex == 1) { threshold = flowerThresholds[plantIndex]; plantName = flowerNames[plantIndex]; }

  // Row0 (20 chars) e.g. "M:100% H: 85% T:29C"
  String m = "M:" + fwInt((int)(soilPct + 0.5), 3) + "%";
  String h = dhtOk ? (" H:" + fwInt((int)(airH + 0.5), 3) + "%") : " H: --%";
  String t = dhtOk ? (" T:" + fwInt((int)(temp + 0.5), 2) + "C") : " T: --C";
  String row0 = m + h + t;
  lcd.clear();
  lcdRowPrint(0, row0);

  // Row1 - relay & pump (active-low)
  bool relayOn = (digitalRead(RELAY_PIN) == LOW);
  String row1 = String("Relay:") + (relayOn ? "ON " : "OFF") + " Pump:" + (pumpRunning ? "ON " : "OFF");
  lcdRowPrint(1, row1);

  // Row2 - plant & threshold
  String pname = plantName;
  if ((int)pname.length() > 10) pname = pname.substring(0,10);
  String row2 = "Plant:" + pname + " Th:" + String(threshold) + "%";
  lcdRowPrint(2, row2);

  // Row3 - footer
  lcdRowPrint(3, "By, i-CropWat");
}

void displayWeatherView() {
  lcd.clear();

  // Row0: Fixed location "Liluah"
  String c = "Liluah";   // <-- fixed location
  lcdRowPrint(0, c);

  // Row1: rain type
  lcdRowPrint(1, "Rain: " + rainType);

  // Row2: probability & time
  String row2 = "Prob:" + fwInt((int)(prob_by_max + 0.5), 3) + "% Time:" + (rainTime.length() ? rainTime : "--:--");
  lcdRowPrint(2, row2);

  // Row3: decision based on soil + DHT
  float soilPct = readSoilPercentOnce();
  float airH = dht.readHumidity();
  float temp = dht.readTemperature();

  int threshold = defaultThreshold;
  if (categoryIndex == 0)      threshold = cropThresholds[plantIndex];
  else if (categoryIndex == 1) threshold = flowerThresholds[plantIndex];

  String decision;
  if (soilPct <= CRITICAL_SOIL) {
    decision = "IMMEDIATE WATER";
  } else if (soilPct >= SAFE_SOIL) {
    decision = "SKIP (Moisture High)";
  } else {
    if (soilPct < threshold) {
      if (prob_by_max >= PROB_SKIP_THRESHOLD) decision = "SKIP (Rain Pred)";
      else if (prob_by_max >= PROB_REDUCE_THRESHOLD) decision = "REDUCE WATER";
      else decision = "NORMAL WATER";

      // Append DHT context (informative only)
      if (!isnan(temp) && temp > 40.0) decision += " | Hot";
      if (!isnan(airH) && airH < 40.0) decision += " | DryAir";
    } else {
      decision = "SKIP (Moisture OK)";
    }
  }
  if ((int)decision.length() > 20) decision = decision.substring(0,20);
  lcdRowPrint(3, decision);
}
