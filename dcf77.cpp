#include "dcf77.h"

#define LENGTH_ONE   150
#define LENGTH_SPIKE 75
#define MIN_BIT_TIME  600
#define NEW_MSG_TIME  2000
#define DEBUG 1

void Dcf77::Init(int aPort, int pdnPort, int gainPort) {
  log_d("DCF77 Init -> Antenna pin: %d, Power down pin: %d, Autogain pin: %d", aPort, pdnPort, gainPort);
  mPort = aPort;
  pinMode(mPort,INPUT);

  if( pdnPort != 0 ) {
    pinMode(pdnPort, OUTPUT);
    digitalWrite(pdnPort, LOW);  // LOW=turn on radio
  }

  if( gainPort != 0 ) {
    pinMode(gainPort, OUTPUT);
    digitalWrite(gainPort, HIGH);  // HIGH=turn on autogain
  }

  mPrevDcf = false;
  mTimePosEdge = 0;
  mTimeNegEdge = 0;
  mBitCounter = 0;
  mBitReceived = false;
  mTimePrevBit = 0;

  mWeatherArea = 0;
  mWeatherSection = 0 ;

  mTime.Clear();
  mTimeValid = false;
  log_d("DCF77 Init done");
}

byte Dcf77::Run() {
  byte lResult = DCF_STATE_SAMPLING;
  bool lDcf = digitalRead(mPort);
  unsigned long now = millis();

  if (!mPrevDcf && lDcf) { // positive edge = start of bit
    if (mTimePosEdge==0) { // only take the first one
      mTimePosEdge = now;
      mBitReceived = false;
    }
  }
  
  if (mPrevDcf && !lDcf ) {  // negative edge = end of bit
    mTimeNegEdge = now;     // always take the last one
  }
  
  if (((now - mTimePosEdge) > MIN_BIT_TIME) && !mBitReceived) { // waited long enough for a complete bit
    bool lBit = ((mTimeNegEdge - mTimePosEdge) >= LENGTH_ONE);
    log_v("%d", lBit);
    if(mBitCounter<MSG_SIZE) {
      mMessage[mBitCounter] = lBit;
    }
    mTimePrevBit = mTimePosEdge;    // that's when this bit started
    mTimePosEdge = 0;   // find the next positive edge
    mBitReceived = true;
    lResult = DCF_STATE_NEWSECOND;		// sampling for bit finished
    mBitCounter++;
  }

  if (((now - mTimePrevBit) > NEW_MSG_TIME) && (mBitCounter!=0)) {
    // start of message (>1 sec after last bit)
    log_d("MSG START");
    checkMessage(mMessage);
    lResult = DCF_STATE_NEWMINUTE;		// new complete minute

    if( addToWeatherInfo(mMessage))	{ // maybe even complete weather info
      lResult = DCF_STATE_NEWWEATHER;
      // calculate area which is related to the time of the message
      mWeatherArea = getArea(mTime);
      mWeatherSection = getSection(mTime);
      log_d("Weather info collected for area #%d, previsions for for the next %d hours", mWeatherArea, mWeatherSection);
      int i=0;
      for( ; i<WEATHER_SIZE; i++) {
        weatherData[i] = mWeatherData[i] ? '0' : '1';
      }
    }
    mBitCounter = 0;
  }

  mPrevDcf = lDcf;
  return lResult;
}
	
DateTime Dcf77::GetTime(void) {
	return mTime;
}

bool Dcf77::TimeIsValid() {
  //return (mTime.d != 0);
  return mTimeValid;
}

byte Dcf77::GetWeatherArea() {
  return mWeatherArea;
}

byte Dcf77::GetWeatherSection() {
  return mWeatherSection;
}

