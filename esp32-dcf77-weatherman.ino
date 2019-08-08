#include "build.h"
#include "DCFDateTime.h"
#include "dcf77.h"
#include "rtc.h"

#define FS_NO_GLOBALS
#include <FS.h>

// JPEG decoder library
#include <JPEGDecoder.h>
#include "img.h"

#include <Adafruit_GFX.h>                                  // Core graphics library
#include <Adafruit_ST7735.h>                               // Hardware-specific library

//#include <mySD.h>



//#include <ssd1327.h> // https://github.com/bitbank2/ssd1327 ( also requires: https://github.com/bitbank2/BitBang_I2C )
//#include <U8g2lib.h>
//U8G2_SSD1327_EA_W128128_F_SW_I2C display(U8G2_R0, /* cs=*/ 32, /* dc=*/ 25, /* reset=*/ U8X8_PIN_NONE);

#define DEBUG 1

// DCF77 from https://www.tindie.com/products/universalsolder/atomic-clock-am-receiver-kit-dcf77-wwvb-msf-jjy60/

/**
// DEV MODULE PINOUT
#define RADIO_DIGITAL_PIN 14    // OUT
#define RADIO_POWERDOWN_PIN 26  // PDN
#define RADIO_AUTOGAIN_PIN 33   // AON
*/

// TTGO MODULE PINOUT
#define RADIO_DIGITAL_PIN 21    // OUT
#define RADIO_POWERDOWN_PIN 26  // PDN
#define RADIO_AUTOGAIN_PIN 32   // AON

#define RTC_SDA 33
#define RTC_SCL 23

#define TFT_CS    16
#define TFT_A0    17
#define TFT_SDA   23
#define TFT_SCK    5
#define TFT_RESET  9

#define LED_BUILTIN 22
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_A0, TFT_SDA, TFT_SCK, TFT_RESET);

BLE_RTC_DS1307 rtc;

Dcf77    mDcf;
DCFDateTime mNow;


bool mDcfStatus = false;

uint8_t mActiveDisplay = 0;

int fourdayforecast = 0;
int twodayforecast = 0;
int significantweather[60] = {0};

char mPrevMinute = 0;
char degreeSign[] = {
  /* ° */
  0xC2,  0xB0,  0
};
char clockSign[] = {
  /* ⏰ */
  0xE2,  0x8F,  0xB0,  0
};

String meteodata;
String weathermemory[60];
String town[]          = {"F Bordeaux", "F la Rochelle", "F Paris", "F Brest", "F Clermont-Ferrand", "F Beziers", "B Bruxelles", "F Dijon", "F Marseille", "F Lyon", "F Grenoble", "CH La Chaux de Fonds",
                         "D Frankfurt am Main", "D Trier", "D Duisburg", "GB Swansea", "GB Manchester", "F le Havre", "GB London", "D Bremerhaven", "DK Herning", "DK Arhus", "D Hannover", "DK Copenhagen", "D Rostock",
                         "D Ingolstadt", "D Muenchen", "I Bolzano", "D Nuernberg", "D Leipzig", "D Erfurt", "CH Lausanne", "CH Zuerich", "CH Adelboden", "CH Sion", "CH Glarus", "CH Davos", "D Kassel", "CH Locarno", "I Sestriere",
                         "I Milano", "I Roma", "NL Amsterdam", "I Genova", "I Venezia", "D Strasbourg", "A Klagenfurt", "A Innsbruck", "A Salzburg", "A/SK Wien/Bratislava", "CZ Praha", "CZ Decin", "D Berlin", "S Gothenburg",
                         "S Stockholm", "S Kalmar", "S Joenkoeping", "D Donaueschingen", "N Oslo", "D Stuttgart",
                         "I Napoli", "I Ancona", "I Bari", "H Budapest", "E Madrid", "E Bilbao", "I Palermo", "E Palma de Mallorca",
                         "E Valencia", "E Barcelona", "AND Andorra", "E Sevilla", "P Lissabon", "I Sassari", "E Gijon", "IRL Galway", "IRL Dublin", "GB Glasgow", "N Stavanger", "N Trondheim", "S Sundsvall", "PL Gdansk",
                         "PL Warszawa", "PL Krakow", "S Umea", "S Oestersund", "CH Samedan", "HR Zagreb", "CH Zermatt", "HR Split" };
