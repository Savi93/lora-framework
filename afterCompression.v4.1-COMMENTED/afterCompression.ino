#include "loraPacket.h"
#include "pinConfigurator.h"
#include <MKRWAN.h>
#include <Wire.h>
#include <SAMDTimerInterrupt.h>
#include <SAMDTimerInterrupt.hpp>
#include <SAMD_ISR_Timer.h>
#include <SAMD_ISR_Timer.hpp>

#define TOGGLE_LSB(val) (val ^ 0b1)

#define PIN_DIGITOUT1 0
#define PIN_DIGITOUT2 1
#define PIN_ANALOUT1 2
#define PIN_ANALOUT2 3
#define PIN_DIGITIN 4
#define PIN_ANALIN 15
#define MPU6050_TWI_ADDR 0x68
#define MPU6050_TEMP_ADDR_MSB 0x41
#define MPU6050_TEMP_ADDR_LSB 0x42

#define BUFF_SIZE 255
#define TIMER0_INTERVAL_MS 15000 //in ms (1500ms = 15s); this timer permits the automation of sending packets with a precise timing

//Put inside Flash memory instead of RAM
const char APPEUI[] PROGMEM = "1234567887654321";
const char APPKEY[] PROGMEM = "97B42333AF14CD245DCBFF1C2B839F41";
char welcome[] PROGMEM = "Welcome!";

LoRaModem modem;
SAMDTimer timer0(TIMER_TCC);

uint8_t rcvBuffer[BUFF_SIZE] = {0};
uint8_t rcvBufferCnt = 0;
uint8_t sndBuffer[BUFF_SIZE] = {0};
uint8_t sndBufferCnt = 0;
volatile uint8_t canSend = 0;

volatile uint16_t twiVal[255] = {0};

void ISRTimedSend()
{
  canSend = TOGGLE_LSB(canSend);
}

inline uint8_t sendMessage(char* message)
{
  loraPacket packet;
  initPacket(&packet);
  uint8_t len = 0;

  while (message[len] != '\0')
    len++;

  for (int j = 0; j < len; j++)
    packUartData(&packet, message[j]);

  serializePacket(&packet, sndBuffer, &sndBufferCnt);

  modem.dataRate(5); //SF7BW125kH
  uint8_t error;
  modem.setPort(sndBuffer[0]);
  modem.beginPacket();
  modem.write(&(sndBuffer[1]), sndBufferCnt - 1); //Skip first byte, sent through fPort
  error = modem.endPacket(false);
  freeBuffer(sndBuffer, &sndBufferCnt);

  return error;
}

inline uint8_t sendData()
{
  loraPacket updata;
  initPacket(&updata);

  if (isAnalogInput(PIN_ANALIN))
    packGpioData(&updata, ANALIN, PIN_ANALIN, analogRead(PIN_ANALIN));

  if (isUartActive())
    packUartData(&updata, 'C');
  if (isUartActive())
    packUartData(&updata, 'i');

  if (isDigitalInput(PIN_DIGITIN))
    packGpioData(&updata, DIGITIN, PIN_DIGITIN, digitalRead(PIN_DIGITIN));
  else if (isDigitalCounter(PIN_DIGITIN))
    packGpioData(&updata, DIGITCOUNT, PIN_DIGITIN, getDigitalCounter());

  if (isUartActive())
    packUartData(&updata, 'a');
  if (isUartActive())
    packUartData(&updata, 'o');

  if (isTwiActive())
  {
    for(uint8_t j = 0; j < 255; j++)
    {
      if(twiVal[j] != 0)
      { 
        packTwiData(&updata, MPU6050_TWI_ADDR, MPU6050_TEMP_ADDR_MSB, twiVal[j] & 0xFF);
        twiVal[j] = 0;
      }
      
      else
        break;
    }
  }

  serializePacket(&updata, sndBuffer, &sndBufferCnt);

  modem.dataRate(5); //SF7BW125kH
  uint8_t error;

  if (sndBufferCnt > 0)
  {
    modem.setPort(sndBuffer[0]);
    modem.beginPacket();
    modem.write(&(sndBuffer[1]), sndBufferCnt - 1); //Skip first byte, sent through fPort
    error = modem.endPacket(false);
    canSend = 0;

    freeBuffer(sndBuffer, &sndBufferCnt);
  }

  return error;
}

inline void receiveStream(uint8_t* rcvBuffer)
{
  rcvBuffer[rcvBufferCnt++] = modem.getDownlinkPort();

  while (modem.available())
    rcvBuffer[rcvBufferCnt++] = modem.read();
}

inline void freeBuffer(uint8_t* buff, uint8_t* count)
{
  for (uint8_t j = 0; j < *count; j++)
    buff[j] = 0;

  *count = 0;
}

