#include <SPI.h>
#include <OneWire.h>
#include <Wire.h>
#include <AM2320.h>
#include <HP203B.h>
#include <DS3231.h>
#include <SD.h>
#include <avr/eeprom.h>

typedef struct _Win
{
  byte WinTempSens;     // 'A' Номер датчика для форточки
  int  WinOpenTemp;     // 'B' Температура открытия для форточки
  int  WinCloseTemp;    // 'C' Температура закрытия для форточки
  byte WinOpenOutPort;  // 'D' Номер выходного регистра открытия для форточки
  byte WinOpenOutPin;   // 'D' Выход регистра открытия для форточки
  byte WinCloseOutPort; // 'E' Номер выходного регистра закрытия для форточки
  byte WinCloseOutPin;  // 'E' Выход регистра закрытия для форточки
  byte WinOpenInPort;   // 'F' Номер входного регистра открытия для форточки
  byte WinOpenInPin;    // 'F' Вход регистра открытия для форточки
  byte WinCloseInPort;  // 'G' Номер входного регистра закрытия для форточки
  byte WinCloseInPin;   // 'G' Вход регистра закрытия для форточки
  byte WinRecCRC;       // контрольная сумма
} Win;

typedef struct _RTC
{
  byte isEnable;        // подключение DS3231
  uint8_t Seconds;      // 0x00 секунды в BCD маска 0x7F 00-59
  uint8_t Minutes;      // 0x01 минуты в BCD маска 0x7F 00-59
  uint8_t Hours;        // 0x02 часы в BCD маска 0x3F 00-23
  uint8_t Day;          // 0x03 день в BCD маска 0x07 1-7
  uint8_t Date;         // 0x04 число в BCD маска 0x3F 01-31
  uint8_t Month;        // 0x05 месяц в BCD маска 0x1F 01-12
  uint8_t Year;         // 0x06 год в BCD маска 0xFF 00-99
  char DateTime[17];    // строка времени 00:00:00 00-00-00
} RTC;
#define DS3231_address  (0xD0 >> 1)  

// описание управления форточками
const Win Wins[] PROGMEM = {
//Sens TOpen TClose OOReg OOPin COReg COPin OIReg OIpin CIReg CIPin  CRC
  {9,  300,  270,    0,    0,    0,    1,    0,    0,    0,    1,    0},
  {9,  300,  270,    0,    2,    0,    3,    0,    2,    0,    3,    0},
  {9,  300,  270,    0,    4,    0,    5,    0,    4,    0,    5,    0},
  {9,  300,  270,    0,    6,    0,    7,    0,    6,    0,    7,    0},
  {9,  330,  240,    1,    0,    1,    1,    1,    0,    1,    1,    0},
  {9,  330,  240,    1,    2,    1,    3,    1,    2,    1,    3,    0},
  {9,  330,  240,    1,    4,    1,    5,    1,    4,    1,    5,    0},
  {9,  330,  240,    1,    6,    1,    7,    1,    6,    1,    7,    0}
};

// описание выводов Arduino
//#define TXD             0  // выход данных на UART
//#define RXD             1  // вход данных на UART
//#define Unused          2  // 
#define OneWirePin        3  // вывод на OW
#define SelAddr0Pin       4  // SA0 селектор адреса (0..7)
#define SelAddr1Pin       5  // SA1
#define SelAddr2Pin       6  // SA2
#define ExAddrMsk         0x07 // 3 bits
//#define Unused          7  // 
#define SDCSelect         8  // 
#define ExRegsOutEnblPin  9  // номер выхода на включение ExRegs (активный LOW)
#define slaveSelectPin    10 // SPI Slave Select Pin (CS/LATCH/LOAD/SHIFT)
//#define SPIMOSI         11 // выход данных на SPI
//#define SPIMISO         12 // вход данных на SPI
//#define SPISCK          13 // тактовый выход данных на SPI
#define HumSensPin0       A0 // ADC канал для датчиков влажности почвы
#define HumSensPin1       A1 // ADC канал для датчиков влажности почвы
//#define Unused          A2 // 
//#define Unused          A3 // 
//#define I2CSDA          A4 // 
//#define I2CSCL          A5 // 
//#define Unused          A6 // ADC
//#define Unused          A7 // ADC

// описание типов датчиков
AM2320 th;                // датчики влажности и температуры
HP203B tp;                // датчики давления и температуры
OneWire ds(OneWirePin);   // 4.7K подтяжка к питанию
DS3231 clock;
RTCDateTime dt;

// массивы данных с датчиков
byte SensorsEnbl[8];        // [8 линий]
int Temperatura[8][8];      // [8 датчиков][8 линий]
int TemperaturaPrev[8][8];  // [8 датчиков][8 линий]
int Hum[8];                 // [8 линий]
int HumPrev[8];             // [8 линий]
int Press[8];               // [8 линий]
int PressPrev[8];           // [8 линий]
#define GetSensTemper(SensNum)      Temperatura[(SensNum) % 8][(SensNum) / 8]
#define GetSensTemperPrev(SensNum)  TemperaturaPrev[(SensNum) % 8][(SensNum) / 8]
#define GetSensHum(SensNum)         Hum[(SensNum) / 8]
#define GetSensHumPrev(SensNum)     HumPrev[(SensNum) / 8]
#define GetSensPress(SensNum)       Press[(SensNum) / 8]
#define GetSensPressPrev(SensNum)   PressPrev[(SensNum) / 8]

// Для диспетчера задач
#define TaskAM      1   // получить данные с датчика AM2320
#define TaskHP      2   // получить данные с датчика HP203B
#define TaskDS      3   // получить данные с датчика DS18B20
#define TaskADC     4   // получить данные с ADC
#define TaskEnd     5   // перезапуск задач
byte Task = TaskAM;     // индекс задачи
unsigned long TimeTask; // время для Task

// Для OW
#define OWTaskSearch  1   // поиск на шине OW
#define OWTaskStart   2   // запуск измерения
#define OWTaskWait    3   // ожидание окончания измерения
#define OWTaskRead    4   // чтение и публикация данных
byte OWTask = OWTaskSearch; // менеджер задач для OW
unsigned long TimeOW;     // время для OW
byte addr[8];             // текущий адрес OW из ROM
byte ID[8][6][6];         // массив ID датчиков DS18B20 [8 линий][6 датчиков][6 hex]
byte ds_cnt = 0;          // счетчик найденых датчиков

// Для селектора адреса
byte ExAddr = 0;       // внешний адрес для адресуемых устройств (OW/I2C/ADC)

