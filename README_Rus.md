# Теплица
Это проект Arduino для контроля температуры, влажности воздуха и земли, и управления форточками в теплице. Статистика измерений и данные о работе системы хранятся на карте памяти SD. Текущее состояние выводится на терминал через UART.
## Состав системы
### Плата Arduino
В качестве базовой платы выступает плата Arduino на STM32F103C8.

![arduino_stm32](https://github.com/EngDial/Greenhouse/blob/master/img/arduino_stm32_.jpg)
### Питание
Питание обеспечивается от 12-вольтного источника бесперебойного питания через DC/DC-преобразователь, который выдает 5V/2A.

![arduino_stm32](https://github.com/EngDial/Greenhouse/blob/master/img/dc_dc_.jpg)
### Часы реального времени
Часы реального времени на DS3231 используются для синхронизации процессов и сохранения времени статистики. EEPROM 24C32 на плате RTC необходима для сохранения настроек.

![DS3231](https://github.com/EngDial/Greenhouse/blob/master/img/DS3231_.jpg)
### SD карта
SD-карта необходима для записи файлов журнала.

![SDC](https://github.com/EngDial/Greenhouse/blob/master/img/SDC_.jpg)
### Датчики
Модуль датчика HP203B используется для измерения температуры воздуха и атмосферного давления. Он подключен к шине I2C и питается от 3,3 вольта от платы Arduino.

![HP203B](https://github.com/EngDial/Greenhouse/blob/master/img/HP203B_.jpg)

AM2320 используется для измерения температуры и относительной влажности воздуха. Он подключен к шине I2C и питается от 5 вольт.

![AM2320](https://github.com/EngDial/Greenhouse/blob/master/img/AM2320_.jpg)

Датчики DS18B20 в герметичном корпусе используются для контроля температуры воздуха и почвы. Он подключается к однопроводной шине и питается от 5 вольт.

![DS18B20_H](https://github.com/EngDial/Greenhouse/blob/master/img/DS18B20_H_.jpg)
### Силовые реле
Для управления внешней нагрузкой используются модули реле. Реле управляются через сдвиговые регистры 74НС595 подключенные к шине SPI.

![Relay8_12V](https://github.com/EngDial/Greenhouse/blob/master/img/Relay8_12V.jpg)
![Cjmcu-595](https://github.com/EngDial/Greenhouse/blob/master/img/Cjmcu-595.jpg)
![74hc595](https://github.com/EngDial/Greenhouse/blob/master/img/74hc595.jpg)
### Входные сигналы
Регистры сдвига 74HC165 с параллельным входом и последовательным выходом, подключенные к шине SPI, расширяют входные порты. Входы используют защиту на обратном диоде, который обеспечивает активный низкий уровень.

![Input](https://github.com/EngDial/Greenhouse/blob/master/img/Input.jpg)
![74HC165](https://github.com/EngDial/Greenhouse/blob/master/img/74hc165.jpg)
### Привод форточек
Для приводов используются редукторы с зубчатой рейкой.

![motor](https://github.com/EngDial/Greenhouse/blob/master/img/motor.jpg)
![drive_win](https://github.com/EngDial/Greenhouse/blob/master/img/drive_win1.jpg)
![drive](https://github.com/EngDial/Greenhouse/blob/master/img/drive.jpg)


