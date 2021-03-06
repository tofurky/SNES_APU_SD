#include "APU.h"

#include <inttypes.h>
#include "Arduino.h"

APU::APU()
{
  init(APU_TYPE_XMEM);
}

APU::APU(uint8_t type)
{
  init(type);
}

void APU::init(uint8_t type)
{
#if not (defined(__AVR_ATmega2560__) || defined(__AVR_ATmega1280__))
  PORTB = 0x0F;
  DDRB = 0x0F;
  DDRC &= 0xC0;
  PORTC &= 0xC0;
   
#else
  mega_type = type;
  if (mega_type)
    init_mega_xmem();
  else
    init_mega();
#endif
}

#if (defined(__AVR_ATmega2560__) || defined(__AVR_ATmega1280__))
void APU::init_mega()
{
  PORTH = 0x60;
  DDRH = 0x03;  //Digital pins 8-9. (A0,A1)
  PORTB = 0x30;
  DDRB |= 0x30;  //Digital pins 10-11 (/RD,/WR)
  DDRF = 0x00; 
  PORTF = 0x00; //Analog Pin 0 (/RESET)
  
}

int APU::init_mega_xmem()
{
  XMCRB = 0;
  XMCRA = (1<<SRE)  //Enable Xmem Interface
        | (1 << SRL2) | (0 << SRL1) | (0 << SRL0) //Set upper sector limit
        | (0 << SRW11) | (0 << SRW10) //Set upper sector wait-states
        | (0 << SRW01) | (0 << SRW00); //Set lower sector wait-states.
  //SRE - Enable XMem Interface
  //SRL2 = 1, SRL1 = 0, SRL0 = 0 - Lower sector = 0x2200-0x7FFF, Upper sector = 0x8000-0xFFFF
  //SRWx1 = 0, SRWx0 = 0 - No wait-states
  //SRWx1 = 0, SRWx0 = 1 - Wait one cycle during read/write strobe
  //SRWx1 = 1, SRWx0 = 0 - Wait two cycles during read/write strobe
  //SRWx1 = 1, SRWx0 = 1 -      And wait one cycle before driving out new address.

  DDRC = 0xFF;
  PORTC = 0x00;
  XMCRB = (0 << XMBK) // External Memory Bus-keeper enable
        | (0 << XMM2) | (0 << XMM1) | (0 << XMM0); //External Memory High Mask

  // XMM2 XMM1 XMM0 - Released IO pins (Arduino notation, AVR notation)
  // 0    0    0    - None
  // 0    0    1    - 30   , PC7
  // 0    1    0    - 30-31, PC6-PC7
  // 0    1    1    - 30-32, PC5-PC7
  // 1    0    0    - 30-33, PC4-PC7
  // 1    0    1    - 30-34, PC3-PC7
  // 1    1    0    - 30-35, PC2-PC7
  // 1    1    1    - 30-37, PC0-PC7

  
  PORTL |= 0x80; //additional address lines
  DDRL |= 0x80;
  DDRL &= 0xBF; //Reset Line
  PORTL &= 0xBF;
  
  DDRD |= 0x80; //Additional address lines
}



#endif

uint8_t APU::read(uint8_t address)
{
  #if not (defined(__AVR_ATmega2560__) || defined(__AVR_ATmega1280__))
    return read_arduino(address);
  #else
    if(!mega_type)
      return read_arduino_mega(address);
    else
      return read_arduino_mega_xmem(address);
  #endif
}

void APU::write(uint8_t address, uint8_t data)
{
  #if not (defined(__AVR_ATmega2560__) || defined(__AVR_ATmega1280__))
    write_arduino(address, data);
  #else
    if(!mega_type)
      write_arduino_mega(address, data);
    else
      write_arduino_mega_xmem(address, data);
  #endif
}