bool Dcf77::GetWeatherInfo(byte* aWeatherInfo) {
  bool lValid = false;
  int uiBitCnt;
  byte ucTemp = 0;
  byte cipher[5];
  byte key[5];
  byte PlainBytes[5];
  uiBitCnt = 0;
  int uiCnt = 1;

/*
  log_d("Weather info Decoded: ");
  for( int i=0; i<82; i++) {
    log_d( "[%d] -> %d\n", i, (byte)mWeatherData[i] );
  }
*/

  for (; uiCnt < 42; uiCnt++) {
      if (uiCnt != 7) {
          ucTemp = (byte)(ucTemp >> 1);
          if (mWeatherData[uiCnt] == 1)
              ucTemp |= 0x80;
          uiBitCnt++;
          if ((uiBitCnt & 7) == 0)
              cipher[(uiBitCnt >> 3) - 1] = ucTemp;
      }
  }
  uiBitCnt=0;
  for (; uiCnt < 82; uiCnt++) {
          ucTemp = (byte)(ucTemp >> 1);
          if (mWeatherData[uiCnt] == 1)
              ucTemp |= 0x80;
          uiBitCnt++;
          if ((uiBitCnt & 7) == 0)
              key[(uiBitCnt >> 3) - 1] = ucTemp;
  }

  Decrypt(cipher,key, PlainBytes);

  uint16_t checkSum;
  checkSum = PlainBytes[2] & 0x0Fu;
  checkSum <<= 8;
  checkSum |= PlainBytes[1];
  checkSum <<= 4;
  checkSum |= (uint16_t)(PlainBytes[0] >> 4);

  if (checkSum == 0x2501) {
    aWeatherInfo[0] = (PlainBytes[3]&0x0F)<<4;
    aWeatherInfo[0] |= (PlainBytes[2]&0xF0)>>4;
    aWeatherInfo[0] = flipByte(aWeatherInfo[0]);
               
    aWeatherInfo[1] = (PlainBytes[4]&0x0F)<<4;
    aWeatherInfo[1] |= (PlainBytes[3]&0xF0)>>4;
    aWeatherInfo[1] = flipByte(aWeatherInfo[1]);
               
    aWeatherInfo[2] = (PlainBytes[0]&0x0F)<<4;
    aWeatherInfo[2] |= (PlainBytes[4]&0xF0)>>4;
    aWeatherInfo[2] = flipByte(aWeatherInfo[2]);
    aWeatherInfo[2] &= 0xFC;
    aWeatherInfo[2] |= 0x02;
    lValid = true;
  }
  
  return lValid;
}

void Dcf77::checkMessage(bool* aMessage) {
  // sometimes the first bit is missed (due to decoding weather)
  //if(aMessage[0]==0 && aMessage[20]==1 && mBitCounter>=58
  if(aMessage[20]==1 && mBitCounter>=58) {
    //if (checkParity(aMessage,21,28) && checkParity(aMessage,29,35) && checkParity(aMessage,36,58))
    if(checkValidMessage(aMessage)) {
      uint8_t hh = (aMessage[29] + aMessage[30]*2 + aMessage[31]*4 + aMessage[32]*8) + 10*(aMessage[33] + aMessage[34]*2);
      uint8_t mm = getMinute(aMessage);
      //mTime.dst = aMessage[17];	// actually bit17+18 indicates the offset to UTC
      uint8_t d = (aMessage[36] + aMessage[37]*2 + aMessage[38]*4 + aMessage[39]*8) +  10*(aMessage[40] + aMessage[41]*2);
      //mTime.weekday = aMessage[42] + aMessage[43]*2 + aMessage[44]*4;
      uint16_t m = (aMessage[45] + aMessage[46]*2 + aMessage[47]*4 + aMessage[48]*8) +  10*aMessage[49];
      uint8_t yOff = (aMessage[50] + aMessage[51]*2 + aMessage[52]*4 + aMessage[53]*8) + 
             10*(aMessage[54] + aMessage[55]*2 + aMessage[56]*4 + aMessage[57]*8);
      mTime.Set(yOff,m,d,hh,mm);
      if (mTime.IsValid())
        mTimeValid = true;
      log_d("From DCF (%s): 20%d-%02d-%02d %02d:%02d", mTimeValid ? "VALID" : "INVALID", yOff, m, d, hh, mm);
    } else {
      mTime.Clear();
      mTimeValid = false;
    }
  }
}