void setup()
{
  Serial.begin(115200);
  while (!Serial);
  Serial.println(F("Welcome to MKR WAN 1300/1310!"));

  while (!modem.begin(EU868))
    Serial.println(F("Failed to start module"));

  //To get also DOWNLINK without UPLINK to the GW
  modem.configureClass(CLASS_C);

  Serial.println(F("Finally connected!"));
  Serial.print(F("Your device EUI is: "));
  Serial.println(modem.deviceEUI());

  uint8_t connected = modem.joinOTAA(APPEUI, APPKEY);

  if (!connected)
  {
    Serial.println(F("Something went wrong; are you indoor? Move near a window and retry..."));
    while (!modem.joinOTAA(APPEUI, APPKEY))
      delay(1000);
  }

  Serial.println(F("Finally joined in the LoraWan network!"));
  if (sendMessage(welcome) > 0)
    Serial.println(F("Message sent correctly!"));
  else
    Serial.println(F("Error sending message!"));

  Serial.end();

  delay(5000);

  timer0.attachInterruptInterval(TIMER0_INTERVAL_MS * 1000, ISRTimedSend);
  initFromFlash();
}

void loop()
{
  loraPacket downdata;
  initPacket(&downdata);

  if (modem.available())
  {
    receiveStream(rcvBuffer);
    downdata = deserializeStream(rcvBuffer, &rcvBufferCnt);
    freeBuffer(rcvBuffer, &rcvBufferCnt);

    uint8_t body_index = 0;
    uint8_t twi_index = 0;

    while (downdata.body[body_index].dummy_body.opcode != 0)
    {
      switch (downdata.body[body_index].dummy_body.opcode)
      {
        case DIGITOUT:
          if (isDigitalOutput(downdata.body[body_index].gpio_body.pin))
            digitalWrite(downdata.body[body_index].gpio_body.pin, TOGGLE_LSB(downdata.body[body_index].gpio_body.val)); //Sinking current from outside (+3.3V) - Inverse logic
          break;
        case CONFIG_DIGIT:
          if (downdata.body[body_index].gpio_body.val == DIGITAL_INPUT)
            setAsDigitalInput(downdata.body[body_index].gpio_body.pin);
          else if (downdata.body[body_index].gpio_body.val == DIGITAL_OUTPUT)
            setAsDigitalOutput(downdata.body[body_index].gpio_body.pin);
          else if (downdata.body[body_index].gpio_body.val == COUNTER_INPUT)
            setAsDigitalCounter(downdata.body[body_index].gpio_body.pin);
          else if ((downdata.body[body_index].gpio_body.val & 0x00FF) == PROXIMITY_OUTPUT)
            setAsProximityOutput(downdata.body[body_index].gpio_body.pin, (downdata.body[body_index].gpio_body.val & 0xFF00) >> 8);
          else if (downdata.body[body_index].gpio_body.val == RESET_DIGITAL)
            resetDigital(downdata.body[body_index].gpio_body.pin);
          break;
        case ANALOUT:
          if (isAnalogOutput(downdata.body[body_index].gpio_body.pin))
            analogWrite(downdata.body[body_index].gpio_body.pin, downdata.body[body_index].gpio_body.val);
          break;
        case CONFIG_ANAL:
          if (downdata.body[body_index].gpio_body.val == ANALOG_INPUT)
            setAsAnalogInput(downdata.body[body_index].gpio_body.pin);
          else if (downdata.body[body_index].gpio_body.val == ANALOG_OUTPUT)
            setAsAnalogOutput(downdata.body[body_index].gpio_body.pin);
          else if (downdata.body[body_index].gpio_body.val == RESET_ANALOG)
            resetAnalog(downdata.body[body_index].gpio_body.pin);
          break;
        case UARTRX:
          {
            char letter = (char)downdata.body[body_index].uart_body.val;
            if (isUartActive())
              Serial.print(letter);
          }
          break;
        case CONFIG_UART:
          if(downdata.body[body_index].uart_body.val == RESET_UART)
            resetUart();
          else
            setUartActive(downdata.body[body_index].uart_body.val);
          break;
        case TWIRX:
          if(isTwiActive())
          {
            Wire.beginTransmission(downdata.body[body_index].twi_body.addr);
            Wire.write(downdata.body[body_index].twi_body.reg);

            if (downdata.body[body_index].twi_body.rw) //Write operation
            {
              Wire.write(downdata.body[body_index].twi_body.val);
              Wire.endTransmission();
            }

            else //Read operation
            {
              Wire.endTransmission();
              Wire.requestFrom(downdata.body[body_index].twi_body.addr, 1);
              twiVal[twi_index++] = Wire.read();
            }
          }
        break;

        case CONFIG_TWI:
          if(downdata.body[body_index].twi_body.addr == RESET_TWI)
            resetTwi();
          else
            setTwiActive();
        break;
      }

      body_index++;
    }

    //Serial.println();
  }

  if (canSend)
    sendData();

  delay(3000);
}