void APU::reset()
{
  #if not (defined(__AVR_ATmega2560__) || defined(__AVR_ATmega1280__))
    reset_arduino();
  #else
    if(!mega_type)
      reset_arduino_mega();
    else
      reset_arduino_mega_xmem();
  #endif
}

#if not (defined(__AVR_ATmega2560__) || defined(__AVR_ATmega1280__))
uint8_t APU::read_arduino(uint8_t address)
{
  unsigned char data;

  DDRB = 0x0F;
  DDRD &= ~0xFC;
  PORTB &= ~0x03;
  PORTB |= (address & 0x03);

  PORTB &= ~0x04;
  __asm__ __volatile__ ("nop");
  __asm__ __volatile__ ("nop");
  data=((PINB&0x30)>>4) | (PIND&0xFC);
  PORTB |= 0x04;
  return data;
}



void APU::write_arduino(uint8_t address, uint8_t data)
{
  DDRB = 0x3F;
  DDRD |= 0xFC;

  PORTD &= ~0xFC;
  PORTD |= (data & 0xFC);

  PORTB &= ~0x33;
  PORTB |= (address & 0x03);
  PORTB |= ((data & 0x03)<<4);
  PORTB &= ~0x08;
  __asm__ __volatile__ ("nop");
  __asm__ __volatile__ ("nop");
  PORTB |= 0x08;
}

void APU::reset_arduino()
{
  pinMode(RESET_PIN,OUTPUT);
  pinMode(RESET_PIN,INPUT);
}
#else
uint8_t APU::read_arduino_mega(uint8_t address)
{
  unsigned char data = 0;
  DDRB |= 0x30;    //APU RD/WR
  DDRH |= 0x60;    //APU A0-A1
  DDRB &= ~0xC0;  //APU D0-D1
  DDRE &= ~0x38;  //APU D2-D3, D5
  DDRG &= ~0x20;  //APU D4
  DDRH &= ~0x18;  //APU D6-D7
  PORTH &= ~0x60;
  PORTH |= ((address & 0x03)<<5);

  PORTB &= ~0x10;
  __asm__ __volatile__ ("nop");
  __asm__ __volatile__ ("nop");
  data=((PINB&0xC0)>>6) | ((PINE&0x30)>>2) | ((PING&0x20)>>1) | ((PINE&0x08)<<2) | ((PINH&0x18)<<3);
  PORTB |= 0x10;
  return data;
}

void APU::write_arduino_mega(uint8_t address, uint8_t data)
{
  DDRB |= 0xF0;   //APU RD/WR,  D0-D1
  DDRE |= 0x38;   //APU D2-D3, D5
  DDRG |= 0x20;   //APU D4
  DDRH |= 0x18;   //APU D6-D7

  PORTB &= ~0xC0;
  PORTB |= ((data & 0x03)<<6);
  
  PORTE &= ~0x38;
  PORTE |= ((data & 0x0C)<<2) | ((data & 0x20)>>2);
  
  PORTG &= ~0x20;
  PORTG |= ((data&0x10)<<1);
  
  PORTH &= ~0x78;
  PORTH |= ((address & 0x03)<<5) | ((data&0xC0)>>3);
  
  PORTB &= ~0x20;
  __asm__ __volatile__ ("nop");
  __asm__ __volatile__ ("nop");
  PORTB |= 0x20;
}

void APU::reset_arduino_mega()
{
  pinMode(RESET_PIN_0,OUTPUT);
  pinMode(RESET_PIN_0,INPUT);
}

uint8_t APU::read_arduino_mega_xmem(uint8_t address)
{
  return _SFR_MEM8(0x2200 + ((address & 3) << 10));
}

void APU::write_arduino_mega_xmem(uint8_t address, uint8_t data)
{
  _SFR_MEM8(0x2200 + ((address & 3) << 10)) = data;
}

void APU::reset_arduino_mega_xmem()
{
  pinMode(RESET_PIN_1,OUTPUT);
  pinMode(RESET_PIN_1,INPUT);
}
#endif

