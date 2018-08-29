/*SlotMachine.ino

  Version:   1.0
  Date:      2018/07/01 - 2018/08/29
  Device:    ATMega328P-PU @ 16mHz
  Language:  C

  Purpose
  =======
  A slot machine for entertainment and educational purposes only, 
  with the following features:
  - AtMega328P microcontroller running at 16mHz
  - Custom I2C seven segment display for displaying credit balance,
    also built with an ATMega328P running at 16mHz.  That program is
    supplied in a seperate file.
  - Three 8x8 LED matricies for displaying symbols driven by MAX7219. 
  - I2C LCD display 20x4, to show menus
  - various buzzers, buttons and an RGB LED.
  - the ability to update various settings via the LCD menu to 
    influence the machine's behavior.
  - the ability to change the amount of the wager.
  
  Known Defects
  =============  
  - Sometimes one or two of the reels won't spin, not really a defect.
  - crash as soon as payed out exceeds 1,000,000.
  
  TODO
  ====
  - add brown out detection
  - add watch dog protection (wdt_enable(value), wdt_reset(), WDTO_1S, WDTO_250MS)

  Warnings
  ========
  - Beware of turning on too much debugging, it's easy to use all 
    of the data memory, and in general this makes the microcontroller
    unstable.
  - Gambling is a tax on people who are bad at math.  This is for
    entertainment only.  It was the intent of the author to program this game
    to return ~%hold of every wager to the house, similar to many slot machines.
  - Why not control the LED that displays the credits with the LedControl 
    library?  I tried that and couldn't get more than one LedControl object to
    work at a time.  So I had to create an I2C slave instead and use another
    AVR.  

  Suggestions
  ===========
  - Best viewed in an editor w/ 160 columns, most comments are at column 80
  - Please submit defects you find so I can improve the quality of the program
    and learn more about embedded programming.

  Author
  ======
  - Copyright ©2018, Daniel Murphy <dan-murphy@comcast.net>
  - Contributors: Source code has been pulled from all over the internet,
    it would be impossible for me to cite all contributors.
    Special thanks to Elliott Williams for his essential book
    "Make: AVR Programming", which is highly recommended. Thanks also
    to Cory Potter, who gave me the idea to do this.

  License
  =======
  Daniel J. Murphy hereby disclaims all copyright interest in this
  program written by Daniel J. Murphy.

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.

  Libraries 
  =========
  - https://github.com/wayoda/LedControl
  - https://bitbucket.org/teckel12/arduino-timer-free-tone/wiki/Home
  - https://github.com/fdebrabander/Arduino-LiquidCrystal-I2C-library
  - https://bitbucket.org/fmalpartida/new-liquidcrystal/wiki/Home

  The Program
  ===========
  - Includes                                                                    */
#include <avr/io.h>
#include <avr/eeprom.h>
#include <stdlib.h>                                                             // for the abs function
#include "LedControl.h"                                                         // https://github.com/wayoda/LedControl
#include "SlotMachine.h"
#include <TimerFreeTone.h>                                                      // https://bitbucket.org/teckel12/arduino-timer-free-tone/wiki/Home
#include <Wire.h>
#include <LCD.h>                                                                // https://bitbucket.org/fmalpartida/new-liquidcrystal/wiki/Home
#include <LiquidCrystal_I2C.h>                                                  // https://github.com/fdebrabander/Arduino-LiquidCrystal-I2C-library

//- Payout Table
/*  Probabilities based on a 1 credit wager
    Three spaceships:     1 / (25 * 25 * 25)    = 0.000064
    Any three symbols:            24 / 15625    = 0.001536
    Two spaceships:         (24 * 3) / 15625    = 0.004608
    One spaceship:      (24 * 24 * 3)/ 15625    = 0.110592
    Two symbols match: (23 * 3 * 24) / 15625    = 0.105984
    House win, 1 minus sum of all probabilities = 0.777216
    _
    Use the spreadsheet to work out the payout table remembering to keep the 
    volatility resonable i.e. < 20.
                                                   P   R   O   O   F
                                                   Actual    Actual    
        Winning Combination Payout   Probablility  Count     Probability
        =================== ======   ============  ========  ===========*/
#define THREE_SPACESHIP_PAYOUT 600 //    0.000064            0.00006860   see the excel spreadsheet  
#define THREE_SYMBOL_PAYOUT    122 //    0.001536            0.00151760   that accompanies this program.
#define TWO_SPACESHIP_PAYOUT    50 //    0.004608            0.00468740
#define ONE_SPACESHIP_PAYOUT     3 //    0.110592            0.11064389
#define TWO_SYMBOL_PAYOUT        2 //    0.105984            0.10575249
//
// With these payouts the Volatility Index is 16.43
//
//- Macros
#define ClearBit(x,y) x &= ~y
#define SetBit(x,y) x |= y
#define ClearBitNo(x,y) x &= ~_BV(y)                                            
#define SetState(x) SetBit(machineState, x)

//- Defines
#define DEBUG                   1                                               // turns on (1) and off (0) output from debug* functions
#define BAUD_RATE               38400                                           // Baud rate for the Serial monitor
                                
#define NUMFRAMES               25                                              // Number of symbols in each "reel" or "slot". e.g three reels: |7|7|7|
#define LINESPERFRAME           8                                               // each line corresponds to one row on the 8x8 dot matrix LED
#define FRAME_DELAY             100                                             // milliseconds, controls the speed of the spinning reels
#define NUMREELS                3                                               // the hardware (8x8 matricies) accomodates 4 reels, we're only using three now
                                
#define DEBOUNCE_TIME           1000                                            // microseconds (changed from 500 to 1000 to cut down on double press problem)
                                
#define BUTTON_DDR              DDRD                                            // this accomodates the button that starts the reels spinning
#define BUTTON_PORT             PORTD
#define BUTTON_PIN              PIND
#define PCMSK_BUTTON            PCMSK2
#define PCIE_BUTTON             PCIE2
                                
#define BUTTON_SPIN_PIN         DDD2                                            // the actual spin button
#define BUTTON_SPIN_INT         PCINT18
#define BUTTON_SPIN_PORT        PORTD2
                                
#define NAV_DDR                 DDRC                                            // this is for the buttons that control menu navigation on the 20x4 LCD
#define NAV_PORT                PORTC
#define NAV_PIN                 PINC
#define PCMSK_NAV               PCMSK1
#define PCIE_NAV                PCIE1
                                
#define NAV_UP_PIN              DDC1                                            // Navigate up button
#define NAV_UP_INT              PCINT9
#define NAV_UP_PORT             PORTC1
                                
#define NAV_DOWN_PIN            DDC2                                            // Navigate down button
#define NAV_DOWN_INT            PCINT10
#define NAV_DOWN_PORT           PORTC2
                                
#define SELECT_PIN              DDC3                                            // Select current menu item button
#define SELECT_INT              PCINT11
#define SELECT_PORT             PORTC3
                                
#define BUZZER_DDR              DDRB                                            // This is for the slot machines piezo buzzer
#define BUZZER_PORT             PORTB
#define BUZZER_PIN              DDB3
#define TONE_PIN                11                                              // Pin you have speaker/piezo connected to (TODO: be sure to include a 100ohm resistor).

#define EVENT_NONE              0                                               // These are all of the various events that can occur in the machine
#define EVENT_SPIN              1
#define EVENT_SHOW_MENU         2  
#define EVENT_SELECT            3
#define EVENT_NAV_UP            4
#define EVENT_NAV_DOWN          5
#define EVENT_BACK              6
#define EVENT_PLAY              10
#define EVENT_BET               11
#define EVENT_SETTINGS          12
#define EVENT_VIEW_METRICS      13
#define EVENT_RESET             14
#define EVENT_HOLD              15

#define STATE_IDLE              B00000001                                       // These are the various states the machine can be in, not all are
#define STATE_SPINNING          B00000010                                       // mutually exclusive.
#define STATE_AUTO              B00000100                                       // This state is for automatically running the program to gather metrics.
#define STATE_SHOW_MENU         B00001000                                       // State we're in when showing the menu.  Note you can spin and show menu 
                                                                                // concurrently.
#define MINIMUM_WAGER           5                                               // TODO: consider this something that can be changed via settings
#define WAGER_INCREMENT         5                                               // TODO: consider this something that can be changed via settings

#define ONE_SECOND              1000                                            // # milliseconds in one second. Used to control how long the siren sounds. 