bool Dcf77::addToWeatherInfo(bool* aMessage) {
  bool lCompleted = false;
  //if(aMessage[0]==0 && aMessage[20]==1 && mBitCounter>=58)
  if(aMessage[20]==1 && mBitCounter>=58) {
    //if (checkParity(aMessage,21,28) && checkParity(aMessage,29,35) && checkParity(aMessage,36,58))
    if(checkValidMessage(aMessage)) {
      // a complete meteo string contains:
      // data[1..14] of 3 messages (note, skip bit 0 and 7 of first message)
      // timestamp of last message (BDC, LSB first)
      //    minute (8bit)
      //    hour   (8bit)
      //    day    (8 bit)
      //    month  (5 bit)
      //    weekday(3 bit)
      //    year   (8 bit)

//      byte minute = (aMessage[21] + aMessage[22]*2 + aMessage[23]*4 + aMessage[24]*8) + // LSD minutes
//                    10*(aMessage[25] + aMessage[26]*2 + aMessage[27]*4);
      byte minute = getMinute(aMessage);
      
      minute--;
      byte part = minute % 3;
      switch(part) {
        case 0:   // first part
        {
          // clear all old data
          for(byte i=0; i<WEATHER_SIZE; i++)
            mWeatherData[i]=0;
          //copy received data into correct part
          copyWeatherInfo(aMessage,0);
          break;
        }
        case 1:   // second part
        {
          copyWeatherInfo(aMessage,14);
          byte j = 14+14+14;
          // add time info
          for(byte i=21; i<28; i++)   // add minutes
            mWeatherData[j++] = aMessage[i];
          j++;
          for(byte i=29; i<35; i++)   // add hours
            mWeatherData[j++] = aMessage[i];
          j+=2;
          for(byte i=36; i<42; i++)   // add day
            mWeatherData[j++] = aMessage[i];
          j+=2;
          for(byte i=45; i<50; i++)   // add month
            mWeatherData[j++] = aMessage[i];
          for(byte i=42; i<45; i++)   // add week day
            mWeatherData[j++] = aMessage[i];
          for(byte i=50; i<58; i++)   // add year
            mWeatherData[j++] = aMessage[i];
          break;
        }
        case 2:   // third part
        {
          copyWeatherInfo(aMessage,14+14);
          lCompleted = true;
          break;
        }
      }
    }
  }
  
  return lCompleted;
}

void Dcf77::copyWeatherInfo(bool* aMessage, byte aIndex) {
  byte j=aIndex;
  for(byte i=0; i<14; i++) {
    mWeatherData[j++] = aMessage[i+1];
  }
}

bool Dcf77::checkValidMessage(bool* aMessage) {
  return (checkParity(aMessage,21,28) && checkParity(aMessage,29,35) && checkParity(aMessage,36,58));
}

bool Dcf77::checkParity(bool* aMessage, byte aStart, byte aEnd) {
  byte lParity = false;
  for(byte i=aStart; i<aEnd; i++) 
    lParity += aMessage[i];
  return ((lParity&0x01) == aMessage[aEnd]);
}

byte Dcf77::getMinute(bool* aMessage) {
  return (aMessage[21] + aMessage[22]*2 + aMessage[23]*4 + aMessage[24]*8) + // LSD minutes
          10*(aMessage[25] + aMessage[26]*2 + aMessage[27]*4);
}


/// bit pattern for 0D,0E from 0B-0D
const uint32_t mUintArrBitPattern12[] /*PROGMEM*/ = {
  0x80000, //0b10000000000000000000, // 0D.3 
  0x00010, //0b00000000000000010000, // 0B.4
  0x00008, //0b00000000000000001000, // 0B.3
  0x00100, //0b00000000000100000000, // 0C.0
  0x00080, //0b00000000000010000000, // 0B.7
  0x01000, //0b00000001000000000000, // 0C.4
  0x00800, //0b00000000100000000000, // 0C.3
  0x10000, //0b00010000000000000000, // 0D.0
  0x08000, //0b00001000000000000000, // 0C.7
  0x00001, //0b00000000000000000001, // 0B.0
  0x00000, //0b00000000000000000000, // xxxx
  0x00000  //0b00000000000000000000  // xxxx
};

