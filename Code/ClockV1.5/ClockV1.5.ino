#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <Arduino.h>
#include <ESP32Time.h>
#include <AudioTools.h>
#include <AudioLibs/AudioSourceSDFAT.h>
#include <AudioCodecs/CodecMP3Helix.h>
#include <FastLED.h>

#define BuiltInLED 1

// OLED display width and height, for a typical SSD1306 OLED
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET    -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#if BuiltInLED
  #define MaxBrightness 255
  #define LEDPin         5
  #define NUM_LEDS       1
  #define LED_TYPE  WS2812
#else
  #define MaxBrightness 100
  #define LEDPin        16
  #define NUM_LEDS      14
  #define LED_TYPE  WS2813
#endif

#define changeModePin T6
#define upPin         T5
#define downPin       T3
#define togglePin     T4
#define screenModePin T0
#define screenTimeOut  1   //in minutes

#define OLEDAddress 0x3C // I2C address for oled display

// Timezone offset in seconds (e.g., -5 * 3600 for EST)
#define  gmtOffset_sec 0

// Daylight saving offset (in seconds)
#define daylightOffset_sec 3600
// #define connectionFrequency 10 //120 * number of mins between wifi reconnects
#define reconnectionAttempts 3
#define DISPLAY_RATE 100
#define UPDATE_RATE 5000


// Replace with your network credentials
const char* ssid     =  "IrishJs-2.4g";
const char* password =     "11IrishJs";
const char* startFilePath =        "/";
const char* ext =                "mp3";



bool wifiStatus = false;      //whether it is connected to wifi or not
bool ledOn = false;           //whether the led is set to be on or off
bool alarmOn = false;         //whether the alarm is sounding or not
bool alarmSet = true;         //whether the alarm is set or not
bool changing = false;        //flag for stopping the time from blinking while you increase or decrease the mins or hours
bool twelveHour = false;      //whether twelve hour mode or not
bool screenOn = true;         //whether the screen is switched on or not
bool screenSwitchOff = true; //whether the screen will switch off after 1 minute or not
bool LEDFadeOn = false;      //whether the light will fade on as the alarm time approaches or not
String mode = "time";         //mode of display ("time" for normal mode or "alarm" to set the alarm)
String LEDMode = "Rainbow";
String IPAdress = "";
String header   = "";
int screenMode =   0;         //which submode you are on
//default times
int Dsec =         0;
int Dmin =         0;
int Dhour =        0;
int Dday =         1;
int Dmonth =       1;
int Dyear =     2024;
//alarm times
int alarmHour =    6;
int alarmMin =    45;
//last time touch
int lastTouch = millis();
int lastDisplay = millis();
int lastUpdate = millis();
//colours
int Hue =        255;
int Brightness = 255;
//brightness of screen (0-250)
int contrast =   250;
//volume (0-1)
float volume =  0.50;
//number of days in current month
int daysInMonth =   31;
//number of current song
int songIndex =      0;
//time for led to fade on before alarm
int ledFadeTime =   30;//in mins
int currentRainbowColour = 0;


//setup led strip object
CRGB leds[NUM_LEDS];

//configure audio
AudioSourceSDFAT source(startFilePath, ext);
I2SStream i2s;
MP3DecoderHelix decoder;
AudioPlayer player(source, i2s, decoder);

//setup display
Adafruit_SH1107 display = Adafruit_SH1107(SCREEN_HEIGHT, SCREEN_WIDTH, &Wire, OLED_RESET, 1000000, 100000);

//ESP32Time rtc;
ESP32Time rtc(0);

//set up wifi server
WiFiServer server(80);

//prints sd card info
void printMetaData(MetaDataType type, const char* str, int len) {
  Serial.print("==> ");
  Serial.print(toStr(type));
  Serial.print(": ");
  Serial.println(str);
}


//find number of days in current month
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
}