String weather[]       = {"Reserved", "Sunny", "Partly clouded", "Mostly clouded", "Overcast", "Heat storms", "Heavy rain", "Snow", "Fog", "Sleet", "Rain showers", "light rain",
                         "Snow showers", "Frontal storms", "Stratus cloud", "Sleet storms" };
String heavyweather[]  = {"None", "Heavy Weather 24 hrs.", "Heavy weather Day", "Heavy weather Night", "Storm 24hrs.", "Storm Day", "Storm Night",
                         "Wind gusts Day", "Wind gusts Night", "Icy rain morning", "Icy rain evening", "Icy rain night", "Fine dust", "Ozon", "Radiation", "High water" };
String probprecip[]    = {"0 %", "15 %", "30 %", "45 %", "60 %", "75 %", "90 %", "100 %"};
String winddirection[] = {"N", "NO", "O", "SO", "S", "SW", "W", "NW", "Changeable", "Foen", "Biese N/O", "Mistral N", "Scirocco S", "Tramont W", "reserved", "reserved" };
String windstrength[]  = { "0", "0-2", "3-4", "5-6", "7", "8", "9", ">=10"};
String anomaly1[]      = {"Same Weather ", "Jump 1 ", "Jump 2 ", "Jump 3 "};
String anomaly2[]      = {"0-2 hrs", "2-4 hrs", "5-6 hrs", "7-8 hrs"};



int weatherunbinary(int a, int b) {
  int val = 0;
  for (int i = 0; i < 16; i++) {
    if (! weathermemory[fourdayforecast].substring(a, b) [i]) {
      break;
    }
    val <<= 1;
    val |= (weathermemory[fourdayforecast].substring(a, b) [i] == '1') ? 1 : 0;
  }
  return val;
}


int binStrToDec( String input ) {
  int out = 0;
  int len = input.length();
  for( int i=0; i<len; i++ ) {
    if( input.substring( i, i+1 ) == "1" ) {
      out += pow(2, i);
    }
  }
  log_d("IN: %s, OUT: %d", input.c_str(), out);
  return out;
}


String decToBinStr( byte aInfo[3] ) {
  String out = "";
  out.reserve(25);
  for(byte a=0;a<3;a++) {
    for (int i = 7; i >=0; i--) {
      //bits[i] = (aInfo[a] >> i) & 1;
      out += String( (aInfo[a] >> i) & 1 ); // bits[i];
    }
  }
  return out;
}