/// <summary>
/// 12-15 from 16-19 (time)
/// </summary>
const uint32_t mUintArrBitPattern30_1[] /*PROGMEM*/ = {
  0x00000200, //0b00000000000000000000001000000000, // 17.1 
  0x00000020, //0b00000000000000000000000000100000, // 16.5 
  0x02000000, //0b00000010000000000000000000000000, // 19.1 
  0x00000000, //0b00000000000000000000000000000000, // 1A.3 
  0x00000000, //0b00000000000000000000000000000000, // 1A.5  
  0x00000080, //0b00000000000000000000000010000000, // 16.7 
  0x40000000, //0b01000000000000000000000000000000, // 19.6 
  0x01000000, //0b00000001000000000000000000000000, // 19.0 

  0x04000000, //0b00000100000000000000000000000000, // 19.2 
  0x00000000, //0b00000000000000000000000000000000, // 1A.4  
  0x00010000, //0b00000000000000010000000000000000, // 18.0 
  0x00000000, //0b00000000000000000000000000000000, // 1A.2 
  0x00400000, //0b00000000010000000000000000000000, // 18.6 
  0x00000010, //0b00000000000000000000000000010000, // 16.4 
  0x00200000, //0b00000000001000000000000000000000, // 18.5 
  0x00080000, //0b00000000000010000000000000000000, // 18.3 

  0x00004000, //0b00000000000000000100000000000000, // 17.6 
  0x00000000, //0b00000000000000000000000000000000, // 1A.6 
  0x00020000, //0b00000000000000100000000000000000, // 18.1 
  0x00100000, //0b00000000000100000000000000000000, // 18.4 
  0x00008000, //0b00000000000000001000000000000000, // 17.7 
  0x00000040, //0b00000000000000000000000001000000, // 16.6 
  0x00001000, //0b00000000000000000001000000000000, // 17.4 
  0x00000400, //0b00000000000000000000010000000000, // 17.2 

  0x00000001, //0b00000000000000000000000000000001, // 16.0 
  0x80000000, //0b10000000000000000000000000000000, // 19.7 
  0x00000008, //0b00000000000000000000000000001000, // 16.3 
  0x00000002, //0b00000000000000000000000000000010, // 16.1 
  0x00040000, //0b00000000000001000000000000000000, // 18.2 
  0x10000000  //0b00010000000000000000000000000000 // 19.4 
};

/// <summary>
/// bit pattern for 12-15 from 1A (time2)
/// </summary>
const byte mUintArrBitPattern30_2[] /*PROGMEM*/ = {
  0x00, //0b00000000,  // 17.1 
  0x00, //0b00000000,  // 16.5 
  0x00, //0b00000000,  // 19.1 
  0x08, //0b00001000,  // 1A.3 
  0x20, //0b00100000,  // 1A.5 
  0x00, //0b00000000,  // 16.7 
  0x00, //0b00000000,  // 19.6 
  0x00, //0b00000000,  // 19.0 

  0x00, //0b00000000,  // 19.2 
  0x10, //0b00010000,  // 1A.4 
  0x00, //0b00000000,  // 18.0 
  0x04, //0b00000100,  // 1A.2 
  0x00, //0b00000000,  // 18.6 
  0x00, //0b00000000,  // 16.4 
  0x00, //0b00000000,  // 18.5 
  0x00, //0b00000000,  // 18.3 

  0x00, //0b00000000,  // 17.6 
  0x40, //0b01000000,  // 1A.6 
  0x00, //0b00000000,  // 18.1 
  0x00, //0b00000000,  // 18.4 
  0x00, //0b00000000,  // 17.7 
  0x00, //0b00000000,  // 16.6 
  0x00, //0b00000000,  // 17.4 
  0x00, //0b00000000,  // 17.2 

  0x00, //0b00000000,  // 16.0 
  0x00, //0b00000000,  // 19.7 
  0x00, //0b00000000,  // 16.3 
  0x00, //0b00000000,  // 16.1 
  0x00, //0b00000000,  // 18.2 
  0x00  //0b00000000  // 19.4 
};

/// <summary>
/// 12-14 from 1C-1E (result from F)
/// </summary>
const uint32_t mUintArrBitPattern20[] /*PROGMEM*/ = {
  0x000004, //0b000000000000000000000100, // 1C.2 
  0x002000, //0b000000000010000000000000, // 1E.5 
  0x008000, //0b000000001000000000000000, // 1E.7 
  0x400000, //0b010000000000000000000000, // 1D.6 
  0x000100, //0b000000000000000100000000, // 1E.0 
  0x100000, //0b000100000000000000000000, // 1D.4 
  0x000400, //0b000000000000010000000000, // 1E.2 
  0x800000, //0b100000000000000000000000, // 1D.7 

  0x040000, //0b000001000000000000000000, // 1D.2 
  0x020000, //0b000000100000000000000000, // 1D.1 
  0x000008, //0b000000000000000000001000, // 1C.3 
  0x000200, //0b000000000000001000000000, // 1E.1 
  0x004000, //0b000000000100000000000000, // 1E.6 
  0x000002, //0b000000000000000000000010, // 1C.1 
  0x001000, //0b000000000001000000000000, // 1E.4 
  0x080000, //0b000010000000000000000000, // 1D.3 

  0x000800, //0b000000000000100000000000, // 1E.3 
  0x200000, //0b001000000000000000000000, // 1D.5 
  0x010000, //0b000000010000000000000000, // 1D.0 
  0x000001  //0b000000000000000000000001  // 1C.0 
};

