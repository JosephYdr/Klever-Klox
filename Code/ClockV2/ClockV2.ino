#include <WiFi.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <ESP32Time.h>
#include <AudioTools.h>
#include <AudioTools/Disk/AudioSourceSDFAT.h>
#include <AudioTools/AudioCodecs/CodecMP3Helix.h>
#include <FastLED.h>
#include <ESPRotary.h>
#include <esp_timer.h>

#define NewScreen 1

#ifdef NewScreen
  #define ScreenType INITR_GREENTAB
#else
  #define ScreenType INITR_BLACKTAB
#endif

#define LED_DIN_PIN   5
#define TFT_CS_PIN    6
#define TFT_RST_PIN   7
#define TFT_DC_PIN    8
#define TFT_MOSI_PIN  35 //SDA
#define TFT_SCK_PIN   36 //SCL
#define TFT_BLK_PIN   11
#define ENC_CLK_PIN   12
#define ENC_DT_PIN    13
#define ENC_SW_PIN    14
#define I2S_BCK_PIN   40 //BCLK
#define I2S_WS_PIN    39 //LRC
#define I2S_DATA_PIN  38 //DIN
#define TCH_MODE_PIN  4
#define TCH_ADJ_PIN   2
#define TCH_ON_PIN    3
#define TCH_NC_PIN    1
#define SD_CS_PIN     34
#define SD_MISO_PIN   37
#define SD_MOSI_PIN   TFT_MOSI_PIN
#define SD_SCK_PIN    TFT_SCK_PIN

#define screenTimeOut      1   //in minutes
#define MaxBrightness      150 //(0-250)
#define NUM_LEDS           14
#define LED_TYPE           WS2813
#define CLICKS_PER_STEP    4   
#define TouchPinThreshold  10000
#define displayRefreshRate 100

// #define connectionFrequency 10 //120 * number of mins between wifi reconnects
#define reconnectionAttempts 3

// Replace with your network credentials
String ssid     =                   "";
String password =                   "";
String ntpServer =   "pool.ntp.org";
String tz = "GMT0IST,M3.5.0/1,M10.4.0"; // Timezone for Ireland (IST/GMT with DST)
const char* startFilePath =        "/";
const char* ext =                "mp3";

bool wifiStatus = false;      //whether it is connected to wifi or not
bool ledOn = false;           //whether the led is set to be on or off
bool alarmOn = false;         //whether the alarm is sounding or not
bool alarmSet = true;         //whether the alarm is set or not
bool changing = false;        //flag for stopping the time from blinking while you increase or decrease the mins or hours
bool twelveHour = false;      //whether twelve hour mode or not
bool screenOn = true;         //whether the screen is switched on or not
bool screenSwitchOff = true;  //whether the screen will switch off after 1 minute or not
bool LEDFadeOn = false;       //whether the light will fade on as the alarm time approaches or not
bool autoNextSong = true;     //loop through the songs
bool alarmArmed = false;      //alarm set to go off soon
String mode = "time";         //mode of display ("time" for normal mode or "alarm" to set the alarm also "LED", "song" and "settings")
String LEDMode = "Custom";    //led colour
String IPAdress = "";         //IP address for webpage
String header   = "";         //input from webpage
int screenMode =   0;         //which submode you are on
//default times
int Dsec =         0;
int Dmin =         0;
int Dhour =        0;
int Dday =         1;
int Dmonth =       1;
int Dyear =     2025;
//alarm times
int alarmHour =    6;
int alarmMin =    45;
//last time touch
int lastTouch = millis();
int lastDisplay = millis();
int lastUpdate = millis() + 60000;
int alarmTime = 0;     //time for song to start
//colours
int Hue =        255;
int Brightness = 255;
//brightness of screen (0-250)
int contrast =   250;
//volume (0-1)
float volume =  1.00;
//number of days in current month
int daysInMonth =   31;
int daysInYear  =  365;
//number of current song
int songIndex =      0;
//time for led to fade on before alarm
int ledFadeTime =   30;//in mins
int currentRainbowColour = 0;

volatile int encoderValue = 0;
volatile int lastEncoderValue = 0;
volatile bool updateEncoderFlag = false; // Flag for updates
uint16_t backgroundColour;

//setup led strip object
CRGB leds[NUM_LEDS];

// configure SDFAT  with CS pin
SdSpiConfig sdcfg(SD_CS_PIN, DEDICATED_SPI, SD_SCK_MHZ(10) , &SPI);

// Define SPI pins
AudioSourceSDFAT source(startFilePath, ext, sdcfg);
I2SStream i2s;
MP3DecoderHelix decoder;
AudioPlayer player(source, i2s, decoder);

// Create an instance of the ST7735 display
Adafruit_ST7735 display = Adafruit_ST7735(&SPI, TFT_CS_PIN, TFT_DC_PIN, TFT_RST_PIN);

//ESP32Time rtc;
ESP32Time rtc(0);

//set up wifi server
WiFiServer server(80);

ESPRotary r;
esp_timer_handle_t timer;

//prints sd card info
void printMetaData(MetaDataType type, const char* str, int len) {
  Serial.print("==> ");
  Serial.print(toStr(type));
  Serial.print(": ");
  Serial.println(str);
}

