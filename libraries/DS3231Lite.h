#ifndef DS3231Lite_H
#define DS3231Lite_H

#include <Arduino.h> 
#include <Wire.h> 

#define DS3231_address			(0xD0 >> 1)  

#define DS3231_REG_TIME             (0x00)
#define DS3231_REG_ALARM_1          (0x07)
#define DS3231_REG_ALARM_2          (0x0B)
#define DS3231_REG_CONTROL          (0x0E)
#define DS3231_REG_STATUS           (0x0F)
#define DS3231_REG_TEMPERATURE      (0x11)

#ifndef RTCDATETIME_STRUCT_H
#define RTCDATETIME_STRUCT_H

struct RTCDateTime
{
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
};

struct RTCAlarmTime
{
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
};
#endif

typedef enum
{
    DS3231_1HZ          = 0x00,
    DS3231_4096HZ       = 0x01,
    DS3231_8192HZ       = 0x02,
    DS3231_32768HZ      = 0x03
} DS3231_sqw_t;

typedef enum
{
    DS3231_EVERY_SECOND   = 0b00001111,
    DS3231_MATCH_S        = 0b00001110,
    DS3231_MATCH_M_S      = 0b00001100,
    DS3231_MATCH_H_M_S    = 0b00001000,
    DS3231_MATCH_DT_H_M_S = 0b00000000,
    DS3231_MATCH_DY_H_M_S = 0b00010000
} DS3231_alarm1_t;

typedef enum
{
    DS3231_EVERY_MINUTE   = 0b00001110,
    DS3231_MATCH_M        = 0b00001100,
    DS3231_MATCH_H_M      = 0b00001000,
    DS3231_MATCH_DT_H_M   = 0b00000000,
    DS3231_MATCH_DY_H_M   = 0b00010000
} DS3231_alarm2_t;

class DS3231Lite
{
  private:
    TwoWire* communication;
    RTCDateTime t;
    uint8_t bcd2dec(uint8_t bcd);
    uint8_t dec2bcd(uint8_t dec);
    uint8_t dow(uint16_t y, uint8_t m, uint8_t d);
    void writeRegister8(uint8_t reg, uint8_t value);
    uint8_t readRegister8(uint8_t reg);

  public:
    bool begin(TwoWire* com_wire);
    void setDateTime(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second);
    RTCDateTime getDateTime(void);
    void forceConversion(void);
    float readTemperature(void);
    void setAging(int8_t aging);
};

#endif