void showWeather() {
  /*
    0110  Day critical weather
        0001  Night critical weather
            0001  Extremeweather
                011  Rainprobability
                   0  Anomaly
                    101101  Temperature
  */
  log_d("Deciphered Weather Data: %s", meteodata.c_str());
  int dcw  = binStrToDec( meteodata.substring(0, 4) );   // Day critical weather
  int ncw  = binStrToDec( meteodata.substring(4, 8) );   // Night critical weather
  int xwh  = binStrToDec( meteodata.substring(8, 12) );  // Extremeweather
  int anm1 = binStrToDec( meteodata.substring(8, 10) );
  int anm2 = binStrToDec( meteodata.substring(10, 12) );
  int rpb  = binStrToDec( meteodata.substring(12, 15) ); // Rainprobability
  int anm  = binStrToDec( meteodata.substring(15, 16) ); // Anomaly
  int tem  = binStrToDec( meteodata.substring(16, 22) ); // Temperature

  log_d("dcw=%s, ncw=%s, xwh=%s, rpb=%s, anm=%s, tem=%d%sC", weather[dcw].c_str(), weather[ncw].c_str(), winddirection[xwh], probprecip[rpb], anm==0 ? "yes" : "no", tem-22, String( degreeSign ).c_str() );

  DCFDateTime mDcfTime = mDcf.GetTime();
  // uint8_t yOff, m, d, hh, mm;

  fourdayforecast = ((mDcfTime.hh) % 3) * 20; 
  
  if (mDcfTime.mm > 0) {
    fourdayforecast += (mDcfTime.mm - 1) / 3;
  }
  if (mDcfTime.mm > 0) {
    twodayforecast = ((((mDcfTime.hh) * 60) + (mDcfTime.mm - 1)) % 90) / 3;
  } else {
    twodayforecast = ((mDcfTime.hh) * 60);
  }
  twodayforecast += 60;

  log_d("four: %d, two: %d", fourdayforecast, twodayforecast);
  Serial.print("");
  delay(10);
  
  if (mDcfTime.hh < 21) { //Between 21:00-23:59 significant weather & temperature is for cities 60-89 wind and wind direction for cities 0-59.
    if ((mDcfTime.hh) % 6 < 3) {
      weathermemory[fourdayforecast] = meteodata;  // Extreme weather is valid from this hour but also +3 hour
      significantweather[fourdayforecast] = weatherunbinary(8, 12);
    }
    Serial.printf("Area 4dayforecast =   %d %s\n", fourdayforecast, town[fourdayforecast].c_str());
    Serial.printf("fourday f/c %5s =      %d\n", String( (((mDcfTime.hh) % 6) > 2) ? "Night" : "Day" ).c_str(), int( (mDcfTime.hh / 6) + 1 ) );
    Serial.printf("Day               =   %s %02x ", meteodata.substring(0, 4).c_str(), dcw);
    if (dcw == 5 || dcw == 6 || dcw == 13 || dcw == 7) {
      if (significantweather[fourdayforecast] == 1 || significantweather[fourdayforecast] == 2) {
        Serial.printf("%s%s with thunderstorms\n", (dcw != 6) ? "Heavy " : "", weather[dcw].c_str());
      } else {
        Serial.printf("%s\n", weather[dcw].c_str());
      }
    } else {
      Serial.printf("%s\n", weather[dcw].c_str());
    }
    Serial.printf("Night             =   %s %02x ", meteodata.substring(4, 8).c_str(), ncw);
    if (ncw == 5 || ncw == 6 || ncw == 13 || ncw == 7) {
      if (significantweather[fourdayforecast] == 1 || significantweather[fourdayforecast] == 3) {
        Serial.printf("%s%s with thunderstorms\n", (ncw != 6) ? "Heavy " : "", weather[ncw].c_str());
      } else {
        Serial.printf("%s\n", weather[ncw].c_str());
      }
    } else {
      if (ncw == 1) {
        Serial.println("Clear");
      } else {
        Serial.printf("%s\n", weather[ncw].c_str());
      }
    }
    if ((mDcfTime.hh) % 6 < 3) {
      Serial.print("Extremeweather    =   "); 
      if (xwh == 0) { // DI=0 and WA =0
        Serial.printf("%s %02x %s\n", weathermemory[fourdayforecast].substring(8, 12).c_str(), significantweather[fourdayforecast], heavyweather[significantweather[fourdayforecast]]);
      } else {
        Serial.printf("%s %02x %s\n", meteodata.substring(8, 10).c_str(), anm1, anomaly1[anm1].c_str() );
        Serial.printf("%s %02x %s\n", meteodata.substring(10, 12).c_str(), anm2, anomaly2[anm2].c_str() );
      }
      Serial.printf("Rainprobability   =    %s %02x %s\n", meteodata.substring(12, 15).c_str(), rpb, probprecip[rpb].c_str() );
    }
    if ((mDcfTime.hh) % 6 > 2) {
      Serial.printf("Winddirection     =   %s %02x %s\n", meteodata.substring(8, 12).c_str(), xwh, winddirection[xwh].c_str());
      Serial.printf("Windstrength      =    %s %02x %s  Bft\n", meteodata.substring(12, 15).c_str(), rpb, windstrength[rpb].c_str());
      Serial.printf("Extremeweather    =   %s %02x %s\n", weathermemory[fourdayforecast].substring(8, 12).c_str(), significantweather[fourdayforecast], heavyweather[significantweather[fourdayforecast]].c_str());
    }
    Serial.printf("Weather Anomality =      %s %02x %s\n", meteodata.substring(15, 16).c_str(), anm, (anm == 1) ? "Yes" : "No");
  } else {
    Serial.printf("Area 4dayforecast =   %d %s\n", fourdayforecast, town[fourdayforecast].c_str());
    Serial.printf("fourday f/c %05s =      %d\n", (((mDcfTime.hh) % 6) > 2) ? "Night" : "Day", (mDcfTime.hh / 6) + 1);
    Serial.printf("Winddirection     =   %s %02x %s\n", meteodata.substring(8, 12).c_str(), xwh, winddirection[xwh].c_str());
    Serial.printf("Windstrength      =    %s %02x %s  Bft\n", meteodata.substring(12, 15).c_str(), rpb, windstrength[rpb].c_str());
    Serial.printf("Area 2dayforecast =   %d %s\n", twodayforecast, town[twodayforecast].c_str());
    Serial.printf("twoday f/c day    =      %d\n", (((mDcfTime.hh - 21) * 60 + mDcfTime.mm) > 90) ? 2 : 1);
    Serial.printf("Day               =   %s %02x %s\n", meteodata.substring(0, 4).c_str(), dcw, weather[dcw].c_str());
    Serial.printf("Night             =   %s %02x %s\n", meteodata.substring(4, 8).c_str(), ncw, (ncw == 1) ? "Clear" : weather[ncw].c_str());
    Serial.printf("Weather Anomality =      %s %02x %s\n", meteodata.substring(15, 16).c_str(), anm, (anm == 1) ? "Yes" : "No");
  }

  String temperatureStr;
  
  if (tem == 0) {
    temperatureStr = "<-21 " + String(degreeSign) + "C";
  } else if (tem == 63) {
    temperatureStr = ">40 " + String(degreeSign) + "C";
  } else {
    temperatureStr = String(tem - 22) + String(degreeSign) + "C";
  }
  // Night temperature is minimum
  // Daytime temperature is daytime maximum
  if (((mDcfTime.hh) % 6) > 2 && (mDcfTime.hh < 21)) {
    temperatureStr += " minimum";
  } else {
    temperatureStr += " maximum";
  }
  Serial.printf("Temperature       = %s %02x %s\n", meteodata.substring(16, 22).c_str(), tem, temperatureStr.c_str());
  Serial.printf("Decoder status    =     %s    %s\n", meteodata.substring(22, 24), meteodata.substring(22, 24) == "10" ? "OK" : "NOT OK");
}





