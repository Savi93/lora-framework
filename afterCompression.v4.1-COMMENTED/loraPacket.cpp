#include "loraPacket.h"
#include "pinConfigurator.h"

//Used to pack the internal data structure within an array before to be trasmitted
void serializePacket(loraPacket* packet, uint8_t* data, uint8_t* count)
{
  uint8_t body_index = 0;
  uint8_t index = 0;

  while (packet->body[body_index].dummy_body.opcode != 0)
  {
    if(!packet->body[body_index].dummy_body.isHomog)
    {
      if(body_index == 0)
        data[index++] = NON_HOMOG;
        
      data[index++] = packet->body[body_index].dummy_body.opcode;   
    }
    
    else if(packet->body[body_index].dummy_body.isHomog && body_index == 0)
      data[index++] = packet->body[body_index].dummy_body.opcode;

    switch (packet->body[body_index].dummy_body.opcode) //Switch depending on current opcode
    {
      case DIGITIN:
        data[index++] = packet->body[body_index].gpio_body.pin; //Pin
        data[index++] = packet->body[body_index].gpio_body.val; //Digital input value is transmitted as 8bit value
        break;
      case ANALIN:
      case DIGITCOUNT:
        data[index++] = packet->body[body_index].gpio_body.pin; //Pin
        data[index++] = (uint8_t)((packet->body[body_index].gpio_body.val & 0xFF00) >> 8); //8 MSB value
        data[index++] = (uint8_t)(packet->body[body_index].gpio_body.val & 0xFF); //8 LSB value
        break;
      case UARTTX:
        data[index++] = packet->body[body_index].uart_body.val; //8 bit ASCII value
        break;
      case TWITX:
        if((packet->body[body_index].dummy_body.isHomog && body_index == 0) || !packet->body[body_index].dummy_body.isHomog)
        {
          data[index++] = packet->body[body_index].twi_body.addr; //Device I2C address (7bit)
          data[index++] = (uint8_t)((packet->body[body_index].twi_body.reg & 0xFF00) >> 8); //8 MSB internal register address
          data[index++] = (uint8_t)(packet->body[body_index].twi_body.reg & 0xFF); //8 LSB internal register address
        }
        
        data[index++] = packet->body[body_index].twi_body.val; //I2C value that was read
        break;

        //TO BE DONE (OTHER CASE CONDITIONS RELATED TO THE IMPLEMENTED PROTOCOLS)
    }

    if (body_index == 0 && packet->body[body_index].dummy_body.isHomog)
      data[index++] = packet->body[body_index].dummy_body.cntNext; //Count next (number of packets following) in case of homogeneous packets

    body_index++;
  }

  *count = index;
}

//Used to unpack the incoming data from the Server (they are stored temporarily into an array) within the internal packet data structure
loraPacket deserializeStream(uint8_t* data, uint8_t* count)
{
  loraPacket packet;
  initPacket(&packet);

  uint8_t index = 0;
  uint8_t body_index = 0;
  bool is_homog = false;
  int16_t cnt_next = 0;

  if ((*count) > 0) //If data received is present
  {
    if(data[index] != NON_HOMOG)
      is_homog = true;
      
    do
    { 
      if (!is_homog)
      {
        if(body_index == 0)
          index++;
          
        packet.body[body_index].dummy_body.opcode = data[index++];
      }

      else
      {
        if(body_index == 0)
          packet.body[body_index].dummy_body.opcode = data[index++];

        else
          packet.body[body_index].dummy_body.opcode = packet.body[body_index - 1].dummy_body.opcode;
          
        packet.body[body_index].dummy_body.isHomog = 1;
      }

      switch (packet.body[body_index].dummy_body.opcode) //Switch depending on current opcode
      {
        case DIGITOUT:
        case CONFIG_DIGIT:
        {
          packet.body[body_index].gpio_body.pin = data[index];
          packet.body[body_index].gpio_body.val = data[++index];
          if(packet.body[body_index].gpio_body.val == PROXIMITY_OUTPUT)
            packet.body[body_index].gpio_body.val |= ((uint16_t)(data[++index]) << 8); //Needed for timing purposes (the value is indicating the seconds for the proximity output)
        }
        break;

        case ANALOUT:
        case CONFIG_ANAL:
          packet.body[body_index].gpio_body.pin = data[index]; //Pin
          packet.body[body_index].gpio_body.val = (((uint16_t)(data[++index]) << 8) | ((uint16_t)data[++index])); //Value (16bit)
        break;

        case UARTRX:
        case CONFIG_UART:
          packet.body[body_index].uart_body.val = data[index]; //ASCII value (8bit)
        break;

        case TWIRX:
        case CONFIG_TWI:
          if(packet.body[body_index].twi_body.opcode == CONFIG_TWI)
            packet.body[body_index].twi_body.addr = data[index]; //Address of the I2C device (7bit)
          else
          {
            if(!is_homog || (is_homog && body_index == 0))
            {
              packet.body[body_index].twi_body.addr = data[index]; //Address of the I2C device (7bit)
              packet.body[body_index].twi_body.rw = data[index] >> 7; //0 = Read, 1 = Write
              packet.body[body_index].twi_body.reg = (((uint16_t)(data[++index]) << 8) | ((uint16_t)data[++index])); //Internal register address (16bit)
            }

            else //Homog. data following the first packet are adopting values from the previous packets
            {
              packet.body[body_index].twi_body.addr = packet.body[body_index - 1].twi_body.addr;
              packet.body[body_index].twi_body.rw = packet.body[body_index - 1].twi_body.rw; //0 = Read, 1 = Write
              packet.body[body_index].twi_body.reg = packet.body[body_index - 1].twi_body.reg + 1; //Internal register address (16bit)
            }
           
            if(packet.body[body_index].twi_body.rw) //Only in case of writing data to device
              packet.body[body_index].twi_body.val = data[++index]; //Value to be written
          }
        break;

          //TO BE DONE (OTHER CASE CONDITIONS RELATED TO THE IMPLEMENTED PROTOCOLS)
      }

      //Only in case of the head of homog. packets, the countNext value is appended
      if (body_index == 0 && packet.body[body_index].dummy_body.isHomog)
      {
        packet.body[body_index].dummy_body.cntNext = data[++index];
        cnt_next = packet.body[body_index].dummy_body.cntNext;
      }

      if (is_homog && cnt_next == 0)
        is_homog = false;

      body_index++;
      index++;
      cnt_next--;
    } while(cnt_next >= 0 || index < *count);
  }

  return packet;
}