// Для ExRegs
#define ExRegsOutCnt       6        // кол-во внешних 8-ми битных регистров Out
byte ExRegsOut[ExRegsOutCnt];       // массив данных внешних выходных 8-ми битных регистров (текущее состояние)
byte ExRegsOutPrev[ExRegsOutCnt];   // массив данных внешних выходных 8-ми битных регистров (предыдущее состояние)
byte ExRegsOutChange[ExRegsOutCnt]; // массив данных внешних выходных 8-ми битных регистров (изменение состояние)
#define ExRegsInCnt        6        // кол-во внешних 8-ми битных регистров In
byte ExRegsIn[ExRegsInCnt];         // массив данных внешних входных 8-ми битных регистров (текущее состояние)
byte ExRegsInPrev[ExRegsInCnt];     // массив данных внешних входных 8-ми битных регистров (предыдущее состояние)
byte ExRegsInChange[ExRegsInCnt];   // массив данных внешних входных 8-ми битных регистров (изменение состояние)
unsigned long TimeExRegs;           // таймер для внешних регистров

byte SDCEnable = 0;     // признак наличия SD
byte isLoger = 0;       // запрос на запись данных

// для управления через терминал
char CmndLine[16];    // командная строка
byte CmndCnt;         // счетчик принятых символов

// для вывода данных на терминал
uint8_t OutScrEnbl;

#define Color 1       // разрешение цветного вывода на терминал (для Putty)
// описание строк для вывода в терминал
// управляющие символы для цвета
#ifdef Color
#define NormalStr     F("\x1b[0m")
#define BoldStr       F("\x1b[1m")
#define BoldOffStr    F("\x1b[22m")
#define BlackStr      F("\x1b[30m")
#define RedStr        F("\x1b[31m")
#define GreenStr      F("\x1b[32m")
#define BlueStr       F("\x1b[34m")
#define WhiteFill     F("\x1b[47m")
#define NewScr        F("\f")
#else
#define NormalStr 
#define BoldStr   
#define BoldOffStr
#define BlackStr
#define RedStr
#define GreenStr
#define BlueStr
#define WhiteFill
#define NewScr
#endif  
// строки
#define UPStr   F("▲")
#define DNStr   F("▼")

/*******************************************************************************
* 
* Работа с часами
* 
*******************************************************************************/

/*******************************************************************************
* 
* Управление периферией
* 
*******************************************************************************/
// ---=== Настраиваем входы и выходы ===---
void io_setup() 
{
  digitalWrite(SelAddr0Pin, LOW);       // селектор адреса
  digitalWrite(SelAddr1Pin, LOW);
  digitalWrite(SelAddr2Pin, LOW);
  digitalWrite(ExRegsOutEnblPin, HIGH); // включение ExRegs (пока выключен)

  pinMode(SelAddr0Pin, OUTPUT);         // селектор адреса
  pinMode(SelAddr1Pin, OUTPUT);
  pinMode(SelAddr2Pin, OUTPUT);
  pinMode(ExRegsOutEnblPin, OUTPUT);    // включение ExRegs
  pinMode(slaveSelectPin, OUTPUT);      // SPI Slave Select Pin
}

// ---=== устанавливаем внешний адрес ===---
void SetExAddr(void)
{
  if (ExAddr & 0x01) digitalWrite(SelAddr0Pin, HIGH);
  else               digitalWrite(SelAddr0Pin, LOW);
  if (ExAddr & 0x02) digitalWrite(SelAddr1Pin, HIGH);
  else               digitalWrite(SelAddr1Pin, LOW);
  if (ExAddr & 0x04) digitalWrite(SelAddr2Pin, HIGH);
  else               digitalWrite(SelAddr2Pin, LOW);
}

// ---=== Чтение и запись данных внешних регистров ===---
void ExRegsReadWrite()
{
byte i;
//--------------------------
  // 1. защелкнуть значения в регистры IN и записать регистры OUT
  digitalWrite(slaveSelectPin, LOW);    // SS в 0 => select chip - для OUT / load - для IN
  for (i = 0; i < ExRegsOutCnt; i++)
    SPI.transfer(~ExRegsOut[ExRegsOutCnt-1-i]); // отправляем данные на внешние регистры начиная со старшего/дальнего (инвертируем т.к. активный LOW)
  // 2. защелкнуть значения в регистры OUT и регистры IN
  digitalWrite(slaveSelectPin, HIGH);   // SS в 1 => latch - для OUT / shift - для IN
  // 3. считать значения из регистров IN
  for (i = 0; i < ExRegsInCnt; i++)
    ExRegsIn[i] = ~SPI.transfer(0xFF);  // читаем данные с внешних регистров начиная с младшего/ближнего (инвертируем т.к. активный LOW)
  TimeExRegs = millis();    // время последнего обмена на ExRegs
}

// ---=== Инициализируем ExRegs ===---
void init_ExRegs(void)
{
byte i;
//--------------------------
  for (i = 0; i < ExRegsOutCnt; i++)    // выходные регистры
    ExRegsOut[i] = 0x00;
  for (i = 0; i < ExRegsInCnt; i++)     // входные регистры
  {
    ExRegsInPrev[i] = 0x00;
    ExRegsInChange[i] = 0x00;
  }
  ExRegsReadWrite();                    // Чтение и запись данных внешних регистров
  digitalWrite(ExRegsOutEnblPin, LOW);  // включение выходов ExRegsOut
}

// ---=== обновляем данные в регистрах ===---
void io_poll() 
{
byte i;
//--------------------------
  if ((millis() - TimeExRegs) > 100)  // через 100 мс
  {
    ExRegsReadWrite();  // Чтение и запись данных внешних регистров
    // проверить изменения на выходах
    for (i = 0; i < ExRegsOutCnt; i++)
    {
      ExRegsOutChange[i] = (ExRegsOut[i] ^ ExRegsOutPrev[i]) | ExRegsOutChange[i];
      ExRegsOutPrev[i] = ExRegsOut[i];
    }
    // проверить изменения на входах
    for (i = 0; i < ExRegsInCnt; i++)
    {
      ExRegsInChange[i] = (ExRegsIn[i] ^ ExRegsInPrev[i]) | ExRegsInChange[i];
      ExRegsInPrev[i] = ExRegsIn[i];
    }
  }
}