#define SHIP_LOC                144                                             // Location of various symbols in the array of symbols maintained in SlotMachine.h
#define ALIEN_1_LOC             152                                             // needed for animation
#define ALIEN_2_LOC             160

#define EEPROM_FREQ             10000                                           // Write to EEPROM every Nth play
#define AUTO_MODE_MAX           1000000                                           // stop after this many plays in auto mode

#define RED                     1                                               // TODO: should we use an enum here?  Must be a better way...
#define GREEN                   2
#define BLUE                    3
#define PURPLE                  4
#define WHITE                   5
#define OFF                     6

#define MAX_NOTE                4978                                            // Maximum high tone in hertz. Used for siren.
#define MIN_NOTE                31                                              // Minimum low tone in hertz. Used for siren.

#define STARTING_CREDIT_BALANCE 500                                             // Number of credits you have at "factory reset".
#define DEFAULT_HOLD            0                                               // default hold is zero, over time the machine pays out whatever is wagered

#define NUM_LED_DATAIN          7
#define NUM_LED_CLK             6
#define NUM_LED_LOAD            5
#define NUM_CHIP_COUNT          1

#define MATRIX_LED_DATAIN       8
#define MATRIX_LED_CLK          13
#define MATRIX_LED_LOAD         12
#define MATRIX_CHIP_COUNT       4

#define LOW_INTENSITY           1                                               // dim
#define HIGH_INTENSITY          10                                              // bright

#define SIREN_FLASHES           1

#define LCD_SCREEN_WIDTH        20
#define LCD_SCREEN_HEIGHT       4

#define CREDITS_I2C_SLAVE_ADDR  0x10                                            // I2C addresses
#define LCD_I2C_ADDR            0x3F                                            // LCD display w/ 4 lines

#define BACKLIGHT_PIN           3
#define En_pin                  2
#define Rw_pin                  1
#define Rs_pin                  0
#define D4_pin                  4
#define D5_pin                  5
#define D6_pin                  6
#define D7_pin                  7

#define MENU_SIZE               17

#define MAIN_MENU_NUMBER        0
#define MAIN_MENU_ELEMENTS      6
char *mainMenu[] =       {                       "Play",
                                                 "Bet",
                                                 "Settings",
                                                 "Metrics",
                                                 "Reset",
                                                 "Hold",    
                                                 " ",    
                                                 " ",    
                                                 " ",    
                                                 " ",    
                                                 " ",    
                                                 " ",    
                                                 " ",    
                                                 " ",    
                                                 " ",    
                                                 " ",    
                                                 " "    };

#define BET_MENU_NUMBER 1
#define BET_MENU_ELEMENTS       3
char *betMenu[] =        {                       "+5 credits: ",                // TODO: make this dynamic based on WAGER_INCREMENT
                                                 "-5 credits: ",
                                                 "Back",    
                                                 " ",    
                                                 " ",    
                                                 " ",    
                                                 " ",    
                                                 " ",    
                                                 " ",    
                                                 " ",    
                                                 " ",    
                                                 " ",    
                                                 " ",    
                                                 " ",    
                                                 " ",    
                                                 " ",    
                                                 " "    };


#define SETTINGS_MENU_NUMBER    2
#define SETTINGS_MENU_ELEMENTS  3
#define SETTINGS_BACK_ITEM      2
char *settingsMenu[] =        {                  "Auto/Manual",                 // TODO: fill out this menu with more cool options
                                                 "Toggle Sound ",
                                                 "Back ",    
                                                 " ",    
                                                 " ",    
                                                 " ",    
                                                 " ",    
                                                 " ",    
                                                 " ",    
                                                 " ",    
                                                 " ",    
                                                 " ",    
                                                 " ",    
                                                 " ",    
                                                 " ",    
                                                 " ",    
                                                 " "    };


#define METRICS_MENU_NUMBER     3
#define METRICS_MENU_ELEMENTS   15
char *metricsMenu[METRICS_MENU_ELEMENTS];

#define HOLD_MENU_NUMBER        4
#define HOLD_MENU_ELEMENTS      3
char *holdMenu[] =        {                      "+1 percent: ",                
                                                 "-1 percent: ",
                                                 "Back",    
                                                 " ",    
                                                 " ",    
                                                 " ",    
                                                 " ",    
                                                 " ",    
                                                 " ",    
                                                 " ",    
                                                 " ",    
                                                 " ",    
                                                 " ",    
                                                 " ",    
                                                 " ",    
                                                 " ",    
                                                 " "    };

int selectPos = 0;
int menuNumber = MAIN_MENU_NUMBER;
int elements = MAIN_MENU_ELEMENTS;

char *currentMenu[MENU_SIZE];

LiquidCrystal_I2C  lcd(     LCD_I2C_ADDR,                                       // Create the LCD display object for the 20x4 display
                            En_pin,
                            Rw_pin,
                            Rs_pin,
                            D4_pin,
                            D5_pin,
                            D6_pin,
                            D7_pin              );

LedControl lc=LedControl(   MATRIX_LED_DATAIN,                                  // Create the LED display object for the 8x8 matrix
                            MATRIX_LED_CLK,
                            MATRIX_LED_LOAD,
                            MATRIX_CHIP_COUNT   );                              // Pins: DIN,CLK,CS, # of chips connected

volatile int reelArrayPos[NUMREELS];
volatile byte machineState;
volatile byte event = EVENT_NONE;
volatile byte color = RED;

#define ADC_READ_PIN            0                                               // we read the voltage from this floating pin to seed the random number generator
#define RED_PIN                 9                                               // Pin locations for the RGB LED
#define GREEN_PIN               10
#define BLUE_PIN                3

#define NUM_NOTES               5                                               // The number of notes in the melody
                                                                                // EEProm address locations
#define PAYEDOUT_ADDR           0x00                                            // 4 bytes
#define WAGERED_ADDR            0x04                                            // 4 bytes
#define PLAYED_ADDR             0x08                                            // 4 bytes
#define TWO_MATCH_ADDR          0x12                                            // 4 bytes
#define THREE_MATCH_ADDR        0x16                                            // 2 bytes
#define SHIP_ONE_MATCH_ADDR     0x18                                            // 4 bytes
#define SHIP_TWO_MATCH_ADDR     0x22                                            // 2 bytes
#define SHIP_THREE_MATCH_ADDR   0x24                                            // 2 bytes
#define EEPROM_WRITES_ADDR      0x34                                            // 4 bytes
#define RESET_FLAG_ADDR         0x38                                            // 4 bytes
#define CREDIT_BALANCE_ADDR     0x42                                            // 4 bytes
#define HOLD_ADDR               0x46                                            // 2 bytes

boolean sound = true;
byte reelMatches = 0;                                                           // per play variables
byte shipMatches = 0;

unsigned long wagered = 0;                                                      // amount wagered on a single spin
double owedExcess = 0;                                                          // change, need to track this so hold is accurate
unsigned long twoMatchCount = 0;                                                // 1 if two symbols match
unsigned int threeMatchCount = 0;                                               // 1 if three symbols match
unsigned long shipOneMatchCount = 0;                                            // 1 if there's one ship present
unsigned int shipTwoMatchCount = 0;                                             // 1 if there are two ships present
unsigned int shipThreeMatchCount = 0;                                           // 1 if there are three ships present (Jackpot!)
unsigned long totalCalcs = 0;                                                   // total plays only relavent in auto mode
signed long startingCreditBalance;                                              // the credit balance before spinning
int increment = WAGER_INCREMENT;
#define DISP_CREDIT_INCREMENT  1                                                // on the seven segment display, increment/decrement the balance by this value until the final value is reached.
                                                                                // lifetime variables (stored in EEprom) Reset sets most back to zero
unsigned long storedPayedOut;                                                   // sum of all payouts
unsigned long storedWagered;                                                    // sum of all wagers  (profit = payouts - wagers)
unsigned long storedPlays;                                                      // the number of spins
unsigned long  storedTwoMatchCount;                                             // number of times two symbols have matched
unsigned int  storedThreeMatchCount;                                            // number of times three symbols have matched
unsigned long  storedShipOneMatchCount;                                         // number of times one ship has appeared
unsigned int  storedShipTwoMatchCount;                                          // number of time two ships have appeared
unsigned int  storedShipThreeMatchCount;                                        // number of times three ships have appeared (Jackpot!)
unsigned long storedEEpromWrites;                                               // number of times we've written to EEprom.  100,000 is the approximate maximum
signed long storedCreditBalance;                                                // the credit balance.
int storedHold = DEFAULT_HOLD;                                                  // the house advantage, in percent, usually between 1 and 15, 2 bytes  

