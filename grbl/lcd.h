#ifndef _LCD_H_
#define _LCD_H_


// commands
#define LCD_CLEARDISPLAY 0x01
#define LCD_RETURNHOME 0x02
#define LCD_ENTRYMODESET 0x04
#define LCD_DISPLAYCONTROL 0x08
#define LCD_CURSORSHIFT 0x10
#define LCD_FUNCTIONSET 0x20
#define LCD_SETCGRAMADDR 0x40
#define LCD_SETDDRAMADDR 0x80

// flags for display entry mode
#define LCD_ENTRYRIGHT 0x00
#define LCD_ENTRYLEFT 0x02
#define LCD_ENTRYSHIFTINCREMENT 0x01
#define LCD_ENTRYSHIFTDECREMENT 0x00

// flags for display on/off control
#define LCD_DISPLAYON 0x04
#define LCD_DISPLAYOFF 0x00
#define LCD_CURSORON 0x02
#define LCD_CURSOROFF 0x00
#define LCD_BLINKON 0x01
#define LCD_BLINKOFF 0x00

// flags for display/cursor shift
#define LCD_DISPLAYMOVE 0x08
#define LCD_CURSORMOVE 0x00
#define LCD_MOVERIGHT 0x04
#define LCD_MOVELEFT 0x00

// flags for function set
#define LCD_8BITMODE 0x10
#define LCD_4BITMODE 0x00
#define LCD_2LINE 0x08
#define LCD_1LINE 0x00
#define LCD_5x10DOTS 0x04
#define LCD_5x8DOTS 0x00
#include <string.h>

#define LCD_COLS 20
#define LCD_ROWS 4

#define LCD_DDR DDRL
#define LCD_PORT PORTL
#define LCD_PIN_RS 5
#define LCD_PIN_EN 4
#define LCD_PIN_D4 0
#define LCD_PIN_D5 1
#define LCD_PIN_D6 2
#define LCD_PIN_D7 3

void init_lcd();
void lcd_clear();
void lcd_reflesh();
int lcd_print(int row, const char *text);
void lcd_run();
int lcd_printf(int row, const char *format, ...);

#endif
