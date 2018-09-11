#include "DS3231Lite.h"

const uint8_t daysArray [] PROGMEM = { 31,28,31,30,31,30,31,31,30,31,30,31 };
const uint8_t dowArray[] PROGMEM = { 0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4 };

//char buffer[255];

bool DS3231Lite::begin(TwoWire* com_wire)
{
  communication = com_wire;
  communication->begin();
  t.year = 2018;
  t.month = 1;
  t.day = 1;
  t.hour = 0;
  t.minute = 0;
  t.second = 0;
  return true;
}

void DS3231Lite::setDateTime(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second)
{
  communication->beginTransmission(DS3231_address);
  communication->write(DS3231_REG_TIME);
  communication->write(dec2bcd(second));
  communication->write(dec2bcd(minute));
  communication->write(dec2bcd(hour));
  communication->write(dec2bcd(dow(year, month, day)));
  communication->write(dec2bcd(day));
  communication->write(dec2bcd(month));
  communication->write(dec2bcd(year-2000));
  communication->write(0);
  communication->endTransmission();
}

RTCDateTime DS3231Lite::getDateTime(void)
{
int values[7];
int i;
//------------------------------
  communication->beginTransmission(DS3231_address);
  communication->write(DS3231_REG_TIME);
  communication->endTransmission();
  communication->requestFrom(DS3231_address, 7);
  while(!communication->available()) {};
  for (i = 0; i < 7; i++)
    values[i] = bcd2dec(communication->read());
  communication->endTransmission();
  t.second = values[0];
  t.minute = values[1];
  t.hour = values[2];
//  t.dayOfWeek = values[3];
  t.day = values[4];
  t.month = values[5];
  t.year = values[6] + 2000;
  return t;
}

void DS3231Lite::forceConversion(void)
{
uint8_t value;
//------------------------------
  value = readRegister8(DS3231_REG_CONTROL);
  value |= 0b00100000;
  writeRegister8(DS3231_REG_CONTROL, value);
  do {} while ( (readRegister8(DS3231_REG_CONTROL) & 0b00100000) != 0 );
}

float DS3231Lite::readTemperature(void)
{
uint8_t msb, lsb;
//------------------------------
  communication->beginTransmission(DS3231_address);
  communication->write(DS3231_REG_TEMPERATURE);
  communication->endTransmission();
  communication->requestFrom(DS3231_address, 2);
  while ( !communication->available() ) {};
  msb = communication->read();
  lsb = communication->read();
  return ((((short)msb << 8) | (short)lsb) >> 6) / 4.0f;
}

uint8_t DS3231Lite::bcd2dec(uint8_t bcd)
{
  return ((bcd / 16) * 10) + (bcd % 16);
}

uint8_t DS3231Lite::dec2bcd(uint8_t dec)
{
  return ((dec / 10) * 16) + (dec % 10);
}

uint8_t DS3231Lite::dow(uint16_t y, uint8_t m, uint8_t d)
{
uint8_t dow;
//------------------------------
  y -= m < 3;
  dow = ((y + y/4 - y/100 + y/400 + pgm_read_byte(dowArray+(m-1)) + d) % 7);
  if ( dow == 0 ) return 7;
  return dow;
}

void DS3231Lite::writeRegister8(uint8_t reg, uint8_t value)
{
  communication->beginTransmission(DS3231_address);
  communication->write(reg);
  communication->write(value);
  communication->endTransmission();
}

uint8_t DS3231Lite::readRegister8(uint8_t reg)
{
uint8_t value;
//------------------------------
  communication->beginTransmission(DS3231_address);
  communication->write(reg);
  communication->endTransmission();
  communication->requestFrom(DS3231_address, 1);
  while ( !communication->available() ) {};
  value = communication->read();
  communication->endTransmission();
  return value;
}