void readSettings() {

  // Initialize SD card
  SdFat32 &sd = source.getAudioFs();
  File32 file = sd.open("/settings.txt", FILE_READ);

  if (!file) {
    Serial.print("error opening settings.txt");
    return;
  }

  while (file.available()) {
    String line = file.readStringUntil('\n');
    int separatorIndex = line.indexOf(':');
    if (separatorIndex > 0) {
      String key = line.substring(0, separatorIndex);
      String value = line.substring(separatorIndex + 1);
      value.trim(); // Remove newline and whitespace
      if (key == "ssid") {
        ssid = value;
      } else if (key == "password") {
        password = value;
      }else if (key == "NTP") {
        ntpServer = value;
      }else if (key == "zone") {
        tz = value;
      }else if (key == "LED mode") {
        LEDMode = value;
      }else if (key == "alarm mins" && value.toInt()) {
        alarmMin = value.toInt();
      }else if (key == "alarm hours" && value.toInt()) {
        alarmHour = value.toInt();
      }else if (key == "sec" && value.toInt()) {
        Dsec = value.toInt();
      }else if (key == "min" && value.toInt()) {
        Dmin = value.toInt();
      }else if (key == "hour" && value.toInt()) {
        Dhour = value.toInt();
      }else if (key == "day" && value.toInt()) {
        Dday = value.toInt();
      }else if (key == "month" && value.toInt()) {
        Dmonth = value.toInt();
      }else if (key == "year" && value.toInt()) {
        Dyear = value.toInt();
      }else if (key == "hue" && value.toInt()) {
        Hue = value.toInt();
      }else if (key == "brightness" && value.toInt()) {
        Brightness = value.toInt();
      }else if (key == "song number" && value.toInt()) {
        songIndex = value.toInt();
      }else if (key == "led fade time" && value.toInt()) {
        ledFadeTime = value.toInt();
      }else if (key == "volume" && value.toFloat()) {
        volume = value.toFloat();
      }else if (key == "LED is on") {
        ledOn = (value == "on");
      }else if (key == "twelve hour time") {
        twelveHour = (value == "on");
      }else if (key == "screen switches off after a min") {
        screenSwitchOff = (value == "on");
      }else if (key == "LED fades on as light approaches") {
        LEDFadeOn = (value == "on");
      }else if (key == "automatically go to next song") {
        autoNextSong = (value == "on");
        player.setAutoNext(autoNextSong);
      }
    }
  }
  file.close();
}

void saveSettings() {
  String fileContent = ""; // Buffer for file content

  // Open the file for reading
  player.end();
  SdFat32& sd = source.getAudioFs();
  File32 file = sd.open("/settings.txt", FILE_READ); // Open in read mode
  if (file) {
    // Read the entire file into a string
    while (file.available()) {
      fileContent += char(file.read());
    }
    file.close();
  }

  // Helper function to replace or add key-value pairs
  auto replaceValue = [&](const String& key, const String& value) {
    int startIndex = fileContent.indexOf(key);
    if (startIndex != -1) {
      // Key exists; replace the value
      int endIndex = fileContent.indexOf('\n', startIndex);
      if (endIndex == -1) endIndex = fileContent.length();
      String oldValue = fileContent.substring(startIndex, endIndex);
      fileContent.replace(oldValue, key + ": " + value);
    } else {
      // Key does not exist; append it to the content
      fileContent += key + ": " + value + "\n";
    }
  };

  // Replace or add settings
  replaceValue("LED mode", LEDMode);
  replaceValue("alarm mins", String(alarmMin));
  replaceValue("alarm hours", String(alarmHour));
  replaceValue("sec", String(Dsec));
  replaceValue("min", String(Dmin));
  replaceValue("hour", String(Dhour));
  replaceValue("day", String(Dday));
  replaceValue("month", String(Dmonth));
  replaceValue("year", String(Dyear));
  replaceValue("hue", String(Hue));
  replaceValue("brightness", String(Brightness));
  replaceValue("song number", String(songIndex));
  replaceValue("volume", String(volume));
  replaceValue("LED is on", ledOn ? "on" : "off");
  replaceValue("twelve hour time", twelveHour ? "on" : "off");
  replaceValue("screen switches off after a min", screenSwitchOff ? "on" : "off");
  replaceValue("LED fades on as light approaches", LEDFadeOn ? "on" : "off");
  replaceValue("automatically go to next song", autoNextSong ? "on" : "off");

  // Delete the existing file if it exists
  sd.remove("/settings.txt");

  // Write the modified content back to the file
  file = sd.open("/settings.txt", FILE_WRITE); // Open in write mode
  if (!file) {
    Serial.println("Failed to open file for writing.");
    return;
  }
  file.print(fileContent);
  file.close();

  player.begin();
  player.setIndex(songIndex);
  Serial.println("Settings saved successfully.");
}

//find number of days in current month and year
void findDayNum() {
  if (Dmonth == 6 || Dmonth == 4 || Dmonth == 9 || Dmonth == 11) {
    daysInMonth = 30;
  } else if (Dmonth == 2) {
    if (Dyear % 4 == 0 && (Dyear % 100 != 0 || Dyear % 400 == 0)) {
      daysInMonth = 29;
    } else {
      daysInMonth = 28;
    }
  } else {
    daysInMonth = 31;
  }
  if (Dyear % 4 == 0 && (Dyear % 100 != 0 || Dyear % 400 == 0)) {
    daysInYear = 366;
  } else {
    daysInYear = 265;
  }
}

int nonNegativeModulo(int a, int b) {//10, 256
  Serial.print(encoderValue);
  Serial.print(", ");
  Serial.print(a);
  Serial.print(", ");
  Serial.print(b);
  a = (a + encoderValue) % b;
  if (a < 0) {
      a += b;
  }
  Serial.print(encoderValue);
  Serial.print(", ");
  Serial.print(a);
  Serial.print(", ");
  Serial.println(b);
  return a;
}

//called whenever a button is pressed
void onTouch() {
  lastTouch = millis();
  if (!screenOn) {
    displayOn();
  }
}