/*******************************************************************************
* 
* Управление датчиками
* 
*******************************************************************************/
// ---=== обрабатываем датчики на I2C & OW & ADC ===---
void Sens_poll()
{
byte i, j;
byte data[12];
int16_t raw;
float celsius;
//int ADCVal;
//--------------------------
  if ((millis() - TimeTask) < 2000) return; // 2 сек минимум

  switch (Task) // выбор текущей задачи
  {
    //-------------------------------------------------------
    // получить данные с датчика AM2320
    case TaskAM:
      if (th.Read() == 0) 
      { // получены данные с датчика AM2320, публикуем их
        SensorsEnbl[ExAddr] |= 1; // найден датчик
        Temperatura[0][ExAddr] = th.cTemp * 10;           // температура от AM2320 в 0 ячейке
        TemperaturaPrev[0][ExAddr] = (TemperaturaPrev[0][ExAddr] + Temperatura[0][ExAddr]) / 2; // сохраняем предыдущую температуру
        Hum[ExAddr] = th.Humidity * 10;           // влажность от AM2320
        HumPrev[ExAddr] = (HumPrev[ExAddr] + Hum[ExAddr]) / 2; // сохраняем предыдущую влажность
      }
      else
      {
        SensorsEnbl[ExAddr] &= ~1; // не найден датчик
        TemperaturaPrev[0][ExAddr] = 0;
        Temperatura[0][ExAddr] = 0;
        HumPrev[ExAddr] = 0;
        Hum[ExAddr] = 0;
      }
      Task = TaskHP; // переходим к следующей задаче
      TimeTask = millis();  // время таймаута для задач
      break;

    //-------------------------------------------------------
    // получить данные с датчика HP203B
    case TaskHP:     
      if (tp.Read() == 0) 
      { // получены данные с датчика HP203B, публикуем их
        SensorsEnbl[ExAddr] |= 2; // найден датчик
        Temperatura[1][ExAddr] = tp.cTemp * 10;           // температура от HP203B в 1 ячейке
        TemperaturaPrev[1][ExAddr] = (TemperaturaPrev[1][ExAddr] + Temperatura[1][ExAddr]) / 2; // сохраняем предыдущую температуру
        Press[ExAddr] = tp.Pressure;           // давление от HP203B
        PressPrev[ExAddr] = (PressPrev[ExAddr] + Press[ExAddr]) / 2; // сохраняем предыдущее давление
      }
      else
      {
        SensorsEnbl[ExAddr] &= ~2; // не найден датчик
        TemperaturaPrev[1][ExAddr] = 0;
        Temperatura[1][ExAddr] = 0;
        Press[ExAddr] = 0;
        PressPrev[ExAddr] = 0;
      }
      Task = TaskDS; // переходим к следующей задаче
      TimeTask = millis();  // время таймаута для задач
      break;

    //-------------------------------------------------------
    // получить данные с датчика DS18B20
    case TaskDS:
      switch (OWTask) // выбор задачи OW
      {
        //-----------------------------------------
        // поиск датчиков
        case OWTaskSearch:   
          if ( (ds.search(addr)) && (ds_cnt < 6) )    // поиск на шине (сохраняет адрес)
            OWTask = OWTaskStart; // есть датчик, переходим к следующей задаче OW
          else
          { // нет датчиков или все уже найдены
            for (i = ds_cnt; i < 6; i++)
            {
              SensorsEnbl[ExAddr] &= ~(1 << (i+2));
              TemperaturaPrev[i+2][ExAddr] = 0;
              Temperatura[i+2][ExAddr] = 0;
              for (j = 0; j < 6; j++) 
                ID[ExAddr][i][j] = 0; // массив ID датчиков DS18B20 [8 линий][6 датчиков][6 hex]
            }
            ds_cnt = 0;
            ds.reset_search();    // сбрасываем поиск
            Task = TaskADC;       // переходим к следующей задаче
            TimeTask = millis();  // время таймаута для задач
          }
          break;

        //-----------------------------------------
        // запуск измерения
        case OWTaskStart: 
          ds.reset();
          ds.select(addr);
          ds.write(0x44, 1);      // start conversion, with parasite power on at the end
          OWTask = OWTaskWait;    // переходим к следующей задаче OW
          TimeOW = millis();      // время таймаута для переобразования
          break;

        //-----------------------------------------
        // ожидание окончания преобразования
        case OWTaskWait: 
          if ((millis() - TimeOW) > 1000) // ждем не менее 1 сек
          { // время вышло
            OWTask = OWTaskRead;   // переходим к следующей задаче OW
          }
          break;

        //-----------------------------------------
        // получаем данные с датчика DS18B20, публикуем их
        case OWTaskRead: 
          ds.reset();
          ds.select(addr);    
          ds.write(0xBE);         // Read Scratchpad
          for (i = 0; i < 2; i++) 
            data[i] = ds.read();  // читаем только температуру
          raw = (data[1] << 8) | data[0];
          celsius = (float)raw / 16.0;
          SensorsEnbl[ExAddr] |= (1 << (2 + ds_cnt));
          Temperatura[2 + ds_cnt][ExAddr] = celsius * 10;
          TemperaturaPrev[2 + ds_cnt][ExAddr] = (TemperaturaPrev[2 + ds_cnt][ExAddr] + Temperatura[2 + ds_cnt][ExAddr]) / 2; // сохраняем предыдущую температуру
          // сохраняем ID
          for (i = 0; i < 6; i++) 
            ID[ExAddr][ds_cnt][i] = addr[i+1];  // массив ID датчиков DS18B20 [8 линий][6 датчиков][6 hex]
          ds_cnt++;
          OWTask = OWTaskSearch;   // переходим к началу OW (поиск очередного датчика)
          break;

        default:
          ds_cnt = 0;
          ds.reset_search();    // сбрасываем поиск
          OWTask = OWTaskSearch;   // переходим к началу OW (поиск очередного датчика)
          break;
      }
      break;

    //-------------------------------------------------------
    // получить данные с ADC датчики влажности почвы, публикуем их
    case TaskADC:
/*
      // читаем значение с ADC0
      ADCVal = analogRead(HumSensPin0);
      // читаем значение с ADC1
      ADCVal = analogRead(HumSensPin1);
*/
      Task = TaskEnd; // переходим к следующей задаче
      TimeTask = millis();  // время таймаута для задач
      break;

    //-------------------------------------------------------
    // установить следующий адрес датчиков, установить паузу
    case TaskEnd:     
      ExAddr = (ExAddr + 1) & ExAddrMsk;
      SetExAddr();          // устанавливаем внешний адрес
      if (ExAddr == 0) isLoger = 1; // запрос на запись данных
      Task = TaskAM;        // переходим к началу
      TimeTask = millis();  // время таймаута для задач
      OutScrEnbl = 1;
      break;

    default:
      Task = TaskAM;        // переходим к началу
      TimeTask = millis();  // время таймаута для задач
      break;
  }
}

// ---=== возвращает наличее датчика ===---
// SensNum - 0..63
byte IsSensEnbl(byte SensNum)
{
  if (SensorsEnbl[SensNum / 8] & (1 << (SensNum % 8))) // проверяем наличее датчика
    return 1;
  else
    return 0;   
}

/*******************************************************************************
* 
* Вывод в терминал
* 
*******************************************************************************/
// ---=== преобразуем значение температуры в массив символов ===---
// 6 символов
// 999 => 99.9; -1 => -0.1;
void PrintTemper(int Val)
{
uint8_t k;
byte data[7];
//--------------------------
  for (k = 0; k < 7; k++)     // очистка массива
    data[k] = ' ';
  k = 0;
  if (Val > 999) Val = 999;   // ограничение
  if (Val < -999) Val = -999; // ограничение
  if (Val < 0)
  { // при отрицательных значениях
    data[k++] = '-';
    Val = -Val;
  }
  if (Val > 99) 
    data[k++] = (Val / 100) | 0x30;     // десятки
  data[k++] = ((Val / 10) % 10) | 0x30; // единицы
  data[k++] = '.';
  data[k++] = (Val % 10) | 0x30;        // десятые
  data[k++] = 0xC2;   // расширенный символ
  data[k] = 0xB0;     // выводится как один
  Serial.write(data, 7);
}