volatile byte portdhistory = 0b00000100;                                        // default is high because of the pull-up, correct setting
volatile byte portchistory = 0b00001110;                                        // default is high because of the pull-up, correct setting
 
//- Debugging Routines                                                          // These routines are helpful for debugging, I will leave them in for your use.
                                                                                // For sending output to the serial monitor. Set the baud rate in setup.
void debug(String text) {
  if (DEBUG) {
    Serial.println(text);
  }
}

void debugNoLF(String text) {
  if (DEBUG) {
    Serial.print(text);
  }
}

void debugInt(signed int anInt) {
  if (DEBUG) {
    char myInt[10];
    itoa(anInt,myInt,10);
    debug(myInt);
  }
}

void debugLong(signed long aLong) {
  if (DEBUG) {
    char myLong[10];
    ltoa(aLong,myLong,10);
    debug(myLong);
  }
}

void debugDouble(double aDouble) {
  if (DEBUG) {
    char *myDouble = ftoa(aDouble);
    debug(myDouble);
  }
}

void debugMetric(const char myString[], signed int anInt) {
  if (DEBUG) {
    debugNoLF(myString);debugNoLF(F(": "));
    debugInt(anInt);
    Serial.print(F("\r\n"));
  }
}

void debugMetricLong(const char myString[], signed long aLong) {
  if (DEBUG) {
    debugNoLF(myString);debugNoLF(F(": "));
    debugLong(aLong);
    Serial.print(F("\r\n"));
  }
}

void debugStoredMetrics() {
  for (int i = 0; i < 11; i++) {
    debug(metricsMenu[i]);
  }
}

void debugMetricDouble(const char myString[], double aDouble) {
  if (DEBUG) {
    debugNoLF(myString);debugNoLF(F(": "));
    debugDouble(aDouble);
    Serial.print(F("\r\n"));
  }
}

                                                                                // quick and dirty ftoa for legacy code
char *ftoa(double f)                                                            // from https://www.microchip.com/forums/m1020134.aspx
{
    static char        buf[17];
    char *            cp = buf;
    unsigned long    l, rem;

    if(f < 0) {
        *cp++ = '-';
        f = -f;
    }
    l = (unsigned long)f;
    f -= (double)l;
    rem = (unsigned long)(f * 1e6);
    sprintf(cp, "%lu.%10.10lu", l, rem);
    return buf;
}

//- All Other Functions

void beep() {                                                                   // Beep and flash LED green unless STATE_AUTO
  setGreen();
  if (sound) {
    BUZZER_PORT |= (1 << BUZZER_PIN);                                           // turn on buzzer
    if (!(STATE_AUTO == (machineState & STATE_AUTO))) {
      delay(100);
    }
    BUZZER_PORT &= ~(1 << BUZZER_PIN);                                          // turn off the buzzer
  }
  setOff();
}

void beepAuto() {                                                               // Beep even during STATE_AUTO, flash LED blue
  setBlue();
  if (sound) {
    BUZZER_PORT |= (1 << BUZZER_PIN);                                           // turn on buzzer
    delay(100);
    BUZZER_PORT &= ~(1 << BUZZER_PIN);                                          // turn off the buzzer
  }
  setOff();
}


void beepPurple() {                                                             // Beep and flash LED purple unless STATE_AUTO
  if (!(STATE_AUTO == (machineState & STATE_AUTO))) {
    setPurple();
    if (sound) {
      BUZZER_PORT |= (1 << BUZZER_PIN);                                         // turn on buzzer
      delay(100);
      BUZZER_PORT &= ~(1 << BUZZER_PIN);                                        // turn off the buzzer
    }
    setOff();
  }
}

void InitInturrupts()                                                           // Initialize interrupts for buttons and switches
{                                                                               
  PCICR |= (1 << PCIE_BUTTON);                                                  // Pin Change Interrupt Control Register, set PCIE2 to enable PCMSK2 scan
  PCICR |= (1 << PCIE_NAV);                                                     // Pin Change Interrupt Control Register, set PCIE1 to enable PCMSK1 scan
                                                                                // Pin Change Mask Register 2 for port D
  PCMSK_BUTTON|=(1<<BUTTON_SPIN_INT);                                           // Set PCINT2 to trigger an interrupt on state change
  PCMSK_NAV|=((1<<NAV_UP_INT)|(1<<NAV_DOWN_INT)|(1<<SELECT_INT));               // Set PCINT1 to trigger an interrupt on state change
  sei();                                                                        // enable interrupts
}

ISR (PCINT1_vect)
{
  byte changeddbits;
  changeddbits = NAV_PIN ^ portchistory;

  ClearBitNo(changeddbits,PORTC0);                                              // not a switch, ignore it
  ClearBitNo(changeddbits,PORTC4);                                              // not a switch, ignore it
  ClearBitNo(changeddbits,PORTC5);                                              // not a switch, ignore it
  ClearBitNo(changeddbits,PORTC6);                                              // not a switch, ignore it

  portchistory = NAV_PIN;

  ClearBitNo(portchistory,PORTC0);                                              // not a switch, ignore it
  ClearBitNo(portchistory,PORTC4);                                              // not a switch, ignore it
  ClearBitNo(portchistory,PORTC5);                                              // not a switch, ignore it
  ClearBitNo(portchistory,PORTC6);                                              // not a switch, ignore it

  if(changeddbits & (1 << NAV_UP_PIN)) 
  {
    if( (portchistory & (1 << NAV_UP_PIN)) == (1 << NAV_UP_PIN) )               // TODO: test using this instead of 16
    {
      _delay_us(DEBOUNCE_TIME);
      if (bit_is_set(NAV_PIN, NAV_UP_PIN)) {                                    // LOW to HIGH pin change (button released)
        // ADD CODE HERE
        int x = 0;
      }
    }
    else
    {
      _delay_us(DEBOUNCE_TIME);
      if (bit_is_clear(NAV_PIN, NAV_UP_PIN)) {                                  // button pressed
                                                                                // HIGH to LOW pin change (spin switch button pressed)
        event = EVENT_NAV_UP;
      }
    }
  }

  if(changeddbits & (1 << NAV_DOWN_PIN)) 
  {
    if( (portchistory & (1 << NAV_DOWN_PIN)) == (1 << NAV_DOWN_PIN) )           // TODO: test using this instead of 16
    {
      _delay_us(DEBOUNCE_TIME);
      if (bit_is_set(NAV_PIN, NAV_DOWN_PIN)) {                                  // LOW to HIGH pin change (button released)
        // ADD CODE HERE
        int x = 0;
      }
    }
    else
    {
      _delay_us(DEBOUNCE_TIME);
      if (bit_is_clear(NAV_PIN, NAV_DOWN_PIN)) {                                // button pressed
                                                                                // HIGH to LOW pin change (spin switch button pressed)
        event = EVENT_NAV_DOWN;
      }
    }
  }

  if(changeddbits & (1 << SELECT_PIN)) 
  {
    if( (portchistory & (1 << SELECT_PIN)) == (1 << SELECT_PIN) )               // TODO: test using this instead of 16
    {
      _delay_us(DEBOUNCE_TIME);
      if (bit_is_set(NAV_PIN, SELECT_PIN)) {                                    // LOW to HIGH pin change (button released)
        int x = 0;
      }
    }
    else
    {
      _delay_us(DEBOUNCE_TIME);
      if (bit_is_clear(NAV_PIN, SELECT_PIN)) {                                  // button pressed
                                                                                // HIGH to LOW pin change (spin switch button pressed)
        event = EVENT_SELECT;
      }
    }
  }
}