/// <summary>
/// bit pattern for 12-15 from 16-19 (1/3)
/// </summary>
 const byte mByteArrLookupTable1C_1[] /*PROGMEM*/ = {
  0xBB, 0x0E, 0x22, 0xC5, 0x73, 0xDF, 0xF7, 0x6D, 0x90, 0xE9, 0xA1, 0x38, 0x1C, 0x84, 0x4A, 0x56,
  0x64, 0x8D, 0x28, 0x0B, 0xD1, 0xBA, 0x93, 0x52, 0x1C, 0xC5, 0xA7, 0xF0, 0xE9, 0x7F, 0x36, 0x4E,
  0xC1, 0x77, 0x3D, 0xB3, 0xAA, 0xE0, 0x0C, 0x6F, 0x14, 0x88, 0xF6, 0x2B, 0xD2, 0x99, 0x5E, 0x45,
  0x1F, 0x70, 0x96, 0xD3, 0xB3, 0x0B, 0xFC, 0xEE, 0x81, 0x42, 0xCA, 0x34, 0xA5, 0x58, 0x29, 0x67
};

/// <summary>
/// bit pattern for 12-15 from 16-19 (2/3)
/// </summary>
const byte mByteArrLookupTable1C_2[] /*PROGMEM*/ = {
  0xAB, 0x3D, 0xFC, 0x74, 0x65, 0xE6, 0x0E, 0x4F, 0x97, 0x11, 0xD8, 0x59, 0x83, 0xC2, 0xBA, 0x20,
  0xC5, 0x1B, 0xD2, 0x58, 0x49, 0x37, 0x01, 0x7D, 0x93, 0xFA, 0xE0, 0x2F, 0x66, 0xB4, 0xAC, 0x8E,
  0xB7, 0xCC, 0x43, 0xFF, 0x58, 0x66, 0xEB, 0x35, 0x82, 0x2A, 0x99, 0xDD, 0x00, 0x71, 0x14, 0xAE,
  0x4E, 0xB1, 0xF7, 0x70, 0x18, 0x52, 0xAA, 0x9F, 0xD5, 0x6B, 0xCC, 0x3D, 0x04, 0x83, 0xE9, 0x26
};

/// <summary>
/// bit pattern for 12-15 from 16-19 (3/3)
/// </summary>
const byte mByteArrLookupTable1C_3[] /*PROGMEM*/ = {
  0x0A, 0x02, 0x00, 0x0F, 0x06, 0x07, 0x0D, 0x08, 0x03, 0x0C, 0x0B, 0x05, 0x09, 0x01, 0x04, 0x0E,
  0x02, 0x09, 0x05, 0x0D, 0x0C, 0x0E, 0x0F, 0x08, 0x06, 0x07, 0x0B, 0x01, 0x00, 0x0A, 0x04, 0x03,
  0x08, 0x00, 0x0D, 0x0F, 0x01, 0x0C, 0x03, 0x06, 0x0B, 0x04, 0x09, 0x05, 0x0A, 0x07, 0x02, 0x0E,
  0x03, 0x0D, 0x00, 0x0C, 0x09, 0x06, 0x0F, 0x0B, 0x01, 0x0E, 0x08, 0x0A, 0x02, 0x07, 0x04, 0x05
};

#define Byte0  Bytes[0]
#define Byte1  Bytes[1]
#define Byte2  Bytes[2]
#define Byte3  Bytes[3]

union ByteUInt {
  byte Bytes[4];
  uint32_t  FullUint;
};

/// Container, wich contains all former global vars
struct DataContainer {
  ByteUInt mByteUint1;  // Registers R12 to R15
  ByteUInt mByteUint2;  // Registers R08 to R0A
  ByteUInt mByteUint3;  // Registers R0B to R0E
  ByteUInt mByteUint4;  // Registers R1C to R1E
  byte mByteUpperTime2; // mByteR1B;
  uint32_t mUintLowerTime;
};


