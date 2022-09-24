#ifndef _PIN_CONFIGURATOR_H_
#define _PIN_CONFIGURATOR_H_
#include <cstdint>
#include <Arduino.h>
#include <Wire.h>
#include <SAMDTimerInterrupt.hpp>
#include <SAMD_ISR_Timer.hpp>
#include <FlashAsEEPROM.h>
#include <FlashStorage.h>

#define PIN_PIR_SENSOR 5

//GPIO related costants
#define DIGITAL_INPUT 1
#define DIGITAL_OUTPUT 2
#define ANALOG_INPUT 3
#define ANALOG_OUTPUT 4
#define COUNTER_INPUT 5
#define PROXIMITY_OUTPUT 6
#define RESET_DIGITAL 7
#define RESET_ANALOG 8

//UART related costants
#define RESET_UART 0
#define UART_1200 1
#define UART_2400 2
#define UART_4800 3
#define UART_9600 4
#define UART_19200 5
#define UART_38400 6
#define UART_57600 7
#define UART_115200 8

//I2C related costants
#define RESET_TWI 0

#define DEBOUNCE_MS 300.0 //300ms debounce constant for digital switches

//Wrapper used in order to correctly R/W data from and to the Flash memory of the uC
typedef struct
{
  uint8_t container[255];
} uintArrayWrapper;

extern unsigned long PROXIMITY_MS;

extern uint8_t count;
extern unsigned long debounce_timer;
extern unsigned long proximity_timer;
extern uint8_t pin_prox_output;
extern SAMDTimer timer1;

void ISRIncrementCount();
void ISRProximityDetect();
bool isDigitalInput(uint8_t);
bool isDigitalOutput(uint8_t);
bool isAnalogInput(uint8_t);
bool isAnalogOutput(uint8_t);
bool isDigitalCounter(uint8_t);
bool isUartActive();
bool isProximityOutput();
bool isTwiActive();
void setAsDigitalInput(uint8_t);
void setAsDigitalOutput(uint8_t);
void setAsAnalogInput(uint8_t);
void setAsAnalogOutput(uint8_t);
void setAsDigitalCounter(uint8_t);
void setAsProximityOutput(uint8_t, uint8_t);
void setUartActive(uint8_t);
void setTwiActive();
void resetUart();
void resetTwi();
void resetDigital(uint8_t);
void resetAnalog(uint8_t);
uint8_t getDigitalCounter();
uint32_t getUartSpeed();
void initFromFlash();
void saveInFlash();
void initPeripherals();


#endif
