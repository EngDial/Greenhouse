# Greenhouse
This is an arduino project for controlling the temperature and humidity of air and ground and controlling the windows in the greenhouse. The measurement statistics and data about the operation of the system are stored on the SD memory card. The current state is output to the terminal via the UART.
## Сomposition of the system
### Arduino board
As a base board, the Arduino board is the STM32F103C8.

![arduino_stm32](https://github.com/EngDial/Greenhouse/blob/master/img/arduino_stm32.jpg)

### Power
Power is provided from a 12-volt uninterruptible power supply via a DC/DC converter that outputs 5V/2A.

![arduino_stm32](https://github.com/EngDial/Greenhouse/blob/master/img/dc_dc.jpg)

### RTC
![DS3231](https://github.com/EngDial/Greenhouse/blob/master/img/DS3231.jpg)

### SD card
![SDC](https://github.com/EngDial/Greenhouse/blob/master/img/SDC.jpg)

### Sensors
The HP203B sensor module is used to measure air temperature and atmospheric pressure. It is connected to the I2C bus and is powered by 3.3 volts from the Arduino board.

![HP203B](https://github.com/EngDial/Greenhouse/blob/master/img/HP203B.jpg)

The AM2320 is used to measure temperature and relative humidity. It is connected to the I2C bus and is powered by 5 volts.

![AM2320](https://github.com/EngDial/Greenhouse/blob/master/img/AM2320.jpg)

Sensors DS18B20 in hermetic design are used to control air and soil temperature. It is connected to a single-wire bus and is powered from 5 volts.

![DS18B20_H](https://github.com/EngDial/Greenhouse/blob/master/img/DS18B20_H.jpg)

### Out relays
To control the external load, relay modules are used. The relays are controlled via the shift registers 74НС595 connected cascaded and connected via the SPI bus.

![RELAY16_12V](https://github.com/EngDial/Greenhouse/blob/master/img/RELAY16_12V_.jpg)

![HC595](https://github.com/EngDial/Greenhouse/blob/master/img/74HC595_.jpg)

### Input siglas
To expand the input ports, the 74HC165 shift registers included cascaded with parallel loading and sequential unloading to the SPI bus. The inputs use protection from a reverse diode which provides an active low level.