ISR (PCINT2_vect)
{
  byte changeddbits;                                                            // Will have bit corresponding to button pressed flipped on

  changeddbits = BUTTON_PIN ^ portdhistory;                                     // flip the bit corresponding to the button that was pressed

  ClearBitNo(changeddbits,PORTD0);                                              // not a switch, ignore it
  ClearBitNo(changeddbits,PORTD1);                                              // not a switch, ignore it
  ClearBitNo(changeddbits,PORTD3);                                              // not a switch, ignore it
  ClearBitNo(changeddbits,PORTD4);                                              // not a switch, ignore it
  ClearBitNo(changeddbits,PORTD5);                                              // not a switch, ignore it
  ClearBitNo(changeddbits,PORTD6);                                              // not a switch, ignore it
  ClearBitNo(changeddbits,PORTD7);                                              // not a switch, ignore it

  portdhistory = BUTTON_PIN;                                                    // set history = to the current state of input

  ClearBitNo(portdhistory,PORTD0);                                              // not a switch, ignore it
  ClearBitNo(portdhistory,PORTD1);                                              // not a switch, ignore it
  ClearBitNo(portdhistory,PORTD3);                                              // not a switch, ignore it
  ClearBitNo(portdhistory,PORTD4);                                              // not a switch, ignore it
  ClearBitNo(portdhistory,PORTD5);                                              // not a switch, ignore it
  ClearBitNo(portdhistory,PORTD6);                                              // not a switch, ignore it
  ClearBitNo(portdhistory,PORTD7);                                              // not a switch, ignore it

  if(changeddbits & (1 << BUTTON_SPIN_PIN)) 
  {
    if( (portdhistory & (1 << BUTTON_SPIN_PIN)) == (1 << BUTTON_SPIN_PIN) )     // TODO: test using this instead of 16
    {
      _delay_us(DEBOUNCE_TIME);
      if (bit_is_set(BUTTON_PIN, BUTTON_SPIN_PIN)) {                            // LOW to HIGH pin change (button released)
      }
    }
    else
    {
      _delay_us(DEBOUNCE_TIME);
      if (bit_is_clear(BUTTON_PIN, BUTTON_SPIN_PIN)){                           // button pressed
        // HIGH to LOW pin change (spin switch button pressed)
        if(STATE_SPINNING == (machineState & STATE_SPINNING)) {
          SetState(STATE_IDLE);
          event = EVENT_NONE;
        } else if (STATE_IDLE == (machineState & STATE_IDLE)) {
          if (STATE_AUTO == (machineState & STATE_AUTO)) {
            ClearBit(machineState, STATE_AUTO);
          }
          event = EVENT_SPIN;
        }
      }
    }
  }
}

void spinAndEvaluate() {                                                        // runs when the spin button is pressed or we 'Play' from the main menu
//debug("spinAndEvaluate()");
  spin();
  checkForWin();
  signed long winnings = calcWinnings();
  calcStored(winnings);
  if (!(STATE_AUTO == (machineState & STATE_AUTO))) {                           // if we're not in auto mode display the credits
    storeMetrics();
    displayCredits();
    if (reelMatches > 0) {
      celebrateWin(reelMatches);
    }
    setupMetricsMenu(); 
  } else if ((totalCalcs++%EEPROM_FREQ) == 0) {                                 //  EEPROM can be written ~100,000 times,
    storeMetrics();
    displayCredits();                                                           // displayCredits takes care of the sign on increment
    setupMetricsMenu(); 
    debugStoredMetrics();   
    debugMetricDouble("owedExcess",owedExcess);                                 // don't want to put owedExcess in metricsMenu because of global var space shortage                                                     
    if (totalCalcs >= AUTO_MODE_MAX) {                                          // drop out of auto mode when threshold exceeded
      ClearBit(machineState, STATE_AUTO);
      SetState(STATE_IDLE); 
      event = EVENT_NONE;
    }
  }
  ClearBit(machineState, STATE_SPINNING);
}

void spin() {
//debug("spin()");
  SetState(STATE_SPINNING);
  if (!(STATE_AUTO == (machineState & STATE_AUTO))) {
    beep();
  }

  zeroAllBalances();
  
  byte reelsStopped[NUMREELS] = {0,0,0};
  byte stopArrayPos[NUMREELS];
  for (int reelNum = 0; reelNum < NUMREELS; reelNum++) {
    if (!(STATE_AUTO == (machineState & STATE_AUTO))) {
      lc.setIntensity(reelNum,LOW_INTENSITY);                                   // Set intensity levels
    }
    stopArrayPos[reelNum] = random(0,(NUMFRAMES)) * LINESPERFRAME;      
    while (stopArrayPos[reelNum] == reelArrayPos[reelNum]) {                    // keep picking a stop array position until it's not equal to the current position
      stopArrayPos[reelNum] = random(0,(NUMFRAMES)) * LINESPERFRAME;
    }
  }
    while (!allReelsStopped(reelsStopped)) {
    for (int reelNum = 0; reelNum < NUMREELS; reelNum++) {
      if (reelArrayPos[reelNum] == ((NUMFRAMES * LINESPERFRAME) + 1)) {      
        reelArrayPos[reelNum] = 0;                                              // go back to top of reel
      }
      if(reelArrayPos[reelNum] != (stopArrayPos[reelNum]+1)) {
        if (!(STATE_AUTO == (machineState & STATE_AUTO))) {
          for (int row = 0; row < LINESPERFRAME; row++) {                       // simulate a spinning reel
            lc.setRow(reelNum,row,reel[reelArrayPos[reelNum] + row]);           // output to 8x8x3 matrix
          }
        }
        //delay(FRAME_DELAY);
        //reelArrayPos[reelNum] += LINESPERFRAME;                               // uncomment for fast play
        reelArrayPos[reelNum] += 1;
      } else {
        if (!(STATE_AUTO == (machineState & STATE_AUTO))) {
          lc.setIntensity(reelNum,HIGH_INTENSITY);                              // Set intensity levels
        }
        reelsStopped[reelNum] = 1;
      }
    }
  } 
}

void checkForWin() {                                                            // this only works if NUMREELS == 3 ! If you change NUMREELS you must do so programming! 
//debug("checkForWin()");
  
  for (int reelNum=0; reelNum < NUMREELS; reelNum++) {                          // see if ships appeared
    if ((reelArrayPos[reelNum] - 1) == SHIP_LOC) {
      shipMatches += 1;
    }
  }

  for (int i = 0; i < NUMREELS; i++) {                                          // check to see if other symbols matched   
    for (int j = 0; j < NUMREELS; j++) {
      if (reelArrayPos[i]  - 1 == reelArrayPos[j] - 1) {
        reelMatches += 1;
      }
    }
  }

  if (reelMatches == 9) {                                                       // code from the block above sets reelMatches to 9 if 3 symbols match
    reelMatches = 3;
    threeMatchCount++;
  } else if (reelMatches == 5) {                                                // etc...
    reelMatches = 2;
    twoMatchCount++;
  } else if (reelMatches == 3) {
    reelMatches = 0;
  } else {
    reelMatches = -1;                                                           // never used
  }

  if (shipMatches == 3) {
      shipThreeMatchCount++;
  } else if (shipMatches == 2) {
      shipTwoMatchCount++;
  } else if (shipMatches == 1) {
      shipOneMatchCount++;
  }

  if (shipThreeMatchCount) {                                    // Wins are mutually exclusive, subsequent code assumes that!
    threeMatchCount = 0;                                        // TODO: make this a switch statement
    shipTwoMatchCount = 0;
    shipOneMatchCount = 0;
    twoMatchCount = 0;
    reelMatches = 0;
  } else if (threeMatchCount) {
    shipTwoMatchCount = 0;
    shipOneMatchCount = 0;
    twoMatchCount = 0;
    reelMatches = 0;
  } else if (shipTwoMatchCount) {
    shipOneMatchCount = 0;
    twoMatchCount = 0;
    reelMatches = 0;
  } else if (shipOneMatchCount) {
    twoMatchCount = 0;
    reelMatches = 0;
  } else if (twoMatchCount) {
    reelMatches = 0;
  }
}

signed long calcWinnings() {
  double winnings = 0;
  //debugMetric("storedHold",storedHold);
  if(shipThreeMatchCount > 0) {
    winnings = wagered * (THREE_SPACESHIP_PAYOUT - (THREE_SPACESHIP_PAYOUT * (storedHold/100.0))); // winnings are the amount wagered times the payout minus the hold.
  } else if (threeMatchCount > 0) {
    winnings = wagered * (THREE_SYMBOL_PAYOUT - (THREE_SYMBOL_PAYOUT * (storedHold/100.0)));
  } else if (shipTwoMatchCount > 0) {
    winnings = wagered * (TWO_SPACESHIP_PAYOUT - (TWO_SPACESHIP_PAYOUT * (storedHold/100.0)));
  } else if (shipOneMatchCount > 0) {
    winnings = wagered * (ONE_SPACESHIP_PAYOUT - (ONE_SPACESHIP_PAYOUT * (storedHold/100.0)));
  } else if (twoMatchCount > 0) {
    winnings = wagered * (TWO_SYMBOL_PAYOUT - (TWO_SYMBOL_PAYOUT * (storedHold/100.0)));
  } else {
    winnings = 0;
  }
  signed long roundWinnings = (signed long) round(winnings);
  owedExcess += winnings - roundWinnings;                                       // owedExcess is the change; credits between -1 and 1.
  if (owedExcess >= 1 || owedExcess <= -1) {                                    // if we can pay out some excess
    int roundOwedExcess = (int) round(owedExcess);
    roundWinnings += roundOwedExcess;                                           // add the rounded portion to the winnings
    owedExcess -= roundOwedExcess;                                              // subtract out what we added to continue to track the excess
  } 
  roundWinnings -= wagered;                                                     // you pay for your bet whether you won or not!  
//  winnings -= wagered;
  return roundWinnings;
//  return((signed long) round(winnings));
}