// ---=== преобразуем значение влажности в массив символов ===---
// 6 символов
// 999 => 99.9
void PrintHum(int Val)
{
uint8_t k;
byte data[6];
//--------------------------
  for (k = 0; k < 6; k++)   // очистка массива
    data[k] = ' ';
  k = 0;
  if (Val > 999) Val = 999; // ограничение
  if (Val < 0) Val = 0;     // ограничение
  if (Val > 99) 
    data[k++] = (Val / 100) | 0x30;     // десятки
  data[k++] = ((Val / 10) % 10) | 0x30; // единицы
  data[k++] = '.';
  data[k++] = (Val % 10) | 0x30;        // десятые
  data[k] = '%';
  Serial.write(data, 6);
}

// ---=== преобразуем значение давления в массив символов ===---
// 6 символов
// 999 => 999
void PrintPress(int Val)
{
uint8_t k;
byte data[6];
//--------------------------
  for (k = 0; k < 6; k++)   // очистка массива
    data[k] = ' ';
  k = 0;
  if (Val > 999) Val = 999; // ограничение
  if (Val < 0) Val = 0;     // ограничение
  if (Val > 99) 
    data[k++] = (Val / 100) | 0x30;       // сотни
  if (Val > 9) 
    data[k++] = ((Val / 10) % 10) | 0x30; // десятки
  data[k++] = (Val % 10) | 0x30;          // единицы
  data[k++] = 'm';
  data[k] = 'm';
  Serial.write(data, 6);
}

// ---=== вывод значения датчика температуры ===---
// 6 символов
// SensNum - номер датчика 0..63
void PrintSensTemper(byte SensNum)
{
  if (IsSensEnbl(SensNum))
    PrintTemper(GetSensTemper(SensNum));
  else
    Serial.print(F("----  "));
}

// ---=== вывод значения датчика температуры среднее  ===---
// 6 символов
// SensNum - номер датчика 0..63
void PrintSensTemperPrev(byte SensNum)
{
  if (IsSensEnbl(SensNum))
    PrintTemper(GetSensTemperPrev(SensNum));
  else
    Serial.print(F("      "));
}

// ---=== вывод значения датчика влажности ===---
// 6 символов
// SensNum - номер датчика 0..63
void PrintSensHum(byte SensNum)
{
  if (IsSensEnbl(SensNum))
    PrintHum(GetSensHum(SensNum));
  else
    Serial.print(F("      "));
}

// ---=== вывод значения датчика давления ===---
// 6 символов
// SensNum - номер датчика 0..63
void PrintSensPress(byte SensNum)
{
  if (IsSensEnbl(SensNum)) // проверяем наличее датчика
    PrintPress(GetSensPress(SensNum));
  else
    Serial.print(F("      "));
}

// ---=== преобразуем HEX_L в символ ===---
// для преобразования адресов в HEX-символьный формат
char HexToChar(uint8_t hex)
{
char result = hex & 0x0F;
//--------------------------
  if (result < 0x0A) result += 0x30;  // (0..9)
  else result += 0x37;  // (A..F)
  return result;
}

// ---=== вывод ID датчика температуры DS18B20 ===---
// 12 символов
// SensNum - номер датчика 0..63
void PrintSensID(byte SensNum)
{
byte i;
//--------------------------
  if (IsSensEnbl(SensNum)) // проверяем наличее датчика
  {
    for (i = 0; i < 6; i++)
    {
      Serial.print(HexToChar(ID[SensNum / 8][(SensNum % 8) - 2][i] >> 4)); // массив ID датчиков DS18B20 [8 линий][6 датчиков][6 hex]
      Serial.print(HexToChar(ID[SensNum / 8][(SensNum % 8) - 2][i]));
    }
  }
  else
    Serial.print(F("            "));
}

// ---=== вывод направления изменения значения датчика ===---
// 1 символ
// SensNum - номер датчика 0..63 [8 датчиков][8 линий]
// Sens* - указатель на массив значений датчиков на линии
// SensPrev* - указатель на массив средних/предыдущих значений датчиков на линии
void PrintSensDirect(byte SensNum, int* Sens, int* SensPrev)
{
  if (IsSensEnbl(SensNum))  // номер датчика 0..63
  {
    if (Sens[SensNum / 8] > SensPrev[SensNum / 8])  // номер линии для датчика
    {
      Serial.print(RedStr);
      Serial.print(UPStr);
    }
    else if (Sens[SensNum / 8] < SensPrev[SensNum / 8])  // номер линии для датчика
    {
      Serial.print(BlueStr);
      Serial.print(DNStr);
    }
    else
    {
      Serial.print(GreenStr);
      Serial.print("=");
    }
    Serial.print(NormalStr);
  }
  else
    Serial.print(F(" "));
}

// ---=== вывод установленного значения температуры для форточки ===---
// 6 символов
// Winx - номер форточки 0..7
// OpenClose - = 1 температура открытия; = 0 температура закрытия
void PrintTemperWin(byte Winx, byte OpenClose)
{
  if (OpenClose)
  {
    Serial.print(RedStr);
    PrintTemper(eeprom_read_word((uint16_t*)(sizeof(Win) * Winx + 1))); 
  }
  else
  {
    Serial.print(BlueStr);
    PrintTemper(eeprom_read_word((uint16_t*)(sizeof(Win) * Winx + 3))); 
  }
  Serial.print(NormalStr);
}

// ---=== вывод номера датчика ===---
// 7 символов
// Winx - номер форточки 0..7
void PrintSensNumWin(byte Winx)
{
byte WinTempSens;
//--------------------------
  WinTempSens = eeprom_read_byte((byte*)(sizeof(Win) * Winx));  // читаем номер датчика из EEPROM
  Serial.print(F("Temp"));
  Serial.print(WinTempSens / 8);  // номер линии/адреса
  Serial.print(F("."));
  Serial.print(WinTempSens % 8);  // номер датчика на линии
}

// ---=== Вывод текущего значения датчика температуры для форточек ===---
// 7 символов
// Winx - номер форточки 0..7
void PrintSensWinTemper(byte Winx)
{
byte WinTempSens;
int WinOpenTemp;
int WinCloseTemp;
//--------------------------
  WinTempSens = eeprom_read_byte((byte*)(sizeof(Win) * Winx));
  WinOpenTemp = eeprom_read_word((uint16_t*)(sizeof(Win) * Winx + 1));
  WinCloseTemp = eeprom_read_word((uint16_t*)(sizeof(Win) * Winx + 3));
  if (IsSensEnbl(WinTempSens)) // проверяем наличее датчика
  {
    PrintSensDirect(WinTempSens, Temperatura[WinTempSens % 8], TemperaturaPrev[WinTempSens % 8]);
    if (GetSensTemper(WinTempSens) > WinOpenTemp)
      Serial.print(RedStr);
    else if (GetSensTemper(WinTempSens) < WinCloseTemp)
      Serial.print(BlueStr);
    else
      Serial.print(GreenStr);
    PrintSensTemper(WinTempSens);
    Serial.print(NormalStr);
  }
  else
    Serial.print(F(" None  "));
}

