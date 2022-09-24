#include "pinConfigurator.h"

//Dynamic look up tables that store the configuration status of each function/pin
uint8_t LUT_PIN_STATUS[255] PROGMEM;
uint32_t LUT_UART_SPEED PROGMEM;
uint8_t LUT_TWI PROGMEM;

//Static look up tables used to make persistent the status of the volatile look up tables
FlashStorage(LUT_PIN_STATUS_FLASH, uintArrayWrapper);
FlashStorage(LUT_UART_SPEED_FLASH, uint32_t);
FlashStorage(LUT_TWI_FLASH, uint8_t);
FlashStorage(PROXIMITY_MS_FLASH, unsigned long);
FlashStorage(COUNT_FLASH, uint8_t);
FlashStorage(PROXIMITY_TIMER_FLASH, unsigned long);
FlashStorage(PIN_PROX_OUTPUT_FLASH, uint8_t);

//Hardware timer calling the ISRProximityDetect function
SAMDTimer timer1(TIMER_TC3);

unsigned long PROXIMITY_MS;
uint8_t count;
unsigned long debounce_timer;
unsigned long proximity_timer;
uint8_t pin_prox_output;

//ISR that increments the counter of the digital input in case configured as digital counter
void ISRIncrementCount()
{
  if(debounce_timer == 0 || (millis() > debounce_timer + DEBOUNCE_MS))
  {
    debounce_timer = millis();
    count = count + 1;
  }
}

//ISR called by the hardware interrupt tick; verifies and eventually sets the output in case configured as proximity output
void ISRProximityDetect()
{
  if(digitalRead(pin_prox_output) == HIGH && digitalRead(PIN_PIR_SENSOR) == HIGH)
  {
    proximity_timer = millis();
    digitalWrite(pin_prox_output, LOW);
  }

  else if(digitalRead(pin_prox_output) == LOW)
  {
    if(millis() > proximity_timer + PROXIMITY_MS)
      digitalWrite(pin_prox_output, HIGH);
  }
}

/* 0 - 255, order as follows: 
 *  1 d.i. (with pull-up), 
 *  2 d.o., 
 *  3 a.i., 
 *  4 a.o., 
 *  5 counter (interrupt-driven with pull-up), 
 *  6 proximity output (advanced feature)
 *  7 reset digital
 *  8 reset analog
 *  
 *  uart set with speed (1-8 : 1200, 2400, 4800, 9600, 19200, 38400, 57600 and 115200),
 *  0 uart reset
 *  
*/

bool isDigitalInput(uint8_t pin)
{
	return (LUT_PIN_STATUS[pin] == DIGITAL_INPUT ? true : false);
}

bool isDigitalOutput(uint8_t pin)
{
	return (LUT_PIN_STATUS[pin] == DIGITAL_OUTPUT ? true : false);
}

bool isAnalogInput(uint8_t pin)
{
	return (LUT_PIN_STATUS[pin] == ANALOG_INPUT ? true : false);
}

bool isAnalogOutput(uint8_t pin)
{
	return (LUT_PIN_STATUS[pin] == ANALOG_OUTPUT ? true : false);
}

bool isDigitalCounter(uint8_t pin)
{
	return (LUT_PIN_STATUS[pin] == COUNTER_INPUT ? true : false);
}

bool isUartActive()
{
  return (LUT_UART_SPEED != RESET_UART ? true : false);
}

bool isProximityOutput(uint8_t pin)
{
  return (LUT_PIN_STATUS[pin] == PROXIMITY_OUTPUT ? true : false);
}

bool isTwiActive()
{
  return (LUT_TWI != RESET_TWI ? true : false);
}

void setAsDigitalInput(uint8_t pin)
{
	LUT_PIN_STATUS[pin] = DIGITAL_INPUT;
	pinMode(pin, INPUT_PULLUP);
  saveInFlash();
}

void setAsDigitalOutput(uint8_t pin)
{
	LUT_PIN_STATUS[pin] = DIGITAL_OUTPUT;
	pinMode(pin, OUTPUT);
  digitalWrite(pin, HIGH);
  saveInFlash();
}

void setAsAnalogInput(uint8_t pin)
{
   LUT_PIN_STATUS[pin] = ANALOG_INPUT;
   saveInFlash();
}

void setAsAnalogOutput(uint8_t pin)
{
  LUT_PIN_STATUS[pin] = ANALOG_OUTPUT;
  saveInFlash();
}

void setAsDigitalCounter(uint8_t pin)
{
	LUT_PIN_STATUS[pin] = COUNTER_INPUT;
	pinMode(pin, INPUT_PULLUP);
	attachInterrupt(pin, ISRIncrementCount, FALLING); //Digital counter calls the ISR in case of falling edge dection on the selected pin
  saveInFlash();
}

