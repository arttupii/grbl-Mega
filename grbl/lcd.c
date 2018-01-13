#include "grbl.h"

#include<stdio.h>

#define LCD_TRUE 1
#define LCD_FALSE 0
#define SEND_8_BIT 0
#define SEND_4_BIT 1

#define COMMAND_MODE 0
#define DATA_MODE 1

int initializeStateCounter;
int refleshNeeded;


char text_buffer[(LCD_COLS+1)*LCD_ROWS+1];

int dataOutStateCounter;
unsigned char dataOut;
int dataOutBitMode;


void textStreamToLcdStateMachine();
void initializeStateMachine();
void sendDataStateMachine();


inline void send4Bit(int val);
inline void send(int val, int mode, int bit4Mode);
inline void set_enable_pin(int val);
inline void set_rs_pin(int val);
inline void setDataPort(int val);

inline int dataOutBusy();
inline unsigned long tick();

inline void setPin(int pin, int val) {
  if(val) {
    LCD_PORT|=1<<pin;
  } else {
    LCD_PORT&=~(1<<pin);
  }
}

void init_lcd() {
  initializeStateCounter = 1;

  LCD_DDR |= 1<<LCD_PIN_RS;
  LCD_DDR |= 1<<LCD_PIN_EN;
  LCD_DDR |= 1<<LCD_PIN_D4;
  LCD_DDR |= 1<<LCD_PIN_D5;
  LCD_DDR |= 1<<LCD_PIN_D6;
  LCD_DDR |= 1<<LCD_PIN_D7;

  // Configure Timer 3: Sleep Counter Overflow Interrupt
  // NOTE: By using an overflow interrupt, the timer is automatically reloaded upon overflow.
  TCCR5B = 0; // Normal operation. Overflow.
  TCCR5A = 0;
  TCCR5B = (1<<CS51); //1/8 prescaler tick=0,0625us
  TCNT5 = 0;  // Reset timer3 counter register
  TIMSK5 |= (1<<TOIE5); // Enable timer3 overflow interrupt
  memset(text_buffer, ' ', sizeof(text_buffer)-1);
  memset(text_buffer, ' ', sizeof(text_buffer)-1);
  sei();
  lcd_print(0, "HELLO");
}

unsigned long tick_overflow=0;

ISR(TIMER5_OVF_vect) { tick_overflow++; }

void lcd_clear() {
  memset(text_buffer, ' ', sizeof(text_buffer));
  lcd_reflesh();
}

void lcd_reflesh() {
  refleshNeeded = LCD_TRUE;
}

int lcd_print(int row, const char *text) {
  int len = strlen(text);
  if(len>LCD_COLS) {
     len=LCD_COLS;
  }
  if(row>=LCD_ROWS) {
    return -1;
  }
  memset(&text_buffer[row*(LCD_COLS)], ' ', LCD_COLS);
  memcpy(&text_buffer[row*(LCD_COLS)], text, len);
  lcd_reflesh();
  return len;
}

int lcd_printf(int row, const char *format, ...)
{
   static char buf[30];
   va_list arg;
   int done;

   va_start (arg, format);
   done = vsprintf (buf, format, arg);
   text_buffer[done]=' ';
   va_end (arg);

   return lcd_print(row, buf);
}

void screenInfoUpdater() {
  static unsigned long t = 0;
  static unsigned int i=0;
  if(tick()-t>1000000) {
    t=tick();
    i++;
    lcd_printf(0,"HELLO %d",i);
  }
}

void lcd_run() {
  initializeStateMachine();
  textStreamToLcdStateMachine();
  sendDataStateMachine();
  screenInfoUpdater();
}

void initializeStateMachine() {
  static unsigned long tick3;
  if(initializeStateCounter>0) {
    static int loop = 0;

    switch(initializeStateCounter) {
      case 0:
        tick3 = tick();
        initializeStateCounter++;
        loop = 3;
        break;
      case 1:
        //Send 0x3 3xtimes to lcd
        if(tick()-tick3>15000) { //Wait 15ms
          tick3 = tick();
          send(0x3, COMMAND_MODE, SEND_4_BIT); //4-bit mode
          if(loop--<-1) {
            initializeStateCounter++;
          }
        }
        break;
      case 2:
        if(tick()-tick3>15000 && !dataOutBusy()) { //Wait 15ms
          tick3 = tick();
          send(0x2, COMMAND_MODE, SEND_4_BIT); //4-bit mode
          initializeStateCounter++;
          loop=3;
        }
        break;
      case 3:
        if(tick()-tick3>4500 && !dataOutBusy()) {
          tick3 = tick();
          send(LCD_FUNCTIONSET | LCD_4BITMODE |LCD_2LINE|LCD_1LINE | LCD_5x8DOTS, COMMAND_MODE, SEND_8_BIT);
          if(loop--<-1) {
            initializeStateCounter++;
          }
        }
        break;
      case 4:
        if(tick()-tick3>4500 && !dataOutBusy()) {
          tick3 = tick();
          send(LCD_DISPLAYCONTROL | LCD_DISPLAYON | LCD_CURSOROFF | LCD_BLINKOFF, COMMAND_MODE,SEND_8_BIT);
          initializeStateCounter++;
        }
        break;
      case 5:
        if(tick()-tick3>4500 && !dataOutBusy()) {
          tick3 = tick();
          send(LCD_ENTRYLEFT | LCD_ENTRYSHIFTDECREMENT|LCD_ENTRYMODESET, COMMAND_MODE,SEND_8_BIT);
          initializeStateCounter++;
        }
        break;
      case 6:
        if(tick()-tick3>50 && !dataOutBusy()) {
          tick3 = tick();
          initializeStateCounter=-1;
        }
        break;

    }
  }
}