// ---=== вывод состояния форточки ===---
// 7 символов
// Winx - номер форточки 0..7
void PrintStateWin(byte Winx)
{
byte WinOpenOutPort;
byte WinOpenOutPin;
byte WinCloseOutPort;
byte WinCloseOutPin;
byte WinOpenInPort;
byte WinOpenInPin;
byte WinCloseInPort;
byte WinCloseInPin;
//--------------------------
  WinOpenOutPort = eeprom_read_byte((byte*)(sizeof(Win) * Winx + 5));
  WinOpenOutPin = eeprom_read_byte((byte*)(sizeof(Win) * Winx + 6));
  WinCloseOutPort = eeprom_read_byte((byte*)(sizeof(Win) * Winx + 7));
  WinCloseOutPin = eeprom_read_byte((byte*)(sizeof(Win) * Winx + 8));
  WinOpenInPort = eeprom_read_byte((byte*)(sizeof(Win) * Winx + 9));
  WinOpenInPin = eeprom_read_byte((byte*)(sizeof(Win) * Winx + 10));
  WinCloseInPort = eeprom_read_byte((byte*)(sizeof(Win) * Winx + 11));
  WinCloseInPin = eeprom_read_byte((byte*)(sizeof(Win) * Winx + 12));
  if ( (ExRegsOut[WinOpenOutPort] & (1 << WinOpenOutPin)) && !(ExRegsOut[WinCloseOutPort] & (1 << WinCloseOutPin)) )
  {
    Serial.print(RedStr);
    if ( !(ExRegsIn[WinCloseInPort] & (1 << WinCloseInPin)) ^ !(ExRegsIn[WinOpenInPort] & (1 << WinOpenInPin)) )
      Serial.print(F("Opening"));
    else
    {
      Serial.print(BoldStr);
      Serial.print(F(" Open  "));
    }
    Serial.print(NormalStr);
  }
  else if ( !(ExRegsOut[WinOpenOutPort] & (1 << WinOpenOutPin)) && (ExRegsOut[WinCloseOutPort] & (1 << WinCloseOutPin)) )
  {
    Serial.print(BlueStr);
    if ( !(ExRegsIn[WinCloseInPort] & (1 << WinCloseInPin)) ^ !(ExRegsIn[WinOpenInPort] & (1 << WinOpenInPin)) )
      Serial.print(F("Closing"));
    else
    {
      Serial.print(BoldStr);
      Serial.print(F(" Close "));
    }
    Serial.print(NormalStr);
  }
  else      
    Serial.print(F("Unknown"));
}

// ---=== вывод выходных портов и пинов ===---
// 7 символов
// Winx - номер форточки 0..7
void PrintOutPortPinWin(byte Winx)
{
byte WinOpenOutPort;
byte WinOpenOutPin;
byte WinCloseOutPort;
byte WinCloseOutPin;
//--------------------------
  WinOpenOutPort = eeprom_read_byte((byte*)(sizeof(Win) * Winx + 5));
  WinOpenOutPin = eeprom_read_byte((byte*)(sizeof(Win) * Winx + 6));
  WinCloseOutPort = eeprom_read_byte((byte*)(sizeof(Win) * Winx + 7));
  WinCloseOutPin = eeprom_read_byte((byte*)(sizeof(Win) * Winx + 8));
  Serial.print(WinOpenOutPort);
  Serial.print(WinOpenOutPin);
  Serial.print(BoldStr);
  if (ExRegsOut[WinOpenOutPort] & (1 << WinOpenOutPin)) Serial.print(F("1"));
  else Serial.print(F("0"));
  Serial.print(NormalStr);
  Serial.print(F(" "));
  Serial.print(WinCloseOutPort);
  Serial.print(WinCloseOutPin);
  Serial.print(BoldStr);
  if (ExRegsOut[WinCloseOutPort] & (1 << WinCloseOutPin)) Serial.print(F("1"));
  else Serial.print(F("0"));
  Serial.print(NormalStr);
}

// ---=== вывод входных портов и пинов ===---
// 7 символов
// Winx - номер форточки 0..7
void PrintInPortPinWin(byte Winx)
{
byte WinOpenInPort =  eeprom_read_byte((byte*)(sizeof(Win) * Winx + 9));
byte WinOpenInPin =   eeprom_read_byte((byte*)(sizeof(Win) * Winx + 10));
byte WinCloseInPort = eeprom_read_byte((byte*)(sizeof(Win) * Winx + 11));
byte WinCloseInPin =  eeprom_read_byte((byte*)(sizeof(Win) * Winx + 12));
//--------------------------
  Serial.print(WinOpenInPort);
  Serial.print(WinOpenInPin);
  Serial.print(BoldStr);
  if (ExRegsIn[WinOpenInPort] & (1 << WinOpenInPin)) Serial.print(F("1"));
  else  Serial.print(F("0"));
  Serial.print(NormalStr);
  Serial.print(F(" "));
  Serial.print(WinCloseInPort);
  Serial.print(WinCloseInPin);
  Serial.print(BoldStr);
  if (ExRegsIn[WinCloseInPort] & (1 << WinCloseInPin)) Serial.print(F("1"));
  else Serial.print(F("0"));
  Serial.print(NormalStr);
}

/*******************************************************************************
* 
* Управление форточками
* 
*******************************************************************************/
// ---=== Контроль температуры и включение приводов форточек ===---
// Winx - номер форточки 0..7
void WinControl(byte Winx)
{
byte WinTempSens;
int WinOpenTemp;
int WinCloseTemp;
byte WinOpenOutPort;
byte WinOpenOutPin;
byte WinCloseOutPort;
byte WinCloseOutPin;
//--------------------------
  WinTempSens = eeprom_read_byte((byte*)(sizeof(Win) * Winx + 0));
  WinOpenTemp = eeprom_read_word((uint16_t*)(sizeof(Win) * Winx + 1));
  WinCloseTemp = eeprom_read_word((uint16_t*)(sizeof(Win) * Winx + 3));
  WinOpenOutPort = eeprom_read_byte((byte*)(sizeof(Win) * Winx + 5));
  WinOpenOutPin = eeprom_read_byte((byte*)(sizeof(Win) * Winx + 6));
  WinCloseOutPort = eeprom_read_byte((byte*)(sizeof(Win) * Winx + 7));
  WinCloseOutPin = eeprom_read_byte((byte*)(sizeof(Win) * Winx + 8));
  if (IsSensEnbl(WinTempSens)) // проверяем наличее датчика
  {
    if (GetSensTemper(WinTempSens) > WinOpenTemp)
    { // Open
      ExRegsOut[WinOpenOutPort] |= (1 << WinOpenOutPin);
      ExRegsOut[WinCloseOutPort] &= ~(1 << WinCloseOutPin);
    }
    else if (GetSensTemper(WinTempSens) < WinCloseTemp)
    { // Close
      ExRegsOut[WinCloseOutPort] |= (1 << WinCloseOutPin);
      ExRegsOut[WinOpenOutPort] &= ~(1 << WinOpenOutPin);
    }
  }
}