//updates time variables
void updateTimes() {
  Dsec = rtc.getSecond();
  Dmin = rtc.getMinute();
  Dhour = rtc.getHour(true);
  Dday = rtc.getDay();
  Dmonth = rtc.getMonth() + 1;
  Dyear = rtc.getYear();
  findDayNum();
}

void LEDColour() {
  if (LEDMode == "Custom") {
    fill_solid(leds, NUM_LEDS, CHSV(Hue, 255, Brightness));
  } else if (LEDMode == "Red") {
    fill_solid(leds, NUM_LEDS, CRGB::Red);
  } else if (LEDMode == "Blue") {
    fill_solid(leds, NUM_LEDS, CRGB::Blue);
  } else if (LEDMode == "Green") {
    fill_solid(leds, NUM_LEDS, CRGB::Green);
  } else if (LEDMode == "Yellow") {
    fill_solid(leds, NUM_LEDS, CRGB::Yellow);
  } else if (LEDMode == "Magenta") {
    fill_solid(leds, NUM_LEDS, CRGB::Magenta);
  } else if (LEDMode == "White") {
    fill_solid(leds, NUM_LEDS, CRGB::White);
  } else if (LEDMode == "Rainbow") {
    fill_rainbow(leds, NUM_LEDS, currentRainbowColour, 7);
    ++currentRainbowColour;
  }
}

//updates led
void updateLed() {
  updateTimes();
  int timeTillAlarm = 0;
  if ((alarmHour * 60 + alarmMin) >= (Dhour * 60 + Dmin)) {
    timeTillAlarm = (alarmHour * 60 + alarmMin) - (Dhour * 60 + Dmin);
  } else {
    timeTillAlarm = (alarmHour * 60 + alarmMin + 1440) - (Dhour * 60 + Dmin);
  }
  if (ledOn) {
    FastLED.setBrightness(MaxBrightness);
    if (mode == "led") {
      fill_solid(leds, NUM_LEDS, CHSV(Hue, 255, Brightness));
    } else {
      LEDColour();
    }
  } else if (timeTillAlarm <= ledFadeTime && LEDFadeOn) {
    FastLED.setBrightness(map(ledFadeTime - timeTillAlarm, 0, ledFadeTime, 0, MaxBrightness));
    LEDColour();
  } else {
    FastLED.clear();
  }
  FastLED.show();
}

void displayOff() {
  screenOn = false;
  analogWrite(TFT_BLK_PIN, 0);
  display.fillScreen(ST77XX_BLACK);  // Clear screen
}

void displayOn() {
  screenOn = true;
  analogWrite(TFT_BLK_PIN, contrast);
  display.fillRoundRect(0, 0, 160, 130, 10, backgroundColour);
  updateDisplay();
}

//updates display
void updateDisplay() {
  //display on time mode
  if (screenOn) {
    analogWrite(TFT_BLK_PIN, contrast);
    if (mode == "time") {
      display.setTextSize(2);
      display.setTextColor(ST77XX_RED, backgroundColour);
      display.setCursor(10, 10);
      if (!(int(millis() / 500) % 2 == 0 && screenMode == 1) || changing) {
        updateTimes();
        if (twelveHour) {
          display.print(rtc.getTime("%I"));
        } else {
          display.print(rtc.getTime("%H"));
        }

      } else {
        display.print("  ");
      }
      display.print(":");
      if (!(int(millis() / 500) % 2 == 0 && screenMode == 2) || changing) {
        display.print(rtc.getTime("%M"));
      } else {
        display.print("  ");
      }
      display.print(":");
      if (!(int(millis() / 500) % 2 == 0 && screenMode == 3) || changing) {
        display.print(rtc.getTime("%S"));
      } else {
        display.print("  ");
      }
      if (twelveHour) {
        display.print(rtc.getAmPm(true));
      }
      display.setTextSize(2);
      display.setTextColor(ST77XX_WHITE, backgroundColour);
      display.setCursor(10, 40);
      if (screenMode != 6) {
        display.print(rtc.getTime("%a "));
      }
      if ((!(int(millis() / 500) % 2 == 0 && screenMode == 4) || changing) && screenMode != 6) {
        display.print(rtc.getTime("%b "));
      } else {
        display.print("    ");
      }
      if ((!(int(millis() / 500) % 2 == 0 && screenMode == 5) || changing) && screenMode != 6) {
        display.print(rtc.getDay());
      } else {
        display.print("  ");
      }
      if ((!(int(millis() / 500) % 2 == 0 && screenMode == 6) || changing) && screenMode == 6) {
        display.setCursor(10, 40);
        display.setTextSize(2);
        display.print(rtc.getYear());
        display.print("     ");
      }
    }

    //display on alarm mode
    else if (mode == "alarm") {
      display.setTextSize(3);
      display.setTextColor(ST77XX_WHITE, backgroundColour);
      display.setCursor(10, 10);
      if (!(int(millis() / 500) % 2 == 0 && screenMode == 1) || changing) {
        if (alarmHour < 10) {
          display.print("0");
          display.print(alarmHour);
        } else {
          display.print(alarmHour);
        }
      } else {
        display.print("  ");
      }
      display.print(":");
      if (!(int(millis() / 500) % 2 == 0 && screenMode == 2) || changing) {
        if (alarmMin < 10) {
          display.print("0");
          display.print(alarmMin);
        } else {
          display.print(alarmMin);
        }
      } else {
        display.print("  ");
      }
    }
    //display on led mode
    else if (mode == "led") {
      display.setTextSize(2);
      display.setTextColor(ST77XX_WHITE, backgroundColour);
      display.setCursor(10, 10);
      if (screenMode == 0) {
        display.setTextSize(1);
      }
      if (screenMode == 1 || screenMode == 0) {
        display.print("Hue:");
        display.print(Hue);
        display.print("   ");
      } if (screenMode == 2 || screenMode == 0) {
        if (screenMode == 0) {
          display.setCursor(10, 40);
        }else {
          display.setTextSize(1.5);
        }
        display.print("Brightness:");
        display.print(Brightness);
        display.print("   ");
      }
    } else if (mode == "song") {
      display.setTextSize(3);
      display.setTextColor(ST77XX_WHITE, backgroundColour);
      display.setCursor(10, 10);
      display.print(songIndex);
      display.print("  ");
      display.setTextSize(1);
      display.setCursor(10, 40);
      char fileName[21];
      strncpy(fileName, source.toStr() + 1, 20);
      fileName[20] = '\0';
      display.print(fileName);
      display.print("                 ");
    } else if (mode == "settings") {
      display.setTextColor(ST77XX_WHITE, backgroundColour);
      display.setCursor(10, 10);
      display.setTextSize(2);
      if (screenMode == 0) {
        display.print("Settings");
      } else if (screenMode == 1) {
        display.print("Vol: ");
        display.println(volume);
      }else if (screenMode == 2) {
        display.setTextSize(2);
        display.print("contrast");
        display.setCursor(10, 40);
        display.setTextSize(1);
        if (contrast == 250) {
          display.print("Very Bright    ");
        }else if (contrast == 200) {
          display.print("Medium Bright  ");
        }else if (contrast == 150) {
          display.print("Slightly Bright");
        }else if (contrast == 100) {
          display.print("Slightly Dim   ");
        }else if (contrast == 50) {
          display.print("Medium Dim     ");
        }else if (contrast == 0) {
          display.print("Very Dim       ");
        }
      } else if (screenMode == 3) {
        display.setTextSize(2);
        display.print("IP Address:");
        display.setTextSize(1.5);
        display.setCursor(10, 30);
        display.print(IPAdress);
      }
    }
    //display A if alarm set
    display.setTextSize(1);
    display.setCursor(10, 100);
    if (alarmSet) {
      display.print("A");
    } else {
      display.print(" ");
    }
    //display LED if led on
    display.setCursor(20, 100);
    if (ledOn) {
      display.print("LED");
    } else {
      display.print("   ");
    }
    //display WiFi if WiFi connected
    display.setCursor(60, 100);
    if (wifiStatus) {
      display.print("WiFi");
    } else {
      display.print("    ");
    }
  }

  if (rtc.getHour(true) == alarmHour && rtc.getMinute() == alarmMin && !rtc.getSecond() && alarmSet && !alarmOn || (alarmArmed && millis() - alarmTime >= 0)) {
    alarmOn = true;
    if (LEDFadeOn) {
      ledOn = true;
    }
    onTouch();
    player.setIndex(songIndex);
    player.play();
    alarmArmed = false;
  }
  updateLed();
}

