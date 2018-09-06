# Greenhouse
This is an arduino project for controlling the temperature and humidity of air and ground and controlling the windows in the greenhouse. The measurement statistics and data about the operation of the system are stored on the SD memory card. The current state is output to the terminal via the UART.

## Сomposition of the system
As a base board, the Arduino board is the STM32F103C8.

![arduino_stm32](https://github.com/EngDial/Greenhouse/blob/master/arduino_stm32.jpg)

Power is provided from a 12-volt uninterruptible power supply via a DC/DC converter that outputs 5V/2A.

![arduino_stm32](https://github.com/EngDial/Greenhouse/blob/master/dc_dc.jpg)

The HP203B sensor module is used to measure air temperature and atmospheric pressure. It is connected to the I2C bus and is powered by 3.3 volts from the Arduino board.

![HP203B](https://github.com/EngDial/Greenhouse/blob/master/HP203B.jpg)

The AM2320 is used to measure temperature and relative humidity. It is connected to the I2C bus and is powered by 5 volts.

![AM2320](https://github.com/EngDial/Greenhouse/blob/master/AM2320.jpg)