// ---=== контроль температуры и управление форточками ===---
void win_ctrl(void)
{
byte Winx;
//--------------------------
  for (Winx = 0; Winx < (sizeof(Wins)/sizeof(Win)); Winx++)
    WinControl(Winx);
}

/*******************************************************************************
* 
* работа с териминалом
* 
*******************************************************************************/
// ---=== вывод строки заголовка ===---
void PrintSensLine(void)
{
byte i;
//--------------------------
  Serial.print(BlackStr);
  Serial.print(WhiteFill);
  Serial.print(F("Sens   "));
  for (i = 0; i < 8; i++)
  {
    Serial.print(F("  "));
    if (ExAddr == i) 
    {
      Serial.print(BoldStr);
      Serial.print(RedStr);
    }
    Serial.print(F(" Line")); 
    Serial.print(i); 
    Serial.print(F(" "));
    if (ExAddr == i)
    {
      Serial.print(BlackStr);
      Serial.print(BoldOffStr);
    }
    Serial.print(F("    "));
  }
  Serial.print(NormalStr);
  Serial.println();
}

// ---=== вывод строки со значениями температуры для датчиков Sens на всех линиях ===---
void PrintTempLine(byte Sens)
{
byte i;
//--------------------------
  Serial.print(F("Temp"));
  Serial.print(Sens);
  Serial.print(F("  "));
  for (i = 0; i < 8; i++) // lines
  {
    Serial.print(F("   "));
    PrintSensDirect(i * 8 + Sens, Temperatura[Sens], TemperaturaPrev[Sens]); // 1 символ
    PrintSensTemper(i * 8 + Sens);  // 6 символов
    Serial.print(F("   "));
  }
  Serial.println();
}

// ---=== вывод строки со значениями температуры для датчиков Sens на всех линиях ===---
void PrintTempPrevLine(byte Sens)
{
byte i;
//--------------------------
  Serial.print(F("Temp"));
  Serial.print(Sens);
  Serial.print(F("  "));
  for (i = 0; i < 8; i++) // lines
  {
    Serial.print(F("    "));
    PrintSensTemperPrev(i * 8 + Sens);  // 6 символов
    Serial.print(F("   "));
  }
  Serial.println();
}

// ---=== вывод строки со значениями влажности на всех линиях ===---
void PrintHumLine(void)
{
byte i;
//--------------------------
  Serial.print(F("Hum0   "));
  for (i = 0; i < 8; i++) // lines
  {
    Serial.print(F("   "));
    PrintSensDirect(i * 8, Hum, HumPrev);
    PrintSensHum(i * 8);
    Serial.print(F("   "));
  }
  Serial.println();
}

// ---=== вывод строки со значениями давления на всех линиях ===---
void PrintPressLine(void)
{
byte i;
//--------------------------
  Serial.print(F("Press1 "));
  for (i = 0; i < 8; i++) // lines
  {
    Serial.print(F("   "));
    PrintSensDirect(i * 8 + 1, Press, PressPrev);
    PrintSensPress(i * 8 + 1);
    Serial.print(F("   "));
  }
  Serial.println();
}

// ---=== вывод строки со значениями ID для датчиков Sens на всех линиях ===---
void PrintIDLine(byte Sens)
{
byte i;
//--------------------------
  Serial.print(F("ID"));
  Serial.print(Sens);
  Serial.print(F("    "));
  for (i = 0; i < 8; i++) // lines
  {
    PrintSensID(i * 8 + Sens);
    Serial.print(F(" "));
  }
  Serial.println();
}

// ---=== вывод строки заголовка ===---
void PrintWinLine(void)
{
byte i;
//--------------------------
  Serial.print(BlackStr);
  Serial.print(WhiteFill);
  Serial.print(F("Params "));
  for (i = 1; i < 9; i++)
  {
    Serial.print(F("  Win")); Serial.print(i); Serial.print(F("   "));
  }
  Serial.print(NormalStr);
  Serial.println();
}

// ---=== вывод строки с номерами датчиков ===---
void PrintSensNumWinLine(void)
{
byte i;
//--------------------------
  Serial.print(F("A "));
  Serial.print(BoldStr);
  Serial.print(F("Sens  "));
  Serial.print(NormalStr);
  for (i = 0; i < 8; i++)
  {
    PrintSensNumWin(i); Serial.print("  ");
  }
  Serial.println();
}

// ---=== вывод строки с установленной температурой ===---
void PrintTempOpenCloseWinLine(byte OpenClose)
{
byte i;
//--------------------------
  if (OpenClose) Serial.print(F("B "));
  else           Serial.print(F("C "));
  Serial.print(BoldStr);
  if (OpenClose) Serial.print(F("Open   "));
  else           Serial.print(F("Close  "));
  Serial.print(NormalStr);
  for (i = 0; i < 8; i++)
  {
    PrintTemperWin(i, OpenClose); Serial.print("   ");
  }
  Serial.println();
}

// ---=== вывод строки с температурой датчиков ===---
void PrintSensWinTemperLine(void)
{
byte i;
//--------------------------
  Serial.print(BoldStr);
  Serial.print(F("  Temp  "));
  Serial.print(NormalStr);
  for (i = 0; i < 8; i++)
  {
    PrintSensWinTemper(i); Serial.print("  ");
  }
  Serial.println();
}

// ---=== вывод строки с состоянием форточек ===---
void PrintStateWinLine(void)
{
byte i;
//--------------------------
  Serial.print(BoldStr);
  Serial.print(F("  State "));
  Serial.print(NormalStr);
  for (i = 0; i < 8; i++)
  {
    PrintStateWin(i); Serial.print("  ");
  }
  Serial.println();
}

void PrintStateIOWinLine(void)
{
byte i;
//--------------------------
  Serial.print(F("     "));
  for (i = 0; i < 8; i++)
    Serial.print(F("   Op  Cl"));
  Serial.println();
}

// ---=== вывод строки с состоянием выходов ===---
void PrintOutPortPinWinLine(void)
{
byte i;
//--------------------------
  Serial.print(F("D E "));
  Serial.print(BoldStr);
  Serial.print(F("Out "));
  Serial.print(NormalStr);
  for (i = 0; i < 8; i++)
  {
    PrintOutPortPinWin(i); Serial.print("  ");
  }
  Serial.println();
}