void rotate(ESPRotary& r) {
   // Serial.println(r.getPosition());
  encoderValue = r.getPosition() - lastEncoderValue;
  lastEncoderValue = r.getPosition();
  updateEncoderFlag = true;
}

// on left or right rotattion
void showDirection(ESPRotary& r) {
  // Serial.println(r.directionToString(r.getDirection()));
}

void IRAM_ATTR handleLoop(void* arg) {
  r.loop();
}

void setup() {
  Serial.begin(115200);

  SPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
  source.setPath(startFilePath);      // Use the correct setPath() method
  source.begin();                     // No return value, so just call it

  readSettings();
  Serial.println("settings read");
  
  WiFi.begin(ssid.c_str(), password.c_str());
  for (int i = 0; i < reconnectionAttempts; ++i) {
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println(F("WiFi connected."));
      wifiStatus = true;
      Serial.println("WiFi connected");
      break;
    }
    delay(500);
    Serial.print(".");
  }
  if (!wifiStatus) {
    rtc.setTime(Dsec, Dmin, Dhour, Dday, Dmonth, Dyear);  // 17th Jan 2021 15:24:30
  }

  // Init and get the time
  if (wifiStatus) {
    configTzTime(tz.c_str(), ntpServer.c_str());
  // Wait for time to be set
  }
  delay(2000);
  Serial.println("time set");

  display.initR(ScreenType);      // Use INITR_BLACKTAB for 1.8-inch display
  display.setRotation(1);             // Set rotation (adjust if needed)
  display.fillScreen(ST77XX_BLACK);   // Clear screen

  backgroundColour = display.color565(56, 115, 21);
  display.fillRoundRect(0, 0, 160, 130, 10, backgroundColour); 
  Serial.println("display started");

  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);
  Serial.println("debugging began");

  if (!source.getAudioFs().begin(SD_CS_PIN)) {
    Serial.println("[SD] init failed (getAudioFs.begin)");
    while (1) delay(100);
  }
  Serial.println("[SD] OK");
  source.setPath(startFilePath);

  // setup output
  auto cfg = i2s.defaultConfig(TX_MODE);
  cfg.pin_bck = I2S_BCK_PIN;
  cfg.pin_ws = I2S_WS_PIN;
  cfg.pin_data = I2S_DATA_PIN;
  i2s.begin(cfg);
  player.setMetadataCallback(printMetaData);
  player.begin();
  player.setVolume(volume);
  player.setIndex(songIndex);
  
  FastLED.addLeds<LED_TYPE, LED_DIN_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(MaxBrightness);
  Serial.println("LEDS setup");

  IPAdress = WiFi.localIP().toString();
  server.begin();
  Serial.println("server began");

  pinMode(ENC_CLK_PIN, INPUT_PULLUP);
  pinMode(ENC_DT_PIN, INPUT_PULLUP);
  pinMode(ENC_SW_PIN, INPUT_PULLUP);
  pinMode(TFT_BLK_PIN, OUTPUT);

  // Attach interrupt to ENC_CLK_PIN
  // attachInterrupt(digitalPinToInterrupt(ENC_CLK_PIN), handleEncoder, CHANGE);
  // Serial.println("interrupt setup");

  
  r.begin(ENC_CLK_PIN, ENC_DT_PIN, CLICKS_PER_STEP);
  r.setChangedHandler(rotate);
  r.setLeftRotationHandler(showDirection);
  r.setRightRotationHandler(showDirection);
  
  esp_timer_create_args_t timer_args = {
    .callback = &handleLoop,
    .arg = NULL,
    .dispatch_method = ESP_TIMER_TASK,
    .name = "my_timer"
  };

  esp_timer_create(&timer_args, &timer);
  // period is in microseconds → 100000 µs = 100 ms = 0.1 s
  esp_timer_start_periodic(timer, 10000);

  Serial.println(touchRead(TCH_MODE_PIN));
  Serial.println(touchRead(TCH_ADJ_PIN));
  Serial.println(touchRead(TCH_ON_PIN));
  Serial.println(touchRead(TCH_NC_PIN));


}