void calcStored(signed long winnings) {
    storedPayedOut += winnings;
    storedWagered += wagered;
    startingCreditBalance = storedCreditBalance;
    storedCreditBalance += winnings;
    storedPlays += 1;                                                           // calcStored is called one time per play     
    storedTwoMatchCount += twoMatchCount;       
    storedThreeMatchCount += threeMatchCount;     
    storedShipOneMatchCount += shipOneMatchCount; 
    storedShipTwoMatchCount += shipTwoMatchCount; 
    storedShipThreeMatchCount += shipThreeMatchCount;
}

void storeMetrics() {
    beepAuto();                                                                 // so we know we're not hung in auto mode.
    updateStoredPayedOut();
    updateStoredWagered();
    updateStoredPlays();                
    updateStoredTwoMatchCount();        
    updateStoredThreeMatchCount();      
    updateStoredShipOneMatchCount();   
    updateStoredShipTwoMatchCount();   
    updateStoredShipThreeMatchCount(); 
    storedEEpromWrites++;
    updateStoredEEpromWrites();      
    updateStoredCreditBalance();   
    updateStoredHold();
}

void displayCredits() {
//debug("displayCredits()");
  int xmitIncrement;
  if ((STATE_AUTO == (machineState & STATE_AUTO))) {                            // display the credits here if we're in auto mode.
    xmitIncrement = abs(startingCreditBalance - storedCreditBalance);           // we don't want the display slave to count up/down
  } else {
    xmitIncrement = DISP_CREDIT_INCREMENT;                                      // set increment back to what it should be during manual play
  }
  
  Wire.beginTransmission(CREDITS_I2C_SLAVE_ADDR);

  Wire.write( startingCreditBalance & 0xFF); 
  Wire.write((startingCreditBalance & 0xFF00) >> 8);
  Wire.write((startingCreditBalance & 0xFF0000) >> 16);
  Wire.write((startingCreditBalance & 0xFF000000) >> 24);                       // most sigificant byte sent last

  if (startingCreditBalance > storedCreditBalance) {                            // if the player lost,
    xmitIncrement *= -1;                                                        // flip the sign on increment so we count down
  }
  Wire.write( xmitIncrement & 0xFF); 
  Wire.write((xmitIncrement & 0xFF00) >> 8);

  Wire.write( storedCreditBalance & 0xFF); 
  Wire.write((storedCreditBalance & 0xFF00) >> 8);
  Wire.write((storedCreditBalance & 0xFF0000) >> 16);
  Wire.write((storedCreditBalance & 0xFF000000) >> 24);                         // most sigificant byte sent last

  byte error = Wire.endTransmission();
  if (error==4)
  {
    debug(F("Unknown error at address"));                                       // I've never seen this happen.
  }    

}

bool allReelsStopped(byte reelsStopped[]) {
  byte sumStopped = 0;
  for (int i; i < NUMREELS; i++) {
    if (reelsStopped[i] == 1) {
      sumStopped += 1;
    }
  }
  if (sumStopped == NUMREELS) {                                                 // all reels stopped
    return 1;
  }
  return 0;
}

void celebrateWin(byte matches) {                                               // we can probably do better than this.  I've never seen it run for a three ship match...
//debug("celebrateWin()");
  for (int i = 0; i < (matches - 1); i++) {
    playSiren();
    delay(ONE_SECOND);
  }
}

void playSiren() {                                                              // play siren and toggle the RGB LED blue and red
//debug("playSiren()");
  for (int j = 1; j <= SIREN_FLASHES; j++){
    setBlue();
    for (int note = MIN_NOTE; note <= MAX_NOTE; note+=5){                       // 5 = # notes to step over. Necessary only w/ TimerFreeTone library.
      if (note%1236==0) {                                                       // at the top of the range change RGB color.
        if (color == RED) {
          for (int reelNum = 0; reelNum < NUMREELS; reelNum++) {
            lc.setIntensity(reelNum, LOW_INTENSITY);                            // this doesn't seem to be working...
          }
          setBlue();
        }
        if (color == BLUE) {
          for (int reelNum = 0; reelNum < NUMREELS; reelNum++) {
            lc.setIntensity(reelNum, HIGH_INTENSITY);                           // this doesn't seem to be working...
          }
          setRed();
        }
      }
      if (sound) {
        TimerFreeTone(TONE_PIN, note, 1);                                       // third parameter is duration
      }
    }
  }
  setOff();
  for (int reelNum = 0; reelNum < NUMREELS; reelNum++) {
    lc.setIntensity(reelNum, HIGH_INTENSITY);
  }
}

void setPurple() {
  //debug("setPurple()");
  setColor(170, 0, 255);                                                        // Purple Color
  color = PURPLE;
}

void setRed(){
  //debug("setRed()");
  setColor(255, 0, 0);                                                          // Red Color
  color = RED;
}

void setGreen(){
  //debug("setGreen()");
  setColor(0, 255, 0);                                                          // Green Color
  color = GREEN;
}

void setBlue(){
  //debug("setBlue()");
  setColor(0, 0, 255);                                                          // Blue Color
  color = BLUE;
}

void setWhite(){
  //debug("setWhite()");
  setColor(255, 255, 255);                                                      // White Color
  color = WHITE;
}

void setOff(){
  //debug("setOff()");
  setColor(0,0,0);                                                              // Off
  color = OFF;
}

void setColor(int redValue, int greenValue, int blueValue) {
  //debug("setColor()");
  analogWrite(RED_PIN, redValue);
  analogWrite(GREEN_PIN, greenValue);
  analogWrite(BLUE_PIN, blueValue);
}

void showColor(int color) {                                                     // There's got to be a better way to do this...
  switch(color) {
     case RED  :
        setRed();
        break; 
     case GREEN  :
        setGreen();
        break; 
     case BLUE  :
        setBlue();
        break;
     case PURPLE  :
        setPurple();
        break;
     case WHITE  :
        setWhite();
        break;
     case OFF  :
        setOff();
        break;
     default :
        setOff();
  }  
}

void playMelody() {
//debug("playMelody()");  
  const int melody[] = {                                                        // notes in the melody:
      NOTE_A2, NOTE_B2, NOTE_G2, NOTE_G1, NOTE_D2                               // "Close Encounters" tones
  };
    
  for (int thisNote = 0; thisNote < NUM_NOTES; thisNote++) {                    // iterate over the notes of the melody:
    showColor(thisNote + 1);                                                    // to calculate the note duration, take one second divided by the note type.
                                                                                //e.g. quarter note = 1000 / 4, eighth note = 1000/8, etc.
    int noteDuration = 500;
    TimerFreeTone(TONE_PIN, melody[thisNote], noteDuration); 
    delay(100);
  }
  setOff();
}

void playAnimation() {                                                          // TODO: add ship animation
  for (int i = 0; i < 3; i++){
    for (int reelNum = 0; reelNum < NUMREELS; reelNum++){
      for (int row = 0; row < LINESPERFRAME; row++) {                           // Play animation at beginning
        lc.setRow(reelNum,row,reel[ALIEN_1_LOC + row]);                         // output to 8x8x3 matrix
      }
      delay(250);
      for (int row = 0; row < LINESPERFRAME; row++) {                           // Play animation at beginning
        lc.setRow(reelNum,row,reel[ALIEN_2_LOC + row]);                         // output to 8x8x3 matrix
      }
      delay(250);
    }
  }
}

void zeroAllBalances() {                                                        // set all balances back to zero.
  reelMatches = 0;
  shipMatches = 0;
  wagered = MINIMUM_WAGER;
  twoMatchCount = 0;
  threeMatchCount = 0;
  shipOneMatchCount = 0;
  shipTwoMatchCount = 0;
  shipThreeMatchCount = 0;
}