void sendDataStateMachine() {
  static unsigned long tick2;
  switch(dataOutStateCounter) {
    case 0:
      send4Bit((dataOut>>4)&0xf); //4-bit mode
      set_enable_pin(LCD_FALSE);
      dataOutStateCounter++;
      tick2 = tick();
      break;
    case 1:
      if(tick()-tick2>1) {
        tick2 = tick();
        dataOutStateCounter++;
        set_enable_pin(LCD_TRUE);
      }
      break;
    case 2:
      if(tick()-tick2>1) {
        tick2 = tick();
        dataOutStateCounter++; // enable pulse must be >450ns
        set_enable_pin(LCD_FALSE);
      }
      break;
    case 3:
      if(tick()-tick2>50/*100*/) { //commands need > 37 us to settle. We should use busy flag instead of this delay. TODO
        tick2 = tick();
        dataOutStateCounter=-1;
        if(dataOutBitMode==SEND_8_BIT) {
           dataOutBitMode = -1;
           dataOutStateCounter=0;
           dataOut = dataOut<<4;
        } else {
          dataOutStateCounter=-1;
        }
      }
      break;
    };
}

void textStreamToLcdStateMachine() {
  static int dataPtr = -1;
  static int setRow = LCD_TRUE;
  if(!dataOutBusy() && dataPtr!=-1 && initializeStateCounter==-1) 	{
    //Stream text to LCD
    if(setRow  && (dataPtr==0||(dataPtr%(LCD_COLS)==0))) {
      setRow = LCD_FALSE;
      int addr = 0;       //row 1
      if(dataPtr>=LCD_COLS*1) {
        addr=0x40; //row 2
      }
      if(dataPtr>=LCD_COLS*2) {
        addr=LCD_COLS;   //row 3
      }
      if(dataPtr>=LCD_COLS*3) {
        addr=0x40+LCD_COLS;  //row 4
      }
      send(LCD_SETDDRAMADDR | addr, COMMAND_MODE, SEND_8_BIT);
      setRow=LCD_FALSE;
      return;
    }
    else {
      send(text_buffer[dataPtr], DATA_MODE, SEND_8_BIT);
      dataPtr++;
      setRow = LCD_TRUE;
      if(dataPtr>=LCD_COLS*LCD_ROWS) {
        dataPtr = -1;
      }
    }
  }

  if(refleshNeeded && dataPtr<0) {
    refleshNeeded = LCD_FALSE;
    dataPtr = 0;
    setRow=LCD_TRUE;
  }
}



void send4Bit(int val) { //Blocking version of send function
  setDataPort(val);
}


void set_enable_pin(int val) {
  setPin(LCD_PIN_EN, val);
}

void set_rs_pin(int val) {
  setPin(LCD_PIN_RS, val);
}

void setDataPort(int val) {
  setPin(LCD_PIN_D4, (val&1));
  setPin(LCD_PIN_D5, (val&2));
  setPin(LCD_PIN_D6, (val&4));
  setPin(LCD_PIN_D7, (val&8));
}

void send(int val, int mode, int bit4Mode) {
  dataOutBitMode = bit4Mode; //Send only the first nibble
  dataOut = bit4Mode==SEND_4_BIT?val<<4:val;
  dataOutStateCounter = 0;
  if(mode==COMMAND_MODE) {
    set_rs_pin(0);
  } else {
    set_rs_pin(1);
  }
}

int dataOutBusy() {
  return dataOutStateCounter!=-1;
}
static unsigned long t = 0;

unsigned long tick() {
  unsigned long tmp = (((unsigned long)TCNT5H)<<8|TCNT5L)/2;
  return (tick_overflow/2)<<16|tmp;
}
