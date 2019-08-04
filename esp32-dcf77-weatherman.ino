#include "build.h"
#include "DateTime.h"
#include "dcf77.h"

#define DEBUG 1

// DCF77 from https://www.tindie.com/products/universalsolder/atomic-clock-am-receiver-kit-dcf77-wwvb-msf-jjy60/
#define RADIO_DIGITAL_PIN 14    // OUT
#define RADIO_POWERDOWN_PIN 26  // PDN
#define RADIO_AUTOGAIN_PIN 33   // AON

Dcf77    mDcf;
DateTime mNow;


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

  DateTime mDcfTime = mDcf.GetTime();
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
  } else /*if ((tem != 0) & (tem != 63))*/ {
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
  Serial.printf("Decoder status    =     %s    %s\n", meteodata.substring(22, 24) == "10" ? "OK" : "NOT OK");
}





void setup(void) {
  Serial.begin(115200);
  for (int i=0;i<61;i++){
    weathermemory[i]="";
    for (int j=0;j<24;j++){
      bool p = 0;
      weathermemory[i]+=p;
    }   
  }
  mDcf.Init(RADIO_DIGITAL_PIN, RADIO_POWERDOWN_PIN, RADIO_AUTOGAIN_PIN);
  Serial.printf("DCF77 Time/Weather reporter by tobozo, Built on %s %s\n", __DATE__, __TIME__);
  mNow = DateTime(BUILDTM_YEAR, BUILDTM_MONTH, BUILDTM_DAY, BUILDTM_HOUR, BUILDTM_MIN);
  Serial.println("Setup done ");
  Serial.println( String( clockSign).c_str() );
}



void loop(void) {

  static unsigned long mPrevTimeCheckKeysMs = 0;
  static unsigned long mPrevTimeTimeMs = 0;
  int lDcfState = mDcf.Run();
  unsigned long nowMs = millis();
  unsigned long diff = nowMs - mPrevTimeTimeMs;

  
  if (lDcfState == DCF_STATE_NEWWEATHER) {
    //byte lSection = mDcf.GetWeatherSection();
    //byte area = mDcf.GetWeatherArea();
    byte aInfo[WEATHER_INFO_SIZE];
    if (mDcf.GetWeatherInfo(aInfo)) {
      log_d( "Ciphered Weather Data: %s", mDcf.weatherData );
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
      //UpdateDisplay(DISPLAY_TIME);
      mPrevMinute = lNowMinute;  // mNow.minute();
    }
    mPrevTimeTimeMs = nowMs;
  }

  // TODO now every loop the same questionis asked, better only ask once
  // although it looks like it only appears once a second
  if (lDcfState == DCF_STATE_NEWSECOND) { // once every second
    //Serial.print("%"); // show indicator of DCF signal ?
    if (mDcfStatus) {
      //mTft.print(mDcf.TimeIsValid()?"*":"+");
    }
    mDcfStatus = !mDcfStatus;   // toggle indicator
  }

  // TODO now every loop the same questionis asked, better only ask once
  if (lDcfState == DCF_STATE_NEWMINUTE) {
    // new time available
    //Serial.print("&");
    if (mDcf.TimeIsValid()) {
      //Serial.print("*");
    }
  }
}