float steps = ((2*PI)/MSG_SIZE);
float invsteps = 1/steps;
float r1 = 54;
float r2 = 60;
#define ST7735_GRAY tft.Color565(64,64,64)




//====================================================================================
//   Decode and render the Jpeg image onto the TFT screen
//====================================================================================
#define minimum(a,b)     (((a) < (b)) ? (a) : (b))
void jpegRender(int xpos, int ypos) {

  // retrieve infomration about the image
  uint16_t  *pImg;
  uint16_t mcu_w = JpegDec.MCUWidth;
  uint16_t mcu_h = JpegDec.MCUHeight;
  uint32_t max_x = JpegDec.width;
  uint32_t max_y = JpegDec.height;

  // Jpeg images are draw as a set of image block (tiles) called Minimum Coding Units (MCUs)
  // Typically these MCUs are 16x16 pixel blocks
  // Determine the width and height of the right and bottom edge image blocks
  uint32_t min_w = minimum(mcu_w, max_x % mcu_w);
  uint32_t min_h = minimum(mcu_h, max_y % mcu_h);

  // save the current image block size
  uint32_t win_w = mcu_w;
  uint32_t win_h = mcu_h;

  // record the current time so we can measure how long it takes to draw an image
  uint32_t drawTime = millis();

  // save the coordinate of the right and bottom edges to assist image cropping
  // to the screen size
  max_x += xpos;
  max_y += ypos;

  // read each MCU block until there are no more
  while ( JpegDec.read()) {

    // save a pointer to the image block
    pImg = JpegDec.pImage;

    // calculate where the image block should be drawn on the screen
    int mcu_x = JpegDec.MCUx * mcu_w + xpos;
    int mcu_y = JpegDec.MCUy * mcu_h + ypos;

    // check if the image block size needs to be changed for the right edge
    if (mcu_x + mcu_w <= max_x) win_w = mcu_w;
    else win_w = min_w;

    // check if the image block size needs to be changed for the bottom edge
    if (mcu_y + mcu_h <= max_y) win_h = mcu_h;
    else win_h = min_h;

    // copy pixels into a contiguous block
    if (win_w != mcu_w)
    {
      for (int h = 1; h < win_h-1; h++)
      {
        memcpy(pImg + h * win_w, pImg + (h + 1) * mcu_w, win_w << 1);
      }
    }

    // draw image MCU block only if it will fit on the screen
    if ( ( mcu_x + win_w) <= tft.width() && ( mcu_y + win_h) <= tft.height())
    {
      tft.drawRGBBitmap(mcu_x, mcu_y, pImg, win_w, win_h);
    }

    else if ( ( mcu_y + win_h) >= tft.height()) JpegDec.abort();

  }

  // calculate how long it took to draw the image
  drawTime = millis() - drawTime; // Calculate the time it took

  // print the results to the serial port
  Serial.print  ("Total render time was    : "); Serial.print(drawTime); Serial.println(" ms");
  Serial.println("=====================================");

}