byte Dcf77::flipByte(byte aByte) {
  byte result = 0;
  byte source = aByte;
  for(int i=0; i<8; i++) {
    result >>= 1;
    result |= source&0x80;
    source <<= 1;
  }
  return result;
}

void Dcf77::CopyTimeToByteUint(byte* data, byte* key, struct DataContainer* container) {
  container->mByteUint1.FullUint = 0;
  container->mByteUint2.FullUint = 0;
  container->mByteUint3.FullUint = 0;
  container->mUintLowerTime = 0;
  container->mByteUpperTime2 = 0;
  
  for (int i = 0; i < 4; i++) {
      container->mUintLowerTime <<= 8;
      container->mUintLowerTime |= key[3 - i];
  }
  container->mByteUpperTime2 = key[4];

  // copy R
  container->mByteUint3.Byte0 = data[2];
  container->mByteUint3.Byte1 = data[3];
  container->mByteUint3.Byte2 = data[4];
  container->mByteUint3.FullUint >>= 4;

  // copy L
  container->mByteUint2.Byte0 = data[0];
  container->mByteUint2.Byte1 = data[1];
  container->mByteUint2.Byte2 = (byte)(data[2] & 0x0F);
}


void Dcf77::ShiftTimeRight(int round, struct DataContainer* container) {
  int count;
  byte tmp;

  if ((round == 16) || (round == 8) || (round == 7) || (round == 3))
      count = 2;
  else
      count = 1;

  while (count-- != 0) {
      tmp = 0;
      if ((container->mUintLowerTime & (uint32_t)0x00100000) != 0)         // save time bit 20
          tmp = 1;

      container->mUintLowerTime &= (uint32_t)0xFFEFFFFF;
      if ((container->mUintLowerTime & 1) != 0)
          container->mUintLowerTime |= (uint32_t)0x00100000;       // copy time bit 0 to time bit 19
      container->mUintLowerTime >>= 1;             // time >>= 1

      if ((container->mByteUpperTime2 & 1) != 0)
          container->mUintLowerTime |= (uint32_t)0x80000000;
      container->mByteUpperTime2 >>= 1;
      if (tmp != 0)
          container->mByteUpperTime2 |= 0x80;          // insert time bit 20 to time bit 39
  }
}

void Dcf77::ExpandR(struct DataContainer* container) {
  uint32_t tmp;

  container->mByteUint3.FullUint &= 0x000FFFFF;      // clear 0D(4-7),0E
  tmp = 0x00100000;         // and set bits form 0B-0D(0-3)
  for (int i = 0; i < 12; i++) {
      if ((container->mByteUint3.FullUint & mUintArrBitPattern12[i]) != 0)
          container->mByteUint3.FullUint |= tmp;
      tmp <<= 1;
  }
}

void Dcf77::CompressKey(struct DataContainer* container) {
  uint32_t tmp = (uint32_t)0x00000001;         // and set bits from 16-1A (time)

  container->mByteUint1.FullUint = 0;          // clear 12-15
  for (int i = 0; i < 30; i++) {
      if ((container->mUintLowerTime & mUintArrBitPattern30_1[i]) != 0 || (container->mByteUpperTime2 & mUintArrBitPattern30_2[i]) != 0)
          container->mByteUint1.FullUint |= tmp;
      tmp <<= 1;
  }
}

