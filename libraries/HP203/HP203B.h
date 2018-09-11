#ifndef HP203B_H
#define HP203B_H

#include <Arduino.h>

#define HP203B_address (0xEC >> 1) 

class HP203B
{
  private:
    int state;
  public:
	HP203B();
	float cTemp;
	float Pressure;
	int alive;
	int Read(void); 
};

#endif