// ---=== вывод строки с состоянием входов ===---
void PrintInPortPinWinLine(void)
{
byte i;
//--------------------------
  Serial.print(F("F G "));
  Serial.print(BoldStr);
  Serial.print(F("In  "));
  Serial.print(NormalStr);
  for (i = 0; i < 8; i++)
  {
    PrintInPortPinWin(i); Serial.print("  ");
  }
  Serial.println();
}

void PrintOutLine(void)
{
byte i;
//--------------------------
  Serial.print(BlackStr);
  Serial.print(WhiteFill);
  for (i = 0; i < ExRegsOutCnt; i++)
  {
    Serial.print(F("  Out"));
    Serial.print(i);    
    Serial.print("   ");    
  }
  Serial.println();
  Serial.print(NormalStr);
  for (i = 0; i < ExRegsOutCnt; i++)
  {
    Serial.print(F("76543210 "));
  }
  Serial.println();
}

void PrintOutPinsLine(void)
{
byte i, j;
//--------------------------
  for (i = 0; i < ExRegsOutCnt; i++)
  {
    for (j = 0; j < 8; j++)
    {
      if (ExRegsOutChange[i] & (1 << (7-j)))
        Serial.print(BoldStr);
      if (ExRegsOut[i] & (1 << (7-j)))
      {
        Serial.print(RedStr);
        Serial.print("1");
      }
      else
      {
        Serial.print(BlueStr);
        Serial.print("0");
      }
      Serial.print(NormalStr);
    }
    Serial.print(" ");
    ExRegsOutChange[i] = 0;
  }  
  Serial.println();
}

void PrintInLine(void)
{
byte i;
//--------------------------
  Serial.print(BlackStr);
  Serial.print(WhiteFill);
  for (i = 0; i < ExRegsInCnt; i++)
  {
    Serial.print(F("  In"));
    Serial.print(i);    
    Serial.print("    ");    
  }
  Serial.println();
  Serial.print(NormalStr);
  for (i = 0; i < ExRegsInCnt; i++)
  {
    Serial.print(F("76543210 "));
  }
  Serial.println();
}

void PrintInPinsLine(void)
{
byte i, j;
//--------------------------
  for (i = 0; i < ExRegsInCnt; i++)
  {
    for (j = 0; j < 8; j++)
    {
      if (ExRegsInChange[i] & (1 << (7-j)))
        Serial.print(BoldStr);
      if (ExRegsIn[i] & (1 << (7-j)))
      {
        Serial.print(RedStr);
        Serial.print("1");
      }
      else
      {
        Serial.print(BlueStr);
        Serial.print("0");
      }
      Serial.print(NormalStr);
    }
    Serial.print(" ");
    ExRegsInChange[i] = 0;
  }  
  Serial.println();
}

void PrintDateTimeSDC(void)
{
  char buffer[20];
  dt = clock.getDateTime();
  Serial.print(F("Time: "));
  Serial.print(clock.dateFormat(buffer, "d-m-Y H:i:s", dt));
  Serial.print(F("  SDC: "));
  if (SDCEnable == 0)
    Serial.print(F("None"));
  Serial.println();
}

// ---=== вывод экрана ===---
void out_scr(void)
{
byte i, j;
//--------------------------
  if (!OutScrEnbl) return;
  OutScrEnbl = 0;
  Serial.println();
  Serial.print(NewScr);
  PrintDateTimeSDC();
  PrintSensLine();
  PrintTempLine(0);
  PrintHumLine();
  PrintTempLine(1);
  PrintPressLine();
  for (i = 2; i < 8; i++)
  {
    PrintTempLine(i);
    PrintIDLine(i);
  }
  Serial.println();
  PrintWinLine();
  PrintSensNumWinLine();
  PrintTempOpenCloseWinLine(1);
  PrintTempOpenCloseWinLine(0);
  PrintSensWinTemperLine();
  PrintStateIOWinLine();
  PrintOutPortPinWinLine();
  PrintInPortPinWinLine();
  PrintStateWinLine();
  Serial.println();
  PrintOutLine();
  PrintOutPinsLine();
  Serial.println();
  PrintInLine();
  PrintInPinsLine();
  Serial.println(F("Warning! ExOutReg & ExInReg in inversion (1 - active Low)"));
  Serial.println();
  Serial.println(F("wX=sddd, where w[1..8], X[A..G], s[-], ddd[0..999 or [0..7]0..7 or [0..5]0..7]"));
  Serial.println();
/*
  Serial.print("{\"Time\":\"");Serial.print(GetDateTime());Serial.print("\", \"Temp\":[");
  for (i = 0; i < 64; i++)
  {
      Serial.print(GetSensTemper(i));
      if (i < 63) Serial.print(", ");
  }
  Serial.print("], \"Hum\":[");
  for (i = 0; i < 8; i++)
  {
      Serial.print(GetSensHum(i));
      if (i < 7) Serial.print(", ");
  }
  Serial.print("], \"Press\":[");
  for (i = 0; i < 8; i++)
  {
      Serial.print(GetSensPress(i));
      if (i < 7) Serial.print(", ");
  }
  Serial.println("]}");
*/  
  Serial.print("#> ");
  Serial.write(CmndLine, CmndCnt);
  
}

// ---=== преобразуем строку в число ===---
bool my_strtoint(char* buf, int* result)
{
int res = 0;
byte minus = 0;
  if (*buf == '-') {minus = 1; buf++;}
  while (*buf != 0)
    if ((*buf >= '0') && (*buf <= '9'))
      res = (res * 10) + (*buf++ & 0x0F);
    else
     return false;
  if (minus) res = -res;
  *result = res;
  return true;
}

char UpCase(char C)
{
  if ((C >= 'a') && (C <= 'z')) C = C - 0x20;
  return C;
}
  