//called whenever a button is pressed
void onTouch() {
  screenOn = true;
  lastTouch = millis();
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
    // Serial.print("Rainbow colour: ");
    // Serial.println(currentRainbowColour);
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

//updates display
void updateOLED() {
  //print time and date
  // Serial.println(rtc.getTimeDate(true));

  //clear the display
  display.clearDisplay();
  display.setCursor(0, 0);
  display.setContrast(contrast);

  //display on time mode
  if (screenOn) {
    if (mode == "time") {
      display.setTextSize(2);
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
      display.setCursor(0, 30);
      display.setTextSize(2);
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
      }
      if ((!(int(millis() / 500) % 2 == 0 && screenMode == 6) || changing) && screenMode == 6) {
        display.setCursor(0, 30);
        display.setTextSize(2);
        display.print(rtc.getYear());
      }
    }

    //display on alarm mode
    else if (mode == "alarm") {
      display.setTextSize(3);
      if (!(int(millis() / 500) % 2 == 0 && screenMode == 1) || changing) {
        if (alarmHour < 10) {
          display.print("0");
          display.print(alarmHour);
        } else {
          display.print(alarmHour);
        }
      } else {
        display.setCursor(36, 0);
      }
      display.print(":");
      if (!(int(millis() / 500) % 2 == 0 && screenMode == 2) || changing) {
        if (alarmMin < 10) {
          display.print("0");
          display.print(alarmMin);
        } else {
          display.print(alarmMin);
        }
      }
    }
    //display on led mode
    else if (mode == "led") {
      display.setTextSize(2);
      if (screenMode == 1 || screenMode == 0) {
        display.print("Hue:");
        display.println(Hue);
      }else if (screenMode == 2 || screenMode == 0) {
        display.print("\nBrightness:\n");
        display.println(Brightness);
      }
    } else if (mode == "song") {
      display.setTextSize(4);
      display.print(songIndex);
    } else if (mode == "settings") {
      display.setTextSize(2);
      if (screenMode == 0) {
        display.print("Settings");
      } else if (screenMode == 1) {
        display.print("Vol: ");
        display.println(volume);
      }else if (screenMode == 2) {
        display.setTextSize(2);
        display.println("contrast\n");
        display.setTextSize(1);
        if (contrast == 250) {
          display.print("Very Bright");
        }else if (contrast == 200) {
          display.print("Meduim Bright");
        }else if (contrast == 150) {
          display.print("Slightly Bright");
        }else if (contrast == 100) {
          display.print("Slightly Dim");
        }else if (contrast == 50) {
          display.print("Meduim Dim");
        }else if (contrast == 0) {
          display.print("Very Dim");
        }
      } else if (screenMode == 3) {
        display.setTextSize(2);
        display.println("IP Adress:");
        display.setTextSize(1.5);
        display.print(IPAdress);
      }
    }
    //display A if alarm set
    if (alarmSet) {
      display.setTextSize(1);
      display.setCursor(0, 50);
      display.print("A");
    }
    //display LED if led on
    if (ledOn) {
      display.setTextSize(1);
      display.setCursor(10, 50);
      display.print("LED ");
    }
    //display WiFi if WiFi connected
    if (wifiStatus) {
      display.setTextSize(1);
      display.setCursor(40, 50);
      display.print("WiFi");
    }
  }
  //show the changes
  display.display();

  if (rtc.getHour(true) == alarmHour && rtc.getMinute() == alarmMin && !rtc.getSecond() && alarmSet && !alarmOn) {
    alarmOn = true;
    if (LEDFadeOn) {
      ledOn = true;
    }
    onTouch();
    player.setIndex(songIndex);
    player.play();
    // Serial.print("Alarm, song number ");
    // Serial.println(songIndex);
  }
  updateLed();
}

void setup() {
  Serial.begin(115200);


  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  for (int i = 0; i < reconnectionAttempts; ++i) {
    if (WiFi.status() == WL_CONNECTED) {
      // Serial.println(F("WiFi connected."));
      wifiStatus = true;
      break;
    }
    delay(500);
    // Serial.print(".");
  }
  if (!wifiStatus) {
    // Serial.println(F("could not connect to wifi."));
    rtc.setTime(Dsec, Dmin, Dhour, Dday, Dmonth, Dyear);  // 17th Jan 2021 15:24:30
  }

  // Init and get the time
  if (wifiStatus) {
    configTime(gmtOffset_sec, daylightOffset_sec, "pool.ntp.org", "time.nist.gov");
  }

  // Wait for time to be set
  delay(2000);

  // Initialize OLED display
  if (!display.begin(0x3C, true)) { // Check your OLED address, usually 0x3C or 0x3D
    // Serial.println(F("SH1107 allocation failed"));
    for (;;);
  }
  display.clearDisplay();
  display.setTextColor(SH110X_WHITE);
  display.setContrast(contrast);
  display.setRotation(1);
  display.clearDisplay();
  display.display();
  

  //start audio
  AudioLogger::instance().begin(Serial, AudioLogger::Info);
  // setup output
  auto cfg = i2s.defaultConfig(TX_MODE);
  cfg.pin_bck = 26;
  cfg.pin_ws = 25;
  cfg.pin_data = 17;
  i2s.begin(cfg);
  player.setMetadataCallback(printMetaData);
  player.begin();
  player.setVolume(volume);
  
  FastLED.addLeds<LED_TYPE, LEDPin, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(MaxBrightness);

  IPAdress = WiFi.localIP().toString();
  server.begin();
  
  delay(2000);
}