void loop() {
  //change modes
  if (touchRead(TCH_MODE_PIN) > TouchPinThreshold) {
    Serial.println("mode pin touched");
    onTouch();
    if (mode == "time") {
      mode = "alarm";
    } else if (mode == "alarm") {
      mode = "led";
    } else if (mode == "led") {
      mode = "song";
    } else if (mode == "song") {
      mode = "settings";
    } else if (mode == "settings") {
      mode = "time";
    }
    screenMode = 0;
    display.fillRoundRect(0, 0, 160, 130, 10, backgroundColour); 
    updateDisplay();
    while (touchRead(TCH_MODE_PIN) > TouchPinThreshold) {
    }
  }

  //change screenModes
  if (touchRead(TCH_ADJ_PIN) > TouchPinThreshold) {
    Serial.println("adjust pin touched");
    onTouch();
    ++screenMode;
    if (mode == "time") {
      if (screenMode > 6) {
        screenMode = 0;
      }
    }
    if (mode == "alarm") {
      if (screenMode > 2) {
        screenMode = 0;
      }
    }
    if (mode == "led") {
      if (screenMode > 2) {
        screenMode = 0;
      }
    }
    if (mode == "song") {
      screenMode = 0;
    }
    if (mode == "settings") {
      if (screenMode > 3) {
        screenMode = 0;
      }
    }
    display.fillRoundRect(0, 0, 160, 90, 10, backgroundColour); 
    updateDisplay();
    while (touchRead(TCH_ADJ_PIN) > TouchPinThreshold) {
    }
  }

  //down button
  if (updateEncoderFlag) {
    Serial.println("encoder turned");
    onTouch();
    if (mode == "time" && screenMode) {
      updateTimes();
      if (screenMode == 1) {
        Dhour = nonNegativeModulo(Dhour, 24);
      } else if (screenMode == 2) {
        Dmin = nonNegativeModulo(Dmin, 60);
      } else if (screenMode == 3) {
        Dsec = nonNegativeModulo(Dsec, 60);
      } else if (screenMode == 4) {
        findDayNum();
        Dmonth = nonNegativeModulo(Dmonth, daysInMonth);
      } else if (screenMode == 5) {
        Dday = nonNegativeModulo(Dday, daysInYear);
      } else if (screenMode == 6) {
        Dyear = encoderValue + Dyear;
      }
      rtc.setTime(Dsec, Dmin, Dhour, Dday, Dmonth, Dyear);
      changing = true;
      updateDisplay();
      changing = false;
    }
    //decrease led brightness
    else if (mode == "led") {
      if (screenMode == 1) {
        Hue = nonNegativeModulo(Hue, 256);
        updateLed();
        updateDisplay();
      }
      else if (screenMode == 2) {
        Brightness = nonNegativeModulo(Brightness, 256);
        updateLed();
        updateDisplay();
      }
      updateLed();
      updateDisplay();
    }
    //decrease song
    else if (mode == "song") {
      songIndex = nonNegativeModulo(songIndex, source.size());
      player.setIndex(songIndex);
      updateDisplay();
      alarmArmed = true;
      alarmTime = millis() + 1250;
    }
    //decrease volume
    else if (mode == "settings" && screenMode == 1) {
      volume = nonNegativeModulo(int(volume*10), 11);
      volume = volume/10;
      player.setVolume(volume);
      updateDisplay();
    }
    //decrease contrast
    else if (mode == "settings" && screenMode == 2) {
      contrast = nonNegativeModulo(int(contrast/50), 6);
      contrast = contrast*50;
      changing = true;
      updateDisplay();
      changing = false;
    }
    //decrease alarm time
    if (mode == "alarm") {
      if (screenMode == 1) {
        alarmHour = nonNegativeModulo(alarmHour, 24);
      }
      if (screenMode == 2) {
        alarmMin = nonNegativeModulo(alarmMin, 60);
      }
      changing = true;
      updateDisplay();
      changing = false;
    }
    updateEncoderFlag = false;
    encoderValue = 0;
  }

  //toggle button
  if (touchRead(TCH_ON_PIN) > TouchPinThreshold) {
    Serial.println("on/off pin touched");
    if (alarmOn) {
      player.stop();
      alarmOn = false;
    }else if (mode == "song") {
      player.play();
      alarmOn = true;
    }
    if (mode == "alarm") {
      alarmSet = !alarmSet;
    } else if (mode == "led") {
      ledOn = !ledOn;
    }
    if (mode == "time") {
      if (screenOn) {
        displayOff();
      }else {
        onTouch();
      }
    } else {
      onTouch();
    }
    updateDisplay();
    while (touchRead(TCH_ON_PIN) > TouchPinThreshold);
  }


  //reset time every 5 mins
  if (millis() - lastUpdate > 60000) {
    Serial.println("update");
    // Init and get the time
    Serial.println(WiFi.status());
    if (WiFi.status() == WL_CONNECTED) {
      wifiStatus = true;
    }else {
      wifiStatus = false;
    }
    if (wifiStatus) {
      configTzTime(tz.c_str(), ntpServer.c_str());
    }
    lastUpdate = millis();
    if (!alarmOn) {
      saveSettings();
    }
  }
  //update OLED ten times every sec
  if (millis() - lastDisplay >= displayRefreshRate) {
    updateDisplay();
    lastDisplay = millis();
  }

  //play song if time
  if (alarmOn) {
    if (player.copy())
    player.copy();
    songIndex = source.index();
  }

  if (screenSwitchOff && screenOn && (millis() - lastTouch > screenTimeOut * 60000)) {
    displayOff();
  }
  
  WiFiClient client = server.available();   // listen for incoming clients

  if (client && wifiStatus) {                             // if you get a client,
    Serial.println(F("New Client."));           // print a message out the serial port
    String currentLine = "";                // make a String to hold incoming data from the client
    while (client.connected()) {            // loop while the client's connected
      if (client.available()) {             // if there's bytes to read from the client,
        char c = client.read();             // read a byte, then
        Serial.write(c);                    // print it out the serial monitor
        header += c;
        if (c == '\n') {                    // if the byte is a newline character

          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            
            
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println();
            
            onTouch();
            //set toggle buttons
            if (header.indexOf("?volume=") != -1 && header.indexOf("?volume=") < 20) {
              
              int lastampersandIndex = header.indexOf('&');
              volume = header.substring(header.indexOf('=')+1, lastampersandIndex).toFloat();
              player.setVolume(volume);
              
              lastampersandIndex = header.indexOf('&', lastampersandIndex + 1);
              contrast = header.substring(header.indexOf("contrast=") + 9, lastampersandIndex).toInt();
              
              lastampersandIndex = header.indexOf('&', lastampersandIndex + 1);
              if (songIndex != header.substring(header.indexOf("songNum=") + 8, lastampersandIndex).toInt()) {
                songIndex = header.substring(header.indexOf("songNum=") + 8, lastampersandIndex).toInt();
                player.setIndex(songIndex);
              }
            }
            //set alarm time
            else if (header.indexOf("?alarmHours=") != -1 && header.indexOf("?alarmHours=") < 20) {
              
              int lastampersandIndex = header.indexOf('&');
              alarmHour = header.substring(header.indexOf('=')+1, lastampersandIndex).toInt();
              
              lastampersandIndex = header.indexOf('&', lastampersandIndex + 1);
              alarmMin = header.substring(header.indexOf("alarmMins=") + 10, lastampersandIndex).toInt();
            }
            //set custom colours
            else if (header.indexOf("?CHSV=") != -1 && header.indexOf("?CHSV=") < 20) {
              
              int lastampersandIndex = header.indexOf('&');
              Hue = header.substring(header.indexOf("=") + 1, lastampersandIndex).toInt();
              lastampersandIndex = header.indexOf('&', lastampersandIndex + 1);
              Brightness = header.substring(header.indexOf("Br=") + 3, lastampersandIndex).toInt();
            }
              
            //set settings (checkboxes)
            else if (header.indexOf("?Check=") != -1 && header.indexOf("?Check=") < 20) {
              
              if (header.indexOf("SW=on") != -1 && header.indexOf("SW=on") < 30) {
                screenSwitchOff = true;
              }else {
                screenSwitchOff = false;
              }
              if (header.indexOf("FLED=on") != -1 && header.indexOf("FLED=on") < 36) {
                LEDFadeOn = true;
              }else {
                LEDFadeOn = false;
              }
            }
            else if (header.indexOf("GET /AlarmSet") != -1) {
              alarmSet = !alarmSet;
            }else if (header.indexOf("GET /LED") != -1) {
              ledOn = !ledOn;
            }else if (header.indexOf("GET /AlarmStop") != -1) {
              alarmOn = !alarmOn;     
              if (alarmOn) {
                player.setIndex(songIndex);
                player.play();
              }
            }else if (header.indexOf("GET /Col") != -1) {
              if (header.indexOf("/Cus")-header.indexOf("GET /Col") == 8) {
                LEDMode = "Custom";
              }else if (header.indexOf("/RB")-header.indexOf("GET /Col") == 8) {
                LEDMode = "Rainbow";
              }else if (header.indexOf("/R")-header.indexOf("GET /Col") == 8) {
                LEDMode = "Red";
              }else if (header.indexOf("/G")-header.indexOf("GET /Col") == 8) {
                LEDMode = "Green";
              }else if (header.indexOf("/B")-header.indexOf("GET /Col") == 8) {
                LEDMode = "Blue";
              }else if (header.indexOf("/Y")-header.indexOf("GET /Col") == 8) {
                LEDMode = "Yellow";
              }else if (header.indexOf("/M")-header.indexOf("GET /Col") == 8) {
                LEDMode = "Magenta";
              }else if (header.indexOf("/W")-header.indexOf("GET /Col") == 8) {
                LEDMode = "White";
              }
            }
  
            client.print("\
<!DOCTYPE html>\
<html>\
<head>\
<title>Clock</title>\
<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\
<style type=\"text/css\">\
	input[type=\"submit\"] {\
		border-radius: 5px;\
		color: darkred;\
		margin: 0px 0px 0px 10px;\
	}\
	body {\
		background-color: #cccccc;\
		color: #4a4a4a;\
		font-family: \'open-sans\', sans-serif;\
		font-size: 16px;\
		line-height: 1.2;\
	}\
	h1 {\
		text-align: center;\
		color: #006600;\
		font-size: 50px;\
	}\
	h2 {\
		text-align: center;\
		font-size: 30px;\
	}\
	h3 {\
		margin: 10px 0px;\
	}\
	h4 {\
		font-size: 30px;\
		margin: 30px 0px 0px 0px;\
	}\
	.settings {\
		margin: 0px 0px 0px 680px;\
		text-align: right;\
	}\
	.flex-container {\
		display: flex;\
		flex-wrap: nowrap;\
	}\
	.number-input-span {\
		margin: 0px 10px;\
	}\
	.toggle-button {\
		border-radius: 5px;\
		background-color: #04AA6D;\
		color: black;\
		text-decoration: none;\
		padding: 8px;\
		margin: 8px 6px;\
		font-size: 16px;\
		border: none;\
		cursor: pointer;\
	}\
	.toggle-buttons {\
		margin: 20px;\
	}\
	.toggle-button:hover {\
		background-color: #05c780;\
		padding: 10px;\
		margin: 6px 4px;\
		border: black;\
	}\
	.slider-colour {\
		border-radius: 5px;\
		-webkit-appearance: none;\
		width: 100%;\
		height: 15px;\
		background: linear-gradient(to right,\
		hsl(0, 100%, 50%),    /* Red */\
		hsl(60, 100%, 50%),   /* Yellow */\
		hsl(120, 100%, 50%),  /* Green */\
		hsl(180, 100%, 50%),  /* Cyan */\
		hsl(240, 100%, 50%),  /* Blue */\
		hsl(300, 100%, 50%),  /* Magenta */\
		hsl(360, 100%, 50%)); /* Red */\
		outline: none;\
		opacity: 0.7;\
		-webkit-transition: .2s;\
		transition: opacity .2s;\
	}\
\
	.slider-colour:hover {\
		opacity: 1;\
	}\
	\
	.slider-colour::-webkit-slider-thumb {\
		border-radius: 15px;\
		-webkit-appearance: none;\
		appearance: none;\
		width: 25px;\
		height: 25px;\
		background: hsl(var(--hue, "+String(Hue*360/255)+"), 100%, 50%); /* Default color: cyan */\
		cursor: pointer;\
		opacity: 1;\
		border: 3px solid black;\
	}\
\
	.slider-colour::-moz-range-thumb {\
		width: 25px;\
		height: 25px;\
		background: hsl(var(--hue, "+String(Hue*360/255)+"), 100%, 50%); /* Default color: cyan */\
		cursor: pointer;\
	}\
	.slider-brightness {\
		border-radius: 5px;\
		-webkit-appearance: none;\
		width: 100%;\
		height: 15px;\
		background: linear-gradient(to right, hsl(0, 0%, 0%), hsl(0, 0%, 100%));\
		outline: none;\
		opacity: 0.7;\
		-webkit-transition: .2s;\
		transition: opacity .2s;\
	}\
\
	.slider-brightness:hover {\
		opacity: 1;\
	}\
	\
	.slider-brightness::-webkit-slider-thumb {\
		border-radius: 15px;\
		-webkit-appearance: none;\
		appearance: none;\
		width: 25px;\
		height: 25px;\
		background: hsl(0, 0%, var(--brightness, "+String(Brightness*100/255)+"%)); /* Default color: cyan */\
		cursor: pointer;\
		opacity: 1;\
		border: 3px solid grey;\
	}\
\
	.slider-brightness::-moz-range-thumb {\
		width: 25px;\
		height: 25px;\
		background: hsl(0, 0%, var(--brightness, "+String(Brightness*100/255)+"%)); /* Default color: cyan */\
		cursor: pointer;\
	}\
	.left-elements {\
		margin: 0px 0px 0px 20px;\
	}\
	.dropbtn {\
		background-color: #04AA6D;\
		color: white;\
		padding: 16px;\
		font-size: 16px;\
		border: none;\
	}\
	.dropdown-div {\
		margin: 0px 0px 0px 50px;\
	}\
	.dropdown {\
		position: relative;\
		display: inline-block;\
		font-size: 20px;\
		min-width: 120px;\
		min-height: 40px;\
		text-align: left;\
	}\
	.dropdown-content {\
		display: none;\
		position: absolute;\
		background-color: #f1f1f1;\
		min-width: 120px;\
		box-shadow: 0px 8px 16px 0px rgba(0,0,0,0.2);\
		z-index: 1;\
	}\
	.dropdown-content a {\
		color: black;\
		padding: 12px 16px;\
		text-decoration: none;\
		display: block;\
	}\
	.dropdown-content a:hover {\
		background-color: #ddd;\
	}\
	.dropdown:hover .dropdown-content {\
		display: block;\
	}\
	.dropdown:hover \
	.dropbtn{\
		background-color: #3e8e41;\
	}\
	.settings-switch {\
\
	}\
	.hidden-checkbox {\
		display: none;\
	}\
</style>\
</head>\
<body>\
	<h1>\
		Welcome To the Alarm Clock\'s Web Page!\
	</h1>\
	<h2>\
		"+rtc.getTimeDate(true)+"\
	</h2>\
	<div class=\"flex-container\">\
		<div>\
			<div class=\"toggle-buttons\">\
				<a href=\"/AlarmSet\" class=\"toggle-button\">\
					Alarm "+returnOnOff(alarmSet)+"\
				</a>\
				<a href=\"/LED\" class=\"toggle-button\">\
					LED "+returnOnOff(ledOn)+"\
				</a>\
				<a href=\"/AlarmStop\" class=\"toggle-button\">\
					Alarm "+returnRinging(alarmOn)+"\
				</a>\
			</div>\
			<form method=\"get\">\
				<div class=\"left-elements\">\
					<div class=\"flex-container\">\
						<span class=\"number-input-span\">\
							<h3 name=\'number-input-header\'>Volume</h3>\
							<input type=\"number\" id=\"volume\" name=\"volume\" min=\"0\" max=\"1\" step=\"0.1\" value="+volume+">\
						</span>\
						<span class=\"number-input-span\">\
							<h3 name=\'number-input-header\'>Contrast</h3>\
							<input type=\"number\" id=\"contrast\" name=\"contrast\" min=\"0\" max=\"250\" step=\"50\" value="+contrast+">\
						</span>\
						<span class=\"number-input-span\">\
							<h3 name=\'number-input-header\'>Song Number</h3>\
							<input type=\"number\" id=\"songNum\" name=\"songNum\" min=\"0\" max=\"100\" step=\"1\" value="+songIndex+">\
						</span>\
					</div>\
					<input type=\"submit\">\
				</div>\
			</form>\
			<form method=\"get\">\
				<div class=\"left-elements\">\
					<h4>Alarm Time</h4>\
					<input type=\"number\" id=\"alarmHours\" name=\"alarmHours\" min=\"0\" max=\"23\" step=\"1\" value="+alarmHour+">:\
					<input type=\"number\" id=\"alarmMins\" name=\"alarmMins\" min=\"0\" max=\"59\" step=\"1\" value="+alarmMin+">\
					<input type=\"submit\">\
				</div>\
			</form>");
if (LEDMode == "Custom") {
  client.print("\
			<form method=\"get\">\
				<div class=\"left-elements\">\
					<h4>Custom Colour</h4>\
						<input class=\"slider-colour\" type=\"range\" min=\"1\" max=\"255\" value=\""+String(Hue)+"\" name=\"CHSV\"  id=\"ColIn\">\
					<h3>Brightness</h3>\
						<input class=\"slider-brightness\" type=\"range\" min=\"1\" max=\"255\" value=\""+String(Brightness)+"\" name=\"Br\"  id=\"BrtIn\">\
					<input type=\"submit\">\
				</div>\
			</form>");
}
client.print("\
		</div>\
		<div class=\"dropdown-div\">\
			<div class=\"dropdown\">\
				<button class=\"dropdown\">\
					"+LEDMode+"\
				</button>\
				<div class=\"dropdown-content\">\
					<a href=\"/Col/Cus\">\
						Custom\
					</a>\
					<a href=\"/Col/R\">\
						Red\
					</a>\
					<a href=\"/Col/G\">\
						Green\
					</a>\
					<a href=\"/Col/B\">\
						Blue\
					</a>\
					<a href=\"/Col/Y\">\
						Yellow\
					</a>\
					<a href=\"/Col/M\">\
						Magenta\
					</a>\
					<a href=\"/Col/W\">\
						White\
					</a>\
					<a href=\"/Col/RB\">\
						Rainbow\
					</a>\
				</div>\
			</div>\
		</div>\
		<div class=\"settings\">\
			<form method=\"get\">\
				<input type=\"checkbox\" class=\"hidden-checkbox\" name=\"Check\" checked>\
				<div class=\"switch\">\
					<label>\
						Screen Switchs off after 1 min\
					</label>\
					<input type=\"checkbox\" class=\"settings-switch\" name=\"SW\""+returnChecked(screenSwitchOff)+">\
					<span class=\"slider-round\"></span>\
				</div>\
				<div class=\"switch\">\
					<label>\
						LED slowly turns on as alarm aproaches\
					</label>\
					<input type=\"checkbox\" class=\"settings-switch\" name=\"FLED\" "+returnChecked(LEDFadeOn)+">\
					<span class=\"slider-round\"></span>\
				</div>\
				<input type=\"submit\" text=\"Apply\">\
			</form>\
		</div>\
	</div>\
	<script>\
		document.getElementById(\'ColIn\').addEventListener(\'input\', function() {\
			var hue = this.value * 3.6/2.55;\
			this.style.setProperty(\'--hue\', hue);\
		});\
		document.getElementById(\'BrtIn\').addEventListener(\'input\', function() {\
			var brightness = this.value/2.55 + \"%\";\
			this.style.setProperty(\'--brightness\', brightness);\
		});\
	</script>\
</body>\
</html>\
");
            // The HTTP response ends with another blank line:
            client.println();
            // break out of the while loop:
            break;
          } else {    // if you got a newline, then clear currentLine:
            currentLine = "";
          }
        } else if (c != '\r') {  // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }
      }
    }
    header = "";
    // close the connection:
    client.stop();
    Serial.println(F("Client Disconnected."));
  }
  r.loop();
  if (!digitalRead(ENC_SW_PIN)) {
    ESP.restart();
  }
}//Sketch uses 1122801 bytes (85%)
 //Sketch uses 1432989 bytes (45%) of program storage space.

String returnOnOff(bool val) {
  if (val) {
    return "On";
  }else {
    return "Off";
  }
}
String returnRinging(bool val) {
  if (val) {
    return "Ringing";
  }else {
    return "Off";
  }
}
String returnChecked(bool val) {
  if (val) {
    return "checked";
  }else {
    return ""