unsigned long getStoredPayedOut(){
    return (long) eeprom_read_dword(PAYEDOUT_ADDR) ;
}

unsigned long getStoredWagered() {
    return (unsigned long) eeprom_read_dword(WAGERED_ADDR) ;
}

unsigned long getStoredPlays() {  
    return (unsigned long) eeprom_read_dword(PLAYED_ADDR);
}              

unsigned long getStoredTwoMatchCount() {        
    return (unsigned long) eeprom_read_word(TWO_MATCH_ADDR);
}              

unsigned int getStoredThreeMatchCount() {      
    return (unsigned int) eeprom_read_word(THREE_MATCH_ADDR);
}              

unsigned long  getStoredShipOneMatchCount() {   
    return (unsigned long) eeprom_read_word(SHIP_ONE_MATCH_ADDR);
}              

unsigned int  getStoredShipTwoMatchCount() {   
    return (unsigned int) eeprom_read_word(SHIP_TWO_MATCH_ADDR);
}              

unsigned int  getStoredShipThreeMatchCount() { 
    return (unsigned int) eeprom_read_word(SHIP_THREE_MATCH_ADDR);
}              

unsigned long getStoredEEpromWrites() {
  return (unsigned long) eeprom_read_dword(EEPROM_WRITES_ADDR);
}

unsigned long getResetFlag() {
  return (long) eeprom_read_dword(RESET_FLAG_ADDR);
}

unsigned long getStoredCreditBalance() {
  return (signed long) eeprom_read_dword(CREDIT_BALANCE_ADDR);
}

int getStoredHold() {
  return (int) eeprom_read_dword(HOLD_ADDR);
}

void updateStoredPayedOut() {                                                   // all of these update functions are only called from one place and they are unnecessary.
    eeprom_write_dword (PAYEDOUT_ADDR, (uint32_t) storedPayedOut);
}

void updateStoredWagered() {
    eeprom_write_dword (WAGERED_ADDR, (uint32_t) storedWagered);
}

void updateStoredPlays() {
    eeprom_write_dword (PLAYED_ADDR, (uint32_t) storedPlays);                
}

void updateStoredTwoMatchCount() {        
    eeprom_write_word (TWO_MATCH_ADDR, (uint32_t) storedTwoMatchCount);
}

void updateStoredThreeMatchCount() {      
    eeprom_write_word (THREE_MATCH_ADDR, (uint16_t) storedThreeMatchCount);
}

void updateStoredShipOneMatchCount() {   
    eeprom_write_word (SHIP_ONE_MATCH_ADDR, (uint32_t) storedShipOneMatchCount);
}

void updateStoredShipTwoMatchCount() {   
    eeprom_write_word (SHIP_TWO_MATCH_ADDR, (uint16_t) storedShipTwoMatchCount);
}

void updateStoredShipThreeMatchCount() { 
    eeprom_write_word (SHIP_THREE_MATCH_ADDR, (uint16_t) storedShipThreeMatchCount);
}

void updateStoredEEpromWrites() {         
    eeprom_write_dword (EEPROM_WRITES_ADDR, (uint32_t) storedEEpromWrites);
}

void updateResetFlag() {         
    eeprom_write_dword (RESET_FLAG_ADDR, (uint32_t) 0);
}

void updateStoredCreditBalance() {         
    eeprom_write_dword (CREDIT_BALANCE_ADDR, (int32_t) storedCreditBalance);
}

void updateStoredHold() {         
    eeprom_write_dword (HOLD_ADDR, (int32_t) storedHold);
}

void resetEEprom() {
    eeprom_write_dword (PAYEDOUT_ADDR, (uint32_t) 0);
    eeprom_write_dword (WAGERED_ADDR, (uint32_t) 0);
    eeprom_write_dword (PLAYED_ADDR, (uint32_t) 0);                
    eeprom_write_word (TWO_MATCH_ADDR, (uint16_t) 0);
    eeprom_write_word (THREE_MATCH_ADDR, (uint16_t) 0);
    eeprom_write_word (SHIP_ONE_MATCH_ADDR, (uint16_t) 0);
    eeprom_write_word (SHIP_TWO_MATCH_ADDR, (uint16_t) 0);
    eeprom_write_word (SHIP_THREE_MATCH_ADDR, (uint16_t) 0);
    eeprom_write_dword (EEPROM_WRITES_ADDR, (uint32_t) ++storedEEpromWrites);   // this should never be set back to 0
    eeprom_write_dword (CREDIT_BALANCE_ADDR, (int32_t) STARTING_CREDIT_BALANCE);
    eeprom_write_dword (RESET_FLAG_ADDR, (uint32_t) 0);
}

void LCDPrintLine1Thru4(char* line1ToPrint, char* line2ToPrint, char* line3ToPrint, char* line4ToPrint) {
  LCDBlank();
  lcd.setCursor(0, 0);
  lcd.print(line1ToPrint);
  lcd.setCursor(0, 1);
  lcd.print(line2ToPrint);
  lcd.setCursor(0, 2);
  lcd.print(line3ToPrint);
  lcd.setCursor(0, 3);
  lcd.print(line4ToPrint);
}


void LCDPrintLine1Clear2(char* lineToPrint) {
  lcd.setCursor(0, 0);
  lcd.print(lineToPrint);
  LCDBlankLine2();
}

void LCDPrintLine1(char* lineToPrint) {
  LCDBlankLine1();
  lcd.setCursor(0,0);
  lcd.print(lineToPrint);
}

void LCDPrintLine2(char* lineToPrint) {
  LCDBlankLine2();
  lcd.setCursor(0,1);
  lcd.print(lineToPrint);
}

void LCDPrintLine3(char* lineToPrint) {
  LCDBlankLine3();
  lcd.setCursor(0,2);
  lcd.print(lineToPrint);
}

void LCDPrintLine4(char* lineToPrint) {
  LCDBlankLine4();
  lcd.setCursor(0,3);
  lcd.print(lineToPrint);
}

void LCDBlank() {
  LCDBlankLine1();
  LCDBlankLine2();
  LCDBlankLine3();
  LCDBlankLine4();
}

void LCDBlankLine1() {
  lcd.setCursor(0,0);
  PrintBlankLine();
}

void LCDBlankLine2() {
  lcd.setCursor(0,1);
  PrintBlankLine();
}

void LCDBlankLine3() {
  lcd.setCursor(0,2);
  PrintBlankLine();
}

void LCDBlankLine4() {
  lcd.setCursor(0,3);
  PrintBlankLine();
}

void PrintBlankLine() {
  lcd.print("                    ");
}

void LCDShowMenu(int position, char **menu) {
  char line1[LCD_SCREEN_WIDTH+1] = "";
  strcat(line1,"->");
  strcat(line1,menu[position]);
  LCDPrintLine1Thru4(line1,menu[position+1],menu[position+2],menu[position+3]);
}

void LCDMenuUp(char **menu) { 
  if (selectPos > 0) {
    selectPos -= 1;
    LCDShowMenu(selectPos, menu);
  }
  if (menuNumber == BET_MENU_NUMBER) {
    showWager();
  }
  if (menuNumber == HOLD_MENU_NUMBER) {
    showHold();
  }
}

void LCDMenuDown(char **menu){ 
  if (selectPos < (elements - 1)) {
    selectPos +=1;
    LCDShowMenu(selectPos, menu);
  }
  if (menuNumber == BET_MENU_NUMBER) {
    showWager();
  }
  if (menuNumber == HOLD_MENU_NUMBER) {
    showHold();
  }
}

void getAllStoredValues() {
  storedPayedOut = getStoredPayedOut();
  storedWagered = getStoredWagered();
  storedPlays = getStoredPlays();
  storedTwoMatchCount = getStoredTwoMatchCount();
  storedThreeMatchCount = getStoredThreeMatchCount();
  storedShipOneMatchCount = getStoredShipOneMatchCount();
  storedShipTwoMatchCount = getStoredShipTwoMatchCount();
  storedShipThreeMatchCount = getStoredShipThreeMatchCount();
  storedEEpromWrites = getStoredEEpromWrites();
  storedCreditBalance = getStoredCreditBalance();
  storedHold = getStoredHold();
}