void tft_drawJpg(const uint8_t * jpg_data, size_t jpg_len, uint16_t x=0, uint16_t y=0, uint16_t maxWidth=0, uint16_t maxHeight=0) {
  // Open the named file (the Jpeg decoder library will close it)
  // Use one of the following methods to initialise the decoder:
  boolean decoded = JpegDec.decodeArray(jpg_data, jpg_len);
  //boolean decoded = JpegDec.decodeSdFile(filename);  // or pass the filename (String or character array)
  if (decoded) {
    jpegRender(x, y);
  } else {
    Serial.println("Jpeg file format not supported!");
  }
}



void showRtcTime() {
    DateTime now = rtc.now();
    
    Serial.print(now.year(), DEC);
    Serial.print('/');
    Serial.print(now.month(), DEC);
    Serial.print('/');
    Serial.print(now.day(), DEC);
    Serial.print(' ');
    Serial.print(now.hour(), DEC);
    Serial.print(':');
    Serial.print(now.minute(), DEC);
    Serial.print(':');
    Serial.print(now.second(), DEC);
    Serial.println();
    
    Serial.print(" since 1970 = ");
    Serial.print(now.unixtime());
    Serial.print("s = ");
    Serial.print(now.unixtime() / 86400L);
    Serial.println("d");
    
    Serial.println();
}