void GetChar(void)
{
char C;
int result;
int Addr;
byte Winx;
//--------------------------
  // проверка на расширенный код
  C = Serial.peek();
  if (C == 0x1B)  // ESC
  {
    if (Serial.available() < 3) return;
    while (Serial.available() > 0) C = Serial.read();
    return;
  }
  OutScrEnbl = 1;
  C = UpCase(Serial.read());
  if ( (C == 0x7F) && (CmndCnt != 0) ) // DEL
    CmndCnt--;
  if ( (C == '-') || (C == '.') || (C == '=') || ( (C >= '0') && (C <= '9') ) || ( (C >= 'A') && (C <= 'Z') ) ) // '-', '.', '=', '0'..'9', 'A'..'Z'
    CmndLine[CmndCnt++] = C;
  if ( (C == 0x0A) || (C == 0x0D) )
  {
    CmndLine[CmndCnt] = 0;
    CmndCnt = 0;
    // парсим командную строку
    if ( (CmndLine[2] == '=') )
    {
      switch(CmndLine[0]) // номер форточки
      {
        case '1': Winx = 0; break;
        case '2': Winx = 1; break;
        case '3': Winx = 2; break;
        case '4': Winx = 3; break;
        case '5': Winx = 4; break;
        case '6': Winx = 5; break;
        case '7': Winx = 6; break;
        case '8': Winx = 7; break;
        default: return; break;
      }
      Addr = sizeof(Win) * Winx;
      switch(CmndLine[1]) // команда
      {
        case 'A': // Номер датчика
          if (my_strtoint(&CmndLine[3], &result))
          {
            eeprom_write_byte((uint8_t*)(Addr+0), (result / 10 * 8 + result % 10));
          }
          break;

        case 'B': // Температура открытия для форточки
          if (my_strtoint(&CmndLine[3], &result))
          {
            eeprom_write_word((uint16_t*)(Addr+1), result);
          }
          break;

        case 'C': // Температура закрытия для форточки
          if (my_strtoint(&CmndLine[3], &result))
          {
            eeprom_write_word((uint16_t*)(Addr+3), result);
          }
          break;

        case 'D': // Номер выходного регистра и Выход регистра открытия для форточки
          if (my_strtoint(&CmndLine[3], &result))
          {
            eeprom_write_byte((uint8_t*)(Addr+5), (result / 10));
            eeprom_write_byte((uint8_t*)(Addr+6), (result % 10));
          }
          break;

        case 'E': // Номер выходного регистра и Выход регистра закрытия для форточки
          if (my_strtoint(&CmndLine[3], &result))
          {
            eeprom_write_byte((uint8_t*)(Addr+7), (result / 10));
            eeprom_write_byte((uint8_t*)(Addr+8), (result % 10));
          }
          break;

        case 'F': // Номер входного регистра и Вход регистра открытия для форточки
          if (my_strtoint(&CmndLine[3], &result))
          {
            eeprom_write_byte((uint8_t*)(Addr+9), (result / 10));
            eeprom_write_byte((uint8_t*)(Addr+10), (result % 10));
          }
          break;

        case 'G': // Номер входного регистра и Вход регистра закрытия для форточки
          if (my_strtoint(&CmndLine[3], &result))
          {
            eeprom_write_byte((uint8_t*)(Addr+11), (result / 10));
            eeprom_write_byte((uint8_t*)(Addr+12), (result % 10));
          }
          break;

        default: return; break;
      }
      RewriteCRC(Winx);
    }
  }
/*
* ESC code:

  CmndLine[CmndCnt++] = HexToChar(C >> 4);
  CmndLine[CmndCnt++] = HexToChar(C);
  if (CmndCnt >= 64) CmndCnt = 0;
  
* Home   1B5B317E     = \x1b[1~     на начало
* Ins    1B5B327E     = \x1b[2~     вставка символа
* Del    1B5B337E     = \x1b[3~     удаление символа
* End    1B5B347E     = \x1b[4~     в конец
* PUp    1B5B357E     = \x1b[5~     страница вверх
* PDn    1B5B367E     = \x1b[5~     страница вниз
*  ↑     1B5B41       = \x1b[A      стрелка вверх
*  ↓     1B5B42       = \x1b[B      стрелка вниз
*  →     1B5B43       = \x1b[С      стрелка вправо
*  ←     1B5B44       = \x1b[D      стрелка влево
* Shift + ↑         1B5B313B3241 = \x1b[1;2A   Shift + стрелка вверх
* Shift + ↓         1B5B313B3242 = \x1b[1;2B   Shift + стрелка вниз
* Shift + →         1B5B313B3243 = \x1b[1;2C   Shift + стрелка вправо
* Shift + ←         1B5B313B3244 = \x1b[1;2D   Shift + стрелка влево
* Alt + ←           1B5B313B3344 = \x1b[1;3D   Alt + стрелка влево
* Ctrl + ←          1B5B313B3544 = \x1b[1;5D   Ctrl + стрелка влево
* Ctrl + Shift + ←  1B5B313B3644 = \x1b[1;6D   Ctrl + Shift + стрелка влево
* 
* 
*/
}

// ---=== переписать CRC в EEPROM ===---
void RewriteCRC(byte Winx)
{
byte j;
byte crc;
//--------------------------
  crc = 0;
  for (j = 0; j < (sizeof(Win)-1); j++)
    crc = crc ^ eeprom_read_byte((byte*)(Winx * sizeof(Win)) + j);
  eeprom_write_byte((byte*)(Winx * sizeof(Win)) + j, crc);
}

// ---=== проверка EEPROM при старте ===---
void Test_EE(void)
{
byte i, j;
byte crc;
//--------------------------
  for (i = 0; i < 8; i++) // индекс окна  
  {
    crc = 0;
    for (j = 0; j < sizeof(Win); j++)
      crc = crc ^ eeprom_read_byte((byte*)(i * sizeof(Win)) + j);
    if ((crc != 0) || (eeprom_read_byte((byte*)(i * sizeof(Win))) == 0xFF))
    {
      crc = 0;
      for (j = 0; j < (sizeof(Win)-1); j++)
      {
        crc = crc ^ pgm_read_byte((byte*)(&Wins[i])+j);
        eeprom_write_byte((byte*)(i * sizeof(Win)) + j, pgm_read_byte((byte*)(&Wins[i])+j));
      }
      eeprom_write_byte((byte*)(i * sizeof(Win)) + j, crc);
    }
  }
}

// ---=== запись данных ===---
/*
void loger_poll(void)
{
  char buffer[20];
  String dataString = "";
  dataString += String(GetDateTime());
  Serial.println(dataString);
  if (!(SDCEnable) || !(isLoger)) return;
  File dataFile = SD.open("datalog.txt", FILE_WRITE);
  if (dataFile) 
  {
    dataFile.println(clock.dateFormat(buffer, "d-m-Y H:i:s", dt));
    dataFile.close();
  }
}
*/

// ---=== настройки при запуске ===---
void setup() 
{
  io_setup();       // настраиваем входы и выходы
  SPI.begin();      // запускаем SPI
  init_ExRegs();    // инициализируем ExRegs
  Serial.begin(57600);
  clock.begin();
  // Set sketch compiling time
  clock.setDateTime(__DATE__, __TIME__);

//  Wire.begin();     // запуск шины I2C
  Wire.setClock(32000); // устанавливаем скорость на шине
  ExAddr = 0;       // устанавливаем начальный адрес
  SetExAddr();      // устанавливаем внешний адрес
  TimeTask = millis();  // время таймаута для задач
  OutScrEnbl = 1;
  Test_EE();
  if (SD.begin(SDCSelect)) SDCEnable = 1;
}

// ---=== основной цикл ===---
void loop() 
{
  out_scr();            // вывод в терминал
  io_poll();            // обрабатываем входы и выходы
  Sens_poll();          // обрабатываем датчики на I2C/OW/ADC
  win_ctrl();           // контроль температуры и управление форточками
//loger_poll();         // запись данных
  if (Serial.available() > 0) 
  {
    GetChar();
  }
} 