void setAsProximityOutput(uint8_t pin, uint8_t amount)
{
  LUT_PIN_STATUS[pin] = PROXIMITY_OUTPUT;
  pinMode(pin, OUTPUT);
  digitalWrite(pin, HIGH);
  PROXIMITY_MS = 1000.0 * (unsigned long) amount; //Convert from seconds to milli-seconds
  pin_prox_output = pin;
  timer1.attachInterruptInterval(5000 * 1000, ISRProximityDetect); //Starts the timer1 hardware timer (comparation purposes)
  saveInFlash();
}

void setUartActive(uint8_t uart_speed)
{
  switch(uart_speed)
  {
    case UART_1200:
      LUT_UART_SPEED = 1200;
    break;
    case UART_2400:
      LUT_UART_SPEED = 2400;
    break;
    case UART_4800:
      LUT_UART_SPEED = 4800;
    break;
    case UART_9600:
      LUT_UART_SPEED = 9600;
    break;
    case UART_19200:
      LUT_UART_SPEED = 19200;
    break;
    case UART_38400:
      LUT_UART_SPEED = 38400;
    break;
    case UART_57600:
      LUT_UART_SPEED = 57600;
    break;
    case UART_115200:
      LUT_UART_SPEED = 115200;
    break;
    
    default:
      LUT_UART_SPEED = 115200;
    break;
  }

  Serial.begin(LUT_UART_SPEED);
  saveInFlash();
}

void setTwiActive()
{
  Wire.begin();
  LUT_TWI = 1;
  saveInFlash();
}

void resetUart()
{
  LUT_UART_SPEED = 0;
  Serial.end();
  saveInFlash();
}

void resetTwi()
{
  Wire.end();
  LUT_TWI = RESET_TWI;
  saveInFlash();
}

void resetDigital(uint8_t pin)
{
  if(LUT_PIN_STATUS[pin] == DIGITAL_OUTPUT)
    digitalWrite(pin, HIGH);
  else if(LUT_PIN_STATUS[pin] == COUNTER_INPUT)
  {
    detachInterrupt(pin);
    count = 0;
  }
  else if(LUT_PIN_STATUS[pin] == PROXIMITY_OUTPUT)
  {
    timer1.stopTimer();
    pin_prox_output = 0;
    digitalWrite(pin, HIGH);
  }
      
  LUT_PIN_STATUS[pin] = 0;
  saveInFlash();
}

void resetAnalog(uint8_t pin)
{
  if(LUT_PIN_STATUS[pin] == ANALOG_OUTPUT)
    analogWrite(pin, 0);
      
  LUT_PIN_STATUS[pin] = 0;
  saveInFlash();
}

uint8_t getDigitalCounter()
{
	return count;
}

uint32_t getUartSpeed()
{
  return LUT_UART_SPEED;
}

//Initializes the volatile look up tables with the value saved in Flash (if any)
void initFromFlash()
{
  uintArrayWrapper wrapper = LUT_PIN_STATUS_FLASH.read();
  for(int j = 0; j < sizeof(LUT_PIN_STATUS); j++)
    LUT_PIN_STATUS[j] = wrapper.container[j];
    
  LUT_UART_SPEED = LUT_UART_SPEED_FLASH.read();
  LUT_TWI = LUT_TWI_FLASH.read();

  PROXIMITY_MS = PROXIMITY_MS_FLASH.read();
  count = COUNT_FLASH.read();
  proximity_timer = PROXIMITY_TIMER_FLASH.read();
  pin_prox_output = PIN_PROX_OUTPUT_FLASH.read();
  
  initPeripherals();
}

//Saves the dynamic look up tables within the Flash (persistency)
void saveInFlash()
{
  uintArrayWrapper wrapper;
  for(int j = 0; j < sizeof(LUT_PIN_STATUS); j++)
    wrapper.container[j] = LUT_PIN_STATUS[j];
    
  LUT_PIN_STATUS_FLASH.write(wrapper);

  LUT_UART_SPEED_FLASH.write(LUT_UART_SPEED);
  LUT_TWI_FLASH.write(LUT_TWI);

  PROXIMITY_MS_FLASH.write(PROXIMITY_MS);
  COUNT_FLASH.write(count);
  PROXIMITY_TIMER_FLASH.write(proximity_timer);
  PIN_PROX_OUTPUT_FLASH.write(pin_prox_output);
}

//Initializes the peripherals at startup depending on the value in the dynamic look up tables
void initPeripherals()
{
  for(int j = 0; j < sizeof(LUT_PIN_STATUS); j++)
  {
    if(isDigitalInput(j))
      setAsDigitalInput(j);
    else if(isDigitalOutput(j))
      setAsDigitalOutput(j);
    else if(isDigitalCounter(j))
      setAsDigitalCounter(j);
    else if(isProximityOutput(j))
      setAsProximityOutput(j, (uint8_t) (PROXIMITY_MS / 1000.0));
  }

  if(isUartActive())
    setUartActive(getUartSpeed());
  if(isTwiActive())
    setTwiActive();
}