void resetAllStoredValues() {
  storedPayedOut              = 0;
  storedWagered               = 0;
  storedPlays                 = 0;
  storedTwoMatchCount         = 0;
  storedThreeMatchCount       = 0;
  storedShipOneMatchCount     = 0;
  storedShipTwoMatchCount     = 0;
  storedShipThreeMatchCount   = 0;
  //storedEEpromWrites        = 0;                                              // leave commented out
  storedCreditBalance         = STARTING_CREDIT_BALANCE;
  storedHold                  = DEFAULT_HOLD;
}

void showWager() {                                                              // augment the bet menu to show the wager amounts
  if (selectPos == 0) {
    char myHigherWager[10];
    itoa(wagered + increment,myHigherWager,10);
    lcd.setCursor(15,0);
    lcd.print("     ");
    lcd.setCursor(15,0);
    lcd.print(myHigherWager);
    char myLowerWager[10];
    itoa(wagered - increment,myLowerWager,10);
    lcd.setCursor(15,1);
    lcd.print("     ");
    lcd.setCursor(15,1);
    lcd.print(myLowerWager);
  } else if (selectPos == 1) {
    char myLowerWager[10];
    itoa(wagered - increment,myLowerWager,10);
    lcd.setCursor(15,0);
    lcd.print("     ");
    lcd.setCursor(15,0);
    lcd.print(myLowerWager);
  }
  lcd.setCursor(0,4);
  PrintBlankLine();
  lcd.setCursor(0,4);
  char myWager[10];
  itoa(wagered,myWager,10);
  lcd.print("Wager: ");
  lcd.print(myWager);    
}

void showHold() {                                                              // augment the bet menu to show the wager amounts
  if (selectPos == 0) {
    char myHigherHold[10];
    itoa(storedHold + 1,myHigherHold,10);
    lcd.setCursor(15,0);
    lcd.print("     ");
    lcd.setCursor(15,0);
    lcd.print(myHigherHold);
    char myLowerHold[10];
    itoa(storedHold - 1,myLowerHold,10);
    lcd.setCursor(15,1);
    lcd.print("     ");
    lcd.setCursor(15,1);
    lcd.print(myLowerHold);
  } else if (selectPos == 1) {
    char myLowerHold[10];
    itoa(storedHold - 1,myLowerHold,10);
    lcd.setCursor(15,0);
    lcd.print("     ");
    lcd.setCursor(15,0);
    lcd.print(myLowerHold);
  }
  lcd.setCursor(0,4);
  PrintBlankLine();
  lcd.setCursor(0,4);
  char myHold[10];
  itoa(storedHold,myHold,10);
  lcd.print("Hold: ");
  lcd.print(myHold);    
}

void setup()
{
  Serial.begin(BAUD_RATE);
  while(!Serial);                                                               // wait for serial port to connect. Needed for native USB
//debug("setup()");
  
  lcd.begin (LCD_SCREEN_WIDTH,LCD_SCREEN_HEIGHT);                               //  <<----- Initial LCD is 20x4

  int reelNum;
  for (reelNum = 0; reelNum <= NUMREELS; reelNum++) {
    lc.shutdown(reelNum,false);                                                 // Wake up displays
    lc.setIntensity(reelNum,HIGH_INTENSITY);                                    // Set intensity levels
    lc.clearDisplay(reelNum);                                                   // Clear Displays
  }

  pinMode(RED_PIN, OUTPUT);                                                     // RGB LED
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);
  pinMode(ADC_READ_PIN, INPUT);                                                 // this pin will float in a high impedance/Hi-Z state and it's voltage
                                                                                // will be read with every spin to seed the random number generator.
  BUZZER_DDR |= (1 << BUZZER_PIN);                                              // set buzzer pin to output
  BUZZER_PORT &= ~(1 << BUZZER_PIN);                                            // turn off the buzzer

  BUTTON_DDR &= ~(1 << BUTTON_SPIN_PIN);                                        // set spin button to input with a 0 (0 is default)
  NAV_DDR &= ~(1 << NAV_UP_PIN);                                                // set spin button to input with a 0 (0 is default)
  NAV_DDR &= ~(1 << NAV_DOWN_PIN);                                              // set spin button to input with a 0 (0 is default)
  NAV_DDR &= ~(1 << SELECT_PIN);                                                // set spin button to input with a 0 (0 is default)
  BUTTON_PORT|=(1<<BUTTON_SPIN_PORT);                                           // Turn on the pull-up for the spin buttons
  NAV_PORT|=((1<<NAV_UP_PORT)|(1<<NAV_DOWN_PORT)|(1<<SELECT_PORT));             // Turn on the pull-up for the spin buttons.  THIS IS WHAT MAKES US GO WONKEY!
  portdhistory =  BUTTON_PIN;
  portchistory =  NAV_PIN;
  InitInturrupts();

  for (int i = 0; i < NUMREELS; i++) {                                          // set the initial position for each reel
    reelArrayPos[i] = NUMREELS * i;
  }
  beep();
  
  lcd.setBacklightPin(BACKLIGHT_PIN,POSITIVE);                                  // Switch on the backlight 
  lcd.setBacklight(HIGH);
  lcd.home (); // go home
  
  LCDPrintLine1("Close Encounters");
  LCDPrintLine2("Slots");

  playAnimation();
  playMelody();

  if (getResetFlag() == 0xFFFFFFFF) {                                           // EEprom set to all 1s before it's ever used
    updateResetFlag();                                                          // set it to all zeros
    resetEEprom();                                                              // set everything to 0 (except startingCreditBalance)
  }
  
  getAllStoredValues();                                                         // read all values from EEprom

  setupMetricsMenu();

  Wire.begin();                                                                 // join i2c bus (address optional for master)
  TWBR=32;                                                                      // == 100kHz SCL frequency  
  
  SetState(STATE_IDLE);
  event = EVENT_SHOW_MENU;

  startingCreditBalance = storedCreditBalance;
  displayCredits();

  int arraySize = 0;
  for (int i = 0; i < MENU_SIZE; i++) {
    arraySize += sizeof(mainMenu[i]);  
  }

  memcpy(currentMenu, mainMenu, arraySize);
  elements = MAIN_MENU_ELEMENTS;
  randomSeed(analogRead(ADC_READ_PIN));                                          // do not ground this pin; use this or randomSeed(millis());
}

void setupMetricsMenu() {

  metricsMenu[0] =   "1                 ";
  metricsMenu[1] =   "2                 ";
  metricsMenu[2] =   "3                 ";
  metricsMenu[3] =   "4                 ";
  metricsMenu[4] =   "5                 ";
  metricsMenu[5] =   "6                 ";
  metricsMenu[6] =   "7                 ";
  metricsMenu[7] =   "8                 ";
  metricsMenu[8] =   "9                 ";
  metricsMenu[9] =   "10                ";
  metricsMenu[10] =  "11                ";
  metricsMenu[11] =  "Back";
  metricsMenu[12] =  " ";
  metricsMenu[13] =  " ";
  metricsMenu[14] =  " ";

  strcpy(metricsMenu[0] ,  "PayedOut          ");
  strcpy(metricsMenu[1] ,  "Wagered           ");
  strcpy(metricsMenu[2] ,  "Plays             ");
  strcpy(metricsMenu[3] ,  "2 Match           ");
  strcpy(metricsMenu[4] ,  "3 Match           ");
  strcpy(metricsMenu[5] ,  "Ship 1 Match      ");
  strcpy(metricsMenu[6] ,  "Ship 2 Match      ");
  strcpy(metricsMenu[7] ,  "Ship 3 Match      ");
  strcpy(metricsMenu[8],   "EEprom            ");
  strcpy(metricsMenu[9],   "Credits           ");
  strcpy(metricsMenu[10],  "Hold              ");

  char buffer[8];
  ltoa(storedPayedOut,buffer,10);   
  memcpy(metricsMenu[0] + (sizeof(char) * (18 - strlen(buffer))), buffer, strlen(buffer) );

  ltoa(storedWagered,buffer,10);   
  memcpy(metricsMenu[1] + (sizeof(char) * (18 - strlen(buffer))), buffer, strlen(buffer) );

  ltoa(storedPlays,buffer,10);   
  memcpy(metricsMenu[2] + (sizeof(char) * (18 - strlen(buffer))), buffer, strlen(buffer) );

  ltoa(storedTwoMatchCount,buffer,10);   
  memcpy(metricsMenu[3] + (sizeof(char) * (18 - strlen(buffer))), buffer, strlen(buffer) );

  itoa(storedThreeMatchCount,buffer,10);   
  memcpy(metricsMenu[4] + (sizeof(char) * (18 - strlen(buffer))), buffer, strlen(buffer) );

  ltoa(storedShipOneMatchCount,buffer,10);   
  memcpy(metricsMenu[5] + (sizeof(char) * (18 - strlen(buffer))), buffer, strlen(buffer) );

  itoa(storedShipTwoMatchCount,buffer,10);   
  memcpy(metricsMenu[6] + (sizeof(char) * (18 - strlen(buffer))), buffer, strlen(buffer) );

  itoa(storedShipThreeMatchCount,buffer,10);   
  memcpy(metricsMenu[7] + (sizeof(char) * (18 - strlen(buffer))), buffer, strlen(buffer) );

  ltoa(storedEEpromWrites,buffer,10);   
  memcpy(metricsMenu[8] + (sizeof(char) * (18 - strlen(buffer))), buffer, strlen(buffer) );
  
  ltoa(storedCreditBalance,buffer,10);  
  memcpy(metricsMenu[9] + (sizeof(char) * (18 - strlen(buffer))), buffer, strlen(buffer) );

  itoa(storedHold,buffer,10);  
  memcpy(metricsMenu[10] + (sizeof(char) * (18 - strlen(buffer))), buffer, strlen(buffer) );
}

