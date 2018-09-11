#include "HP203B.h"
#include <Wire.h>
// 
// HP203B Temperature & Pressure Sensor library for Arduino

/* Command set for HP203B sensor */
#define	SOFT_RST	0x06	        // Soft reset the device
#define	READ_PT		0x10	        // Read the temperature and pressure values
#define	READ_AT		0x11	        // Read the temperature and altitude values
#define	ANA_CAL		0x28	        // Re-calibrate the internal analog blocks
#define	READ_P		0x30	        // Read the pressure value only
#define	READ_A		0x31	        // Read the altitude value only
#define	READ_T		0x32	        // Read the temperature value only
#define	ADC_CVT		0x40	        // Perform ADC conversion
#define	READ_REG	0x80	        // Read out the control registers
#define	WRITE_REG	0xC0	        // Write in the control registers

/* The 3-bit OSR defines the decimation rate of the internal digital filter */
#define	OSR4096		0x00	        // 4096
#define	OSR2048		0x01	        // 2048
#define	OSR1024		0x02	        // 1024 ~20ms (delayconvert ~25ms)
#define	OSR512		0x03	        // 512
#define	OSR256		0x04	        // 256
#define	OSR128		0x05	        // 128

/* The 2-bit channel (CHNL) parameter tells the device the data from which channel(s) shall be converted by the internal ADC */
#define	PT_CHANNEL	0x00	        // Sensor pressure and temperature channel
#define	T_CHANNEL	0x02	        // Temperature channel

// Constructor
HP203B::HP203B()
{
  alive = 0;
}

int HP203B::Read()
{
unsigned int buf[6];
uint8_t i; 
//------------------------------
  alive = 0;
  // Start I2C transmission
  Wire.beginTransmission(HP203B_address);
  // Send OSR and channel setting command
  Wire.write(ADC_CVT | (OSR128<<2) | PT_CHANNEL);
  // Stop I2C transmission
  if ( (state = Wire.endTransmission()) != 0) return state;	// 0: передача успешна
  
  delay(5); // delay 5 ms

  // clr buf
  for (i = 0; i < 6; i++) 
    buf[i] = 0x00; 
  // Start I2C transmission
  Wire.beginTransmission(HP203B_address);
  // Select data register
  Wire.write(READ_PT);
  // Stop I2C transmission
  if ( (state = Wire.endTransmission()) != 0 ) return state;
  // Read data (6 byte = (3 Temp)+(3 Press))
  if ( Wire.requestFrom(HP203B_address, 6) != 6 ) return state = 1;
  for (i = 0; i < 6; i++)
    buf[i] = Wire.read();
  cTemp = ( ((buf[0] & 0x0F) * 65536) + (buf[1] * 256) + buf[2] ) / 100.00; // *C
  Pressure = ( ((buf[3] & 0x0F) * 65536) + (buf[4] * 256) + buf[5] ) * 0.00750063755419211; // mmHg
  alive = 1;
  return state;

}
