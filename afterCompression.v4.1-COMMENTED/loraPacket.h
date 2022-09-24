#ifndef _LORA_PACKET_H_
#define _LORA_PACKET_H_
#include <cstdint>

//Ascending order definition of function opcode
#define NON_HOMOG 1
#define DIGITOUT 2
#define DIGITIN 3
#define ANALOUT 4
#define ANALIN 5
#define UARTRX 6
#define UARTTX 7
#define TWIRX 8
#define TWITX 9
#define DIGITCOUNT 10

//Descending order definition of configuration opcode (NOTICE: 223 is the max value usable as fPort)
#define CONFIG_TWI 220
#define CONFIG_UART 221
#define CONFIG_ANAL 222
#define CONFIG_DIGIT 223

//Max allowed serial data packets (estimated empirically based on the loraWan limitations)
#define MAX_PACKET_SIZE 51

typedef struct
{
    uint8_t opcode;
    uint8_t isHomog : 1; //Value into the head of the list, used to inform whether the packet contains homogeneous data
    uint8_t cntNext; //Value which may or may not be present into the head of the list (depending on hasNext), shows the number of data following
} loraPacketDummyBody;

typedef struct
{
    uint8_t opcode;
    uint8_t isHomog : 1; //Value into the head of the list, used to inform whether the packet contains homogeneous data
    uint8_t cntNext; //Value which may or may not be present into the head of the list, indicates the number of homog. data following
    uint8_t pin; //Physical Arduino pin 
    uint16_t val; //Value which can be either 0-1 for digital GPIOs or with greater resolution for analog GPIOs
} loraPacketGPIOBody;

typedef struct
{
    uint8_t opcode;
    uint8_t isHomog : 1; //Value into the head of the list, used to inform whether the packet contains homogeneous data
    uint8_t cntNext; //Value which may or may not be present into the head of the list, indicates the number of homog. data following
    uint8_t val; //Character corresponding to the ASCII table value
} loraPacketUartBody;

typedef struct
{
  uint8_t opcode;
  uint8_t isHomog : 1; //Value into the head of the list, used to inform whether the packet contains homogeneous data
  uint8_t cntNext; //Value which may or may not be present into the head of the list, indicates the number of homog. data following
  uint8_t addr : 7; //Address of the I2C device
  uint8_t rw : 1; //R = 1, W = 0
  uint16_t reg; //Register where to R/W
  uint8_t val; //Value which may or may not be present depending on the r/w status (present only if writing)
} loraPacketTWIBody;

typedef union
{
    loraPacketDummyBody dummy_body;
    loraPacketGPIOBody gpio_body;
    loraPacketUartBody uart_body;
    loraPacketTWIBody twi_body;
    //TO BE EXTENDED WITH OTHER PROTOCOLS (MODBUS, DALI, ...)
} loraPacketBody;

typedef struct
{
  loraPacketBody body[MAX_PACKET_SIZE];
} loraPacket;

void serializePacket(loraPacket*, uint8_t*, uint8_t*);
loraPacket deserializeStream(uint8_t*, uint8_t*);
void initPacket(loraPacket*);
uint8_t packetLength(loraPacket*);
bool isDataHomog(loraPacket*);
void resetHomog(loraPacket*);
void packGpioData(loraPacket*, uint8_t, uint8_t, uint16_t);
void packUartData(loraPacket*, uint8_t);
void packTwiData(loraPacket*, uint8_t, uint16_t, uint8_t);

#endif