void loop() {
  //change modes
  if (touchRead(changeModePin) <= 40) {
    // Serial.println("mode button pressed");
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
    updateOLED();
    // Serial.print("mode: ");
    // Serial.print(mode);
    while (touchRead(changeModePin) <= 40) {
      // Serial.println(touchRead(changeModePin));
    }
  }

  //change screenModes
  if (touchRead(screenModePin) <= 40) {
    // Serial.println("screenMode button pressed");
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
    updateOLED();
    while (touchRead(screenModePin) <= 40) {
      // Serial.println(touchRead(screenModePin));
    }
  }

  //up button
  if (touchRead(upPin) <= 40) {
    // Serial.println("up button pressed");
    onTouch();
    if (mode == "time") {
      updateTimes();
      if (!screenMode) {

        //toggle twelve hour time mode
        twelveHour = !twelveHour;
      } else {

        //increase time
        if (Dhour < 23 && screenMode == 1) {
          ++Dhour;
        } else if (Dmin < 59 && screenMode == 2) {
          ++Dmin;
        } else if (screenMode == 3) {
          Dsec = 0;
        } else if (Dmonth < 12 && screenMode == 4) {
          ++Dmonth;
          if (Dday > daysInMonth) {
            Dday = daysInMonth;
          }
        } else if (Dday < daysInMonth && screenMode == 5) {
          ++Dday;
        } else if (Dyear < 2100 && screenMode == 6) {
          ++Dyear;
        }
        rtc.setTime(Dsec, Dmin, Dhour, Dday, Dmonth, Dyear);
        changing = true;
        updateOLED();
        changing = false;
        for (int i = 0; i < 200; ++i) {
          if (touchRead(upPin) > 40) {
            break;
          }
          delay(5);
        }
        while (touchRead(upPin) <= 40) {
          updateTimes();
          if (Dhour < 23 && screenMode == 1) {
            ++Dhour;
          }
          else if (Dmin < 59 && screenMode == 2) {
            ++Dmin;
          }
          else if (screenMode == 3) {
            Dsec = 0;
          }
          else if (Dmonth < 12 && screenMode == 4) {
            ++Dmonth;
            findDayNum();
            if (Dday > daysInMonth) {
              Dday = daysInMonth;
            }
          }
          else if (Dday < daysInMonth && screenMode == 5) {
            ++Dday;
          }
          else if (Dyear < 2104 && screenMode == 6) {
            ++Dyear;
          }
          rtc.setTime(Dsec, Dmin, Dhour, Dday, Dmonth, Dyear);
          changing = true;
          updateOLED();
          changing = false;
          delay(100);
        }
      }
      while (touchRead(upPin) <= 40);
    }else if (mode == "alarm") {

      //increase alarm time
      if (alarmHour < 23 && screenMode == 1) {
        ++alarmHour;
      }
      if (alarmMin < 59 && screenMode == 2) {
        ++alarmMin;
      }
      changing = true;
      updateOLED();
      changing = false;
      for (int i = 0; i < 200; ++i) {
        if (touchRead(upPin) > 40) {
          break;
        }
        delay(5);
      }
      while (touchRead(upPin) <= 40) {
        if (alarmHour < 23 && screenMode == 1) {
          ++alarmHour;
        }
        if (alarmMin < 59 && screenMode == 2) {
          ++alarmMin;
        }
        changing = true;
        updateOLED();
        changing = false;
        delay(100);
      }
    }else if (mode == "led") {
      if (screenMode == 1) {
          //change led colour
        if (Hue < 255) {
          ++Hue;
        }
        updateLed();
        updateOLED();
        for (int i = 0; i < 200; ++i) {
          if (touchRead(upPin) > 40) {
            break;
          }
          delay(5);
        }
        while (touchRead(upPin) <= 40) {
          if (Hue < 255) {
            ++Hue;
          }
        }
      }
      if (screenMode == 2) {
          //increase led brightness
        if (Brightness < 255) {
          ++Brightness;
        }
        updateLed();
        updateOLED();
        for (int i = 0; i < 200; ++i) {
          if (touchRead(upPin) > 40) {
            break;
          }
          delay(5);
        }
        while (touchRead(upPin) <= 40) {
          if (Brightness < 255) {
            ++Brightness;
          }
        }
      }
      updateLed();
      updateOLED();
      //increase song number
    }else if (mode == "song") {
      ++songIndex;
      player.setIndex(songIndex);
      player.play();
      alarmOn = true;
      updateOLED();
      while (touchRead(upPin) <= 40);
    }else if (mode == "settings") {
      if (screenMode == 1) {
        //increase volume
        if (volume < 1) {
          volume += .10;
          player.setVolume(volume);
        }
      }else if (screenMode == 2) {
        //increase contrast
        if (contrast < 201) {
            contrast += 50;
        }
        changing = true;
        updateOLED();
        changing = false;
      }
      // Serial.println("updating");
      updateOLED();
      // Serial.println("updated");
      while (touchRead(upPin) <= 40);
    }
  }

  //down button
  if (touchRead(downPin) <= 40) {
    // Serial.println("down button pressed");
    onTouch();
    if (mode == "time" && screenMode) {
      updateTimes();
      if (Dhour > 0 && screenMode == 1) {
        --Dhour;
      } else if (Dmin > 0 && screenMode == 2) {
        --Dmin;
      } else if (Dmonth > 1 && screenMode == 4) {
        --Dmonth;
        findDayNum();
        if (Dday > daysInMonth) {
          Dday = daysInMonth;
        }
      } else if (Dday > 0 && screenMode == 5) {
        --Dday;
      } else if (Dyear > 0 && screenMode == 6) {
        --Dyear;
      }
      rtc.setTime(Dsec, Dmin, Dhour, Dday, Dmonth, Dyear);
      changing = true;
      updateOLED();
      changing = false;
      for (int i = 0; i < 200; ++i) {
        if (touchRead(downPin) > 40) {
          break;
        }
        delay(5);
      }
      while (touchRead(downPin) <= 40) {
        updateTimes();
        if (Dhour > 0 && screenMode == 1) {
          --Dhour;
        } else if (Dmin > 0 && screenMode == 2) {
          --Dmin;
        } else if (Dmonth > 1 && screenMode == 4) {
          --Dmonth;
          findDayNum();
          if (Dday > daysInMonth) {
            Dday = daysInMonth;
          }
        } else if (Dday > 0 && screenMode == 5) {
          --Dday;
        } else if (Dyear > 1968 && screenMode == 6) {
          --Dyear;
        }
        rtc.setTime(Dsec, Dmin, Dhour, Dday, Dmonth, Dyear);
        changing = true;
        updateOLED();
        changing = false;
        delay(100);
      }
    }

    //decrease led brightness
    else if (mode == "led") {
      if (screenMode == 1) {
        if (Hue > 0) {
          --Hue;
        }
        updateLed();
        updateOLED();
        for (int i = 0; i < 200; ++i) {
          if (touchRead(downPin) > 40) {
            break;
          }
          delay(5);
        }
        while (touchRead(downPin) <= 40) {
          if (Hue > 0) {
            --Hue;
          }
        }
      }
      else if (screenMode == 2) {
        if (Brightness > 0) {
          --Brightness;
        }
        updateLed();
        updateOLED();
        for (int i = 0; i < 200; ++i) {
          if (touchRead(downPin) > 40) {
            break;
          }
          delay(5);
        }
        while (touchRead(downPin) <= 40) {
          if (Brightness > 0) {
            --Brightness;
          }
        }
      }
      updateLed();
      updateOLED();
    }

    //decrease song
    else if (mode == "song") {
      if (songIndex > 0) {
        --songIndex;
      }
      player.setIndex(songIndex);
      player.play();
      alarmOn = true;
      updateOLED();
      while (touchRead(downPin) <= 40);
    }

    //decrease volume
    else if (mode == "settings" && screenMode == 1) {
      if (volume > 0) {
        volume -= .10;
        if (volume < .10) {
          volume = 0;
        }
        player.setVolume(volume);
      }
      updateOLED();
      while (touchRead(downPin) <= 40);
    }

    //decrease contrast
    else if (mode == "settings" && screenMode == 2) {
      if (contrast > 49) {
        contrast -= 50;
      }
      changing = true;
      updateOLED();
      changing = false;
      while (touchRead(downPin) <= 40);
    }

    //decrease alarm time
    if (mode == "alarm") {
      if (alarmHour > 0 && screenMode == 1) {
        --alarmHour;
      }
      if (alarmMin > 0 && screenMode == 2) {
        --alarmMin;
      }
      changing = true;
      updateOLED();
      changing = false;
      for (int i = 0; i < 200; ++i) {
        if (touchRead(downPin) > 40) {
          break;
        }
        delay(5);
      }
      while (touchRead(downPin) <= 40) {
        if (alarmHour > 0 && screenMode == 1) {
          --alarmHour;
        }
        if (alarmMin > 0 && screenMode == 2) {
          --alarmMin;
        }
        changing = true;
        updateOLED();
        changing = false;
        delay(100);
      }
    }
  }

  //toggle button
  if (touchRead(togglePin) <= 40) {
    // Serial.println("toggle button pressed");

    if (alarmOn) {
      player.stop();
      alarmOn = false;
    }
    if (mode == "alarm") {
      alarmSet = !alarmSet;
    } else if (mode == "led") {
      ledOn = !ledOn;
    }
    if (mode == "time") {
      screenOn = !screenOn;
      lastTouch = millis();
    } else {
      onTouch();
    }
    updateOLED();
    while (touchRead(togglePin) <= 40);
  }


  //reset time every 5secs
  if (millis() - lastUpdate > 5000) {
    // Init and get the time
    if (WiFi.status() == WL_CONNECTED) {
      wifiStatus = true;
    }else {
      wifiStatus = false;
    }
    if (wifiStatus) {
      configTime(gmtOffset_sec, daylightOffset_sec, "pool.ntp.org", "time.nist.gov");
      // Serial.print("updated time to ");
    }
    lastUpdate = millis();
  }
  //update OLED twice every sec
  if (millis() - lastDisplay >= 100) {
    updateOLED();
    lastDisplay = millis();
  }

  //play song if time
  if (alarmOn) {
    player.copy();
  }

  if (screenSwitchOff && screenOn && (millis() - lastTouch > screenTimeOut * 60000)) {
    screenOn = false;
    // Serial.println(lastTouch);
  }
  
  WiFiClient client = server.available();   // listen for incoming clients

  if (client && wifiStatus) {                             // if you get a client,
    // Serial.println(F("New Client."));           // print a message out the serial port
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
            // Serial.println(F("received line"));
            // Serial.println(header);
            // Serial.println(F("end of header"));
            // Serial.println(header.indexOf("?volume="));
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
              Hue = header.substring(header.indexOf("=") + 1, header.indexOf('&')).toInt();
            }
            //set settings (checkboxes)
            else if (header.indexOf("?Check=") != -1 && header.indexOf("?Check=") < 20) {
              
              if (header.indexOf("SW=on") != -1 && header.indexOf("SW=on") < 30) {
                screenSwitchOff = true;
              }else {
                screenSwitchOff = false;
              }
              if (header.indexOf("FLED=on") != -1 && header.indexOf("FLED=on") < 30) {
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
  
            // the content of the HTTP response follows the header:<button onclick="this.style.background = 'yellow'">I feel boring... please click me.</button>
            client.print("\
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
.slider {\
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
.slider:hover {\
opacity: 1;\
}\
\
.slider::-webkit-slider-thumb {\
border-radius: 15px;\
-webkit-appearance: none;\
appearance: none;\
width: 25px;\
height: 25px;\
background: hsl(var(--hue, "+String(Hue*360/255)+"), 100%, 50%);\
cursor: pointer;\
opacity: 1;\
border: 2px solid gray;\
}\
\
.slider::-moz-range-thumb {\
width: 25px;\
height: 25px;\
background: hsl(var(--hue, "+String(Hue*360/255)+"), 100%, 50%);\
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
.dropdown:hover\
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
  </form>\
  <form method=\"get\">\
  <div class=\"left-elements\">");
if (LEDMode == "Custom") {
  client.print("\
  <h4>Custom Colour</h4>\
  <input class=\"slider\" type=\"range\" min=\"1\" max=\"255\" value="+String(Hue)+" name=\"CHSV\"  id=\"myRange\">\
  <input type=\"submit\">");
  
}
client.print("\
</div>\
</form>\
</div>\
<div class=\"dropdown-div\">\
<h3> LED Mode</h3>\
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
<input type=\"checkbox\" class=\"settings-switch\" name=\"SW\" "+returnChecked(screenSwitchOff)+">\
<span class=\"slider-round\"></span>\
</div>\
<div class=\"switch\">\
<label>\
LED slowly turns on as alarm aproaches\
</label>\
<input type=\"checkbox\" class=\"settings-switch\" name=\"FLED\" "+returnChecked(LEDFadeOn)+">\
<span class=\"slider-round\"></span>\
</div>\
<input type=\"submit\">\
</form>\
</div>\
</div>\
<script>\
document.getElementById(\'myRange\').addEventListener(\'input\', function() {\
var hue = this.value * 3.6/2.55;\
this.style.setProperty(\'--hue\', hue);\
});\
</script>\
</body>\
</html>");
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
    // Serial.println(F("Client Disconnected."));
  }
}//Sketch uses 1122801 bytes (85%)

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
    return "";
  }
}