//Initialization of the packet data structures to a default value
void initPacket(loraPacket* packet)
{
  for (uint8_t j = 0; j < MAX_PACKET_SIZE; j++)
  {
    packet->body[j].dummy_body.opcode = 0; //Invalid opcode 0
    packet->body[j].dummy_body.isHomog = 0; //Data not homog.
    packet->body[j].dummy_body.cntNext = 0; //No next homog. packets
  }
}

//Returns the current size of the serial packets within the data structure
uint8_t packetLength(loraPacket* packet)
{
  uint8_t body_index = 0;

  while (packet->body[body_index].dummy_body.opcode != 0) //Loop until next empty element is found
    body_index++;

  return body_index;
}

//The current packet data structure does not contain homog. data and isHomog flags are set accordingly
void resetHomog(loraPacket* packet)
{
  uint8_t body_index = 0;
  
  while (packet->body[body_index].dummy_body.opcode != 0)
  {
    packet->body[body_index].dummy_body.isHomog = 0; 
    
    if(body_index == 0)
      packet->body[0].dummy_body.cntNext = 0;
      
    body_index++;
  }
}

//Returns true in case the current data structure contains homog. data
bool isDataHomog(loraPacket* packet)
{
  bool is_homog = true;
  uint8_t body_index = 1;

  while (packet->body[body_index].dummy_body.opcode != 0) //Loop until next empty element is found
  {
    if(packet->body[body_index].dummy_body.opcode != packet->body[body_index - 1].dummy_body.opcode)
    {
      is_homog = false;
      break;
    }
    
    body_index++;
  }
    
  return is_homog;
}

//Packs GPIO related (analog or digital) data within the data structure
void packGpioData(loraPacket* packet, uint8_t opcode, uint8_t pin, uint16_t val)
{
  uint8_t body_index = packetLength(packet);

  packet->body[body_index].gpio_body.opcode = opcode;
  packet->body[body_index].gpio_body.pin = pin;
  packet->body[body_index].gpio_body.val = val;

  //Only in case that all the packets within the data structure are homog. (of the same type)
  if (isDataHomog(packet))
  {
    packet->body[body_index].dummy_body.isHomog = 1;
    packet->body[0].dummy_body.isHomog |= 1;

    if(body_index >= 1)
      packet->body[0].dummy_body.cntNext++; //Increment counter of homog. elements following the head.
  }
  
  else
    resetHomog(packet);
}

//Packs UART related data within the data structure
void packUartData(loraPacket* packet, uint8_t val)
{
  uint8_t body_index = packetLength(packet);

  packet->body[body_index].uart_body.opcode = UARTTX;
  packet->body[body_index].uart_body.val = val;

  //Only in case that all the packets within the data structure are homog. (of the same type)
  if (isDataHomog(packet))
  {
    packet->body[body_index].dummy_body.isHomog = 1;
    packet->body[0].dummy_body.isHomog |= 1;

    if(body_index >= 1)
      packet->body[0].dummy_body.cntNext++; //Increment counter of homog. elements following the head.
  }
  
  else
    resetHomog(packet);
}

//Packs I2C related data within the data structure
void packTwiData(loraPacket* packet, uint8_t addr, uint16_t reg, uint8_t val)
{
  uint8_t body_index = packetLength(packet);

  packet->body[body_index].twi_body.opcode = TWITX;
  packet->body[body_index].twi_body.addr = addr;
  packet->body[body_index].twi_body.reg = reg;
  packet->body[body_index].twi_body.val = val;

  //Only in case that all the packets within the data structure are homog. (of the same type)
  if (isDataHomog(packet))
  {
    packet->body[body_index].dummy_body.isHomog = 1;
    packet->body[0].dummy_body.isHomog |= 1;
    
    if(body_index >= 1)
      packet->body[0].dummy_body.cntNext++; //Increment counter of homog. elements following the head.
  }

  else
    resetHomog(packet);
}