void setup(void) {
  Serial.begin(115200);


  pinMode(27,INPUT);//Backlight:27
  digitalWrite(27,HIGH);//New version added to backlight control
  tft.initR(INITR_18GREENTAB);                             // 1.44 v2.1
  tft.fillScreen(ST7735_BLACK);                            // CLEAR
  tft.setTextColor(0x5FCC);                                // GREEN
  tft.setRotation(1);                                      // 


  rtc.begin(RTC_SDA, RTC_SCL);
  setSyncProvider(rtc.get);   // the function to get the time from the RTC
  if (timeStatus() != timeSet) {
    log_e("Unable to sync with the RTC");
  } else {
    log_d("RTC has set the system time");
  }
  delay(10);
  showRtcTime();
  //rtc.adjust(DateTime(__DATE__, __TIME__));

  for(float i=-PI;i<PI;i+=steps) {
    float x1 = (-sin(i)*r1) + 80;
    float y1 = (cos(i)*r1) + 64;
    float x2 = (-sin(i)*r2) + 80;
    float y2 = (cos(i)*r2) + 64;
    tft.drawCircle(x1, y1, 2, ST7735_GRAY);
    tft.drawCircle(x2, y2, 2, ST7735_GRAY);
  }
  
  for (int i=0;i<61;i++){
    weathermemory[i]="";
    for (int j=0;j<24;j++){
      bool p = 0;
      weathermemory[i]+=p;
    }   
  }
  mDcf.Init(RADIO_DIGITAL_PIN, RADIO_POWERDOWN_PIN, RADIO_AUTOGAIN_PIN);
  Serial.printf("DCF77 Time/Weather reporter by tobozo, Built on %s %s\n", __DATE__, __TIME__);
  mNow = DCFDateTime(BUILDTM_YEAR, BUILDTM_MONTH, BUILDTM_DAY, BUILDTM_HOUR, BUILDTM_MIN);
  Serial.println("Setup done ");
  Serial.println( String( clockSign).c_str() );

  tft_drawJpg(img_antenna_jpg, img_antenna_jpg_len, ( tft.width()/2 - 48/2 ), ( tft.height()/2 - 48/2 ), 48, 48);
  //tft.drawBitmap( ( tft.width()/2 - 75/2 ), (tft.height()-75)/2, img_antenna_bmp
  //img_antenna_bmp_len

  /*
  ssd1327Init(0x3c, 0, 0, 25, 32, 1000000UL);
  ssd1327Fill(0);
  ssd1327SetContrast(255);

  display.begin();
  display.clearBuffer();
  display.clearBuffer();

  float steps = ((2*PI)/60);
  float r1 = 40;
  float r2 = 36;
  
  //display.firstPage();

  for(float i=-PI;i<PI;i+=steps) {
    float x1 = (sin(i)*r1) + 64;
    float y1 = (cos(i)*r1) + 64;
    float x2 = (sin(i)*r2) + 64;
    float y2 = (cos(i)*r2) + 64;
    
    //do {
      display.drawLine(x1, y1-16, x2, y2-16);
    //} while( 
    //  display.nextPage(); 
    //);
  }
  display.sendBuffer();
  //ssd1327DrawLine(starXPrev, starYPrev, star[i][3], star[i][4], starColor);
  */
  
  //memcpy(mDcf.mMessage, mDcf.mMessageHistory[1], MSG_SIZE);
}


bool mMessageHistory[2][MSG_SIZE];
byte lastmBitCounter;