void Dcf77::DoSbox(struct DataContainer* container) {
  byte tmp, helper; //mByteR1B;

  helper = container->mByteUint1.Byte3;                // R1B = R15;
  container->mByteUint1.Byte3 = container->mByteUint1.Byte2;      // R15 = R14

  // INNER LOOP
  for (int i = 5; i > 0; i--) {
      if ((i & 1) == 0) { // round 4,2
          tmp = (byte)(container->mByteUint1.Byte0 >> 4);    // swap R12
          tmp |= (byte)((container->mByteUint1.Byte0 & 0x0f) << 4);
          container->mByteUint1.Byte0 = tmp;
      }
      container->mByteUint1.Byte3 &= 0xF0;           // set R1C
      tmp = (byte)((container->mByteUint1.Byte0 & 0x0F) | container->mByteUint1.Byte3);

      if ((i & 4) != 0)
          tmp = mByteArrLookupTable1C_1[(tmp & 0x3F)];

      if ((i & 2) != 0)
          tmp = mByteArrLookupTable1C_2[(tmp & 0x3F)];

      else if (i == 1)
          tmp = mByteArrLookupTable1C_3[(tmp & 0x3F)];

      if ((i & 1) != 0)
          container->mByteUint4.Byte0 = (byte)(tmp & 0x0F);
      else
          container->mByteUint4.Byte0 |= (byte)(tmp & 0xF0);

      if ((i & 1) == 0) { // copy 14->13->12, 1C->1E->1D
          tmp = container->mByteUint1.Byte3;
          container->mByteUint1.FullUint >>= 8;
          container->mByteUint1.Byte3 = tmp;
          container->mByteUint4.FullUint <<= 8;
      }

      container->mByteUint1.Byte3 >>= 1;         // rotate R1B>R15 twice
      if ((helper & 1) != 0)
          container->mByteUint1.Byte3 |= 0x80;
      helper >>= 1;

      container->mByteUint1.Byte3 >>= 1;
      if ((helper & 1) != 0)
          container->mByteUint1.Byte3 |= 0x80;
      helper >>= 1;
  } // end of inner loop
}

void Dcf77::DoPbox(struct DataContainer* container) {
  uint32_t tmp = (uint32_t)0x00000001;         // and set bits from 1C-1E (result from F)

  container->mByteUint1.FullUint = (uint32_t)0xFF000000;     // clear 12-14
  for (int i = 0; i < 20; i++) {
      if ((container->mByteUint4.FullUint & mUintArrBitPattern20[i]) != 0)
          container->mByteUint1.FullUint |= tmp;
      tmp <<= 1;
  }
}


void Dcf77::Decrypt(byte* cipher, byte* key, byte* result) {
  DataContainer container;
  
  CopyTimeToByteUint(cipher, key, &container);
  
  // OUTER LOOP 1
  for (int i = 16; i > 0; i--) {
    ShiftTimeRight(i, &container);
    ExpandR(&container);
    CompressKey(&container);
    
    // expR XOR compr.Key
    container.mByteUint1.FullUint ^= container.mByteUint3.FullUint; // 12-15 XOR 0B-0E
    container.mByteUint3.Byte2 &= 0x0F;       // clear 0D(4-7)
    
    DoSbox(&container);
    DoPbox(&container);
    
    // L XOR P-Boxed Round-Key (L')
    container.mByteUint1.FullUint ^= container.mByteUint2.FullUint;
    
    // L = R
    container.mByteUint2.FullUint = container.mByteUint3.FullUint & (uint32_t)0x00FFFFFF;
    
    // R = L'
    container.mByteUint3.FullUint = container.mByteUint1.FullUint & (uint32_t)0x00FFFFFF;
  } // end of outer loop 1
  
  container.mByteUint3.FullUint <<= 4;
  container.mByteUint2.Byte2 &= 0x0F;
  container.mByteUint2.Byte2 |= (byte)(container.mByteUint3.Byte0 & 0xF0);
  
  //R0B0C0D0E.byte.R0D |= (R08090A.byte.R08 & 0xF0);
  result[0] = container.mByteUint2.Byte0;
  result[1] = container.mByteUint2.Byte1;
  result[2] = container.mByteUint2.Byte2;
  result[3] = container.mByteUint3.Byte1;
  result[4] = container.mByteUint3.Byte2;
}


byte Dcf77::getArea(DateTime aTime)  {
  unsigned int minutes = getMinutesSince2200(aTime);
  // each block of data takes 3 minutes
  // in total 60 areas
  int area = minutes%(60*3);
  area /= 3;
  area--;
  return area;
}

byte Dcf77::getSection(DateTime aTime) {
  unsigned int minutes = getMinutesSince2200(aTime);
  // each block of data takes 3 minutes
  // in total 60 areas
  int section = minutes/(60*3);
  return section;
}

unsigned int Dcf77::getMinutesSince2200(DateTime aTime) {
  int hours = aTime.hh;
  hours--;    // CET -> UTC
  //if (aTime.dst)  // correction DST
  if( IsDst(aTime))
    hours--;
  hours -= 22;
  if (hours<0)
    hours += 24;
  int minutes = aTime.mm + hours*60;

  return minutes;
}