void loop()
{
  ProcessEvent();  
}

void ProcessEvent()                                                             // processes events
{   //debug("ProcessEvent()");
  if (event == EVENT_NONE) {
        return;
  } else if (event == EVENT_SPIN) {
    spinAndEvaluate();
    // now event == EVENT_SELECT if select button pressed during spinAndEvaluate()
    if (STATE_AUTO == (machineState & STATE_AUTO)) {
      if (event != EVENT_SELECT) {
        event = EVENT_SPIN;
      } 
    } else {
      event = EVENT_NONE;
      SetState(STATE_IDLE);
    }
  } else if (event == EVENT_SHOW_MENU) {
    event = EVENT_NONE;
    selectPos = 0;
    LCDShowMenu(selectPos, currentMenu);
    if (menuNumber == BET_MENU_NUMBER) {
      showWager();
    }
    if (menuNumber == HOLD_MENU_NUMBER) {
      showHold();
    }
  } else if (event == EVENT_SELECT) {
    beepPurple();
    if (menuNumber == MAIN_MENU_NUMBER) {
      if (selectPos == 0) {                        
        event = EVENT_PLAY      ;
      } else if (selectPos == 1) {                                              // selected Change Bet
        event = EVENT_BET;
      } else if (selectPos == 2) {
        event = EVENT_SETTINGS;
      } else if (selectPos == 3) {
        event = EVENT_VIEW_METRICS;
      } else if (selectPos == 4) {
        event = EVENT_RESET;
      } else if (selectPos == 5) {
        event = EVENT_HOLD;
      } else {
        event = EVENT_NONE;
      }
    } else if (menuNumber == BET_MENU_NUMBER) {
      if (selectPos == 0) {
        wagered += increment;
        showWager();
        event = EVENT_NONE;
      } else if (selectPos == 1) {
        wagered -= increment;
        showWager();
        event = EVENT_NONE;
      } else if (selectPos == 2) {
        int arraySize = 0;
        for (int i = 0; i < MENU_SIZE; i++) {
          arraySize += sizeof(mainMenu[i]);  
        }
        memcpy(currentMenu, mainMenu, arraySize);
        elements = MAIN_MENU_ELEMENTS;
        menuNumber = MAIN_MENU_NUMBER;
        event = EVENT_SHOW_MENU;
      }
    } else if (menuNumber == SETTINGS_MENU_NUMBER) {
      if (selectPos == 0) {
        if (STATE_AUTO == (machineState & STATE_AUTO)) {
          ClearBit(machineState, STATE_AUTO);
          SetState(STATE_IDLE); 
          event = EVENT_NONE;
        } else {
          SetState(STATE_AUTO);
          event = EVENT_SPIN;                                                   // to kick off processing in auto mode
        }
      } else if (selectPos == 1) {
        sound = !sound;
        event = EVENT_NONE;
      } else if (selectPos == SETTINGS_BACK_ITEM) {
        int arraySize = 0;
        for (int i = 0; i < MENU_SIZE; i++) {
          arraySize += sizeof(mainMenu[i]);  
        }
        memcpy(currentMenu, mainMenu, arraySize);
        elements = MAIN_MENU_ELEMENTS;
        menuNumber = MAIN_MENU_NUMBER;
        event = EVENT_SHOW_MENU;
      }
    } else if (menuNumber == METRICS_MENU_NUMBER) {
      //if (selectPos == 13) {                                                  // if "Back" was selected
        int arraySize = 0;
        for (int i = 0; i < MENU_SIZE; i++) {
          arraySize += sizeof(mainMenu[i]);  
        }
        memcpy(currentMenu, mainMenu, arraySize);                                     
        elements = MAIN_MENU_ELEMENTS;
        menuNumber = MAIN_MENU_NUMBER;
        event = EVENT_SHOW_MENU;
      //}
    } else if (menuNumber == HOLD_MENU_NUMBER) {
      if (selectPos == 0) {
        storedHold += 1;
        showHold();
        event = EVENT_NONE;
      } else if (selectPos == 1) {
        storedHold -= 1;
        showHold();
        event = EVENT_NONE;
      } else if (selectPos == 2) {
        int arraySize = 0;
        for (int i = 0; i < MENU_SIZE; i++) {
          arraySize += sizeof(mainMenu[i]);  
        }
        memcpy(currentMenu, mainMenu, arraySize);
        elements = MAIN_MENU_ELEMENTS;
        menuNumber = MAIN_MENU_NUMBER;
        event = EVENT_SHOW_MENU;
      }
    }
  } else if (event == EVENT_NAV_UP) {
    beep();
    event = EVENT_NONE;
    LCDMenuUp(currentMenu);
  } else if (event == EVENT_NAV_DOWN) {
    beep();
    event = EVENT_NONE;
    LCDMenuDown(currentMenu);
  } else if (event == EVENT_PLAY) {
    event = EVENT_SPIN;
  } else if (event == EVENT_BET) {
    beep();
    int arraySize = 0;
    for (int i = 0; i < MENU_SIZE; i++) {
      arraySize += sizeof(betMenu[i]);  
    }
    memcpy(currentMenu, betMenu, arraySize);
    elements = BET_MENU_ELEMENTS;
    menuNumber = BET_MENU_NUMBER;
    event = EVENT_SHOW_MENU;
  } else if (event == EVENT_HOLD) {
    beep();
    int arraySize = 0;
    for (int i = 0; i < MENU_SIZE; i++) {
      arraySize += sizeof(holdMenu[i]);  
    }
    memcpy(currentMenu, holdMenu, arraySize);
    elements = HOLD_MENU_ELEMENTS;
    menuNumber = HOLD_MENU_NUMBER;
    event = EVENT_SHOW_MENU;
  } else if (event == EVENT_SETTINGS) {
    beep();
    int arraySize = 0;
    for (int i = 0; i < MENU_SIZE; i++) {
      arraySize += sizeof(settingsMenu[i]);  
    }
    memcpy(currentMenu, settingsMenu, arraySize);
    elements = SETTINGS_MENU_ELEMENTS;
    menuNumber = SETTINGS_MENU_NUMBER;
    event = EVENT_SHOW_MENU;
  } else if (event == EVENT_VIEW_METRICS) {
    beep();
    setupMetricsMenu();                                                         // copy the metrics into the menu itself
    int arraySize = 0;
    for (int i = 0; i < METRICS_MENU_ELEMENTS; i++) {                           // using METRICS_MENU_ELEMENTS instead of MENU_SIZE
      arraySize += sizeof(metricsMenu[i]);                                      // arraySize == 34
    }
    memcpy(currentMenu, metricsMenu, arraySize);                                // corruption introduced here!
    elements = METRICS_MENU_ELEMENTS;
    menuNumber = METRICS_MENU_NUMBER;
    debugStoredMetrics();                                                       // send output to serial monitor
    event = EVENT_SHOW_MENU;
  } else if (event == EVENT_RESET) {
    beep();beep();
    resetEEprom();                                                              // TODO: Add confirmation question YES/NO
    resetAllStoredValues();
    startingCreditBalance = storedCreditBalance;
    zeroAllBalances();
    setupMetricsMenu();
    event = EVENT_NONE;
    beep();
  }
}