void loop(void) {

  static unsigned long mPrevTimeCheckKeysMs = 0;
  static unsigned long mPrevTimeTimeMs = 0;
  int lDcfState = mDcf.Run();
  unsigned long nowMs = millis();
  unsigned long diff = nowMs - mPrevTimeTimeMs;
  int16_t  x1, y1;
  uint16_t w, h;


  if( (lDcfState != DCF_STATE_SAMPLING) && lastmBitCounter != mDcf.mBitCounter ) {
    lastmBitCounter = mDcf.mBitCounter-1;
    if( lastmBitCounter==255 ) lastmBitCounter = MSG_SIZE;
    float i = (float(lastmBitCounter - MSG_SIZE/2) / MSG_SIZE) *2*PI;
    float x1 = (-sin(i)*r1) + 80;
    float y1 = (cos(i)*r1) + 64;
    if( mDcf.mMessage[lastmBitCounter] ) {
      tft.fillCircle(x1, y1, 2, ST7735_BLUE);
    } else {
      tft.fillCircle(x1, y1, 2, ST7735_BLACK);
      tft.drawCircle(x1, y1, 2, ST7735_GRAY);
    }
    mMessageHistory[0][lastmBitCounter] = mDcf.mMessage[lastmBitCounter];
    // antenna blink
    tft.fillCircle(79, 51, 3, mDcfStatus ? ST7735_BLUE : ST7735_WHITE);
  }
  

  if (lDcfState == DCF_STATE_NEWWEATHER) {
    //byte lSection = mDcf.GetWeatherSection();
    //byte area = mDcf.GetWeatherArea();
    byte aInfo[WEATHER_INFO_SIZE];
    if (mDcf.GetWeatherInfo(aInfo)) {
      //log_d( "Ciphered Weather Data: %s", mDcf.weatherData );
      meteodata = decToBinStr(aInfo);
      showWeather();
    } else {
      log_d("Weather info NOT deciphered");
    }
  }

  if ((diff > 10000) &&   // once every 10 seconds
      (lDcfState != DCF_STATE_SAMPLING)) {
    //Serial.print("+");
    mNow = mDcf.GetTime();
    if (mPrevMinute != mNow.mm) { // once every minute
      //Serial.print("-");
      Serial.printf("%s : %s\n", String( clockSign ).c_str(), mNow.GetTimeStr());
      //handleTempPressure();
      // add a new value to the graphics bar every 30 minutes
      byte lNowMinute = mNow.mm;
      if ((lNowMinute == 0 || lNowMinute == 30)) { // once every 30 minutes
        //registerPressure((byte)mCurrentPress);
      }

      // does the time and/or date on the screen need an update?
      
      const String timeStr = mNow.GetTimeStr( mDcfStatus ? ":" : " " );
      tft.getTextBounds(timeStr, (int16_t)0, (int16_t)0,  &x1, &y1, &w, &h);
      tft.setCursor(tft.width()/2 - w/2, 24);
      tft.setTextColor(ST7735_WHITE, ST7735_BLACK);
      tft.print( mNow.GetTimeStr( mDcfStatus ? ":" : " " ) );
      
      const String dateStr = mNow.GetDateStr();
      tft.getTextBounds(dateStr, (int16_t)0, (int16_t)0,  &x1, &y1, &w, &h);
      tft.setCursor(tft.width()/2 - w/2, 92);
      tft.setTextColor(ST7735_WHITE, ST7735_BLACK);
      tft.print( dateStr );
      
      mPrevMinute = lNowMinute;  // mNow.minute();
    }
    mPrevTimeTimeMs = nowMs;
  }

  // TODO now every loop the same questionis asked, better only ask once
  // although it looks like it only appears once a second
  if (lDcfState == DCF_STATE_NEWSECOND) { // once every second
    //Serial.print("%"); // show indicator of DCF signal ?
  
    if(mDcf.TimeIsValid()) {
      mNow = mDcf.GetTime();

      const String timeStr = mNow.GetTimeStr( mDcfStatus ? ":" : " " );
      tft.getTextBounds(timeStr, (int16_t)0, (int16_t)0,  &x1, &y1, &w, &h);
      tft.setCursor(tft.width()/2 - w/2, 24);
      tft.setTextColor(ST7735_WHITE, ST7735_BLACK);
      tft.print( mNow.GetTimeStr( mDcfStatus ? ":" : " " ) );
      
    }

    mDcfStatus = !mDcfStatus;   // toggle indicator
  }

  // TODO now every loop the same questionis asked, better only ask once
  if (lDcfState == DCF_STATE_NEWMINUTE || lDcfState == DCF_STATE_NEWWEATHER) {
    // new time available
    //Serial.print("&");
    for(uint8_t r=0;r<MSG_SIZE;r++) {
      float i = (float(r - MSG_SIZE/2) / MSG_SIZE) *2*PI;
      //for(float i=-PI;i<PI;i+=steps)
      float x1 = (-sin(i)*r1) + 80;
      float y1 = (cos(i)*r1) + 64;
      float x2 = (-sin(i)*r2) + 80;
      float y2 = (cos(i)*r2) + 64;
      int msgIndex = (i+PI)*invsteps;
      mMessageHistory[1][msgIndex] = mMessageHistory[0][msgIndex];
      mMessageHistory[0][msgIndex] = false;
      //mDcf.mMessage[msgIndex] = false;
      if( mMessageHistory[1][msgIndex] ) {
        tft.fillCircle(x2, y2, 2, ST7735_GREEN);
      } else {
        tft.fillCircle(x2, y2, 2, ST7735_BLACK);
        tft.drawCircle(x2, y2, 2, ST7735_GRAY);
      }
      tft.fillCircle(x1, y1, 2, ST7735_BLACK);
      tft.drawCircle(x1, y1, 2, ST7735_GRAY);
    }
    if (mDcf.TimeIsValid()) {
      //Serial.print("*");
    }
    //*mDcf.mMessage = false;
  }
}
