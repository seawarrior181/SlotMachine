/*slotCreditsDisplaySlave.ino

  Version:   1.0
  Date:      2018/07/01 - 2018/07/29
  Device:    ATMega328P-PU @ 16mHz
  Language:  C

  Purpose
  =======
 `The .purpose of this program is to function as an I2C slave
  responsible for displaying credits in a slot machine 
  
  Known Defects
  =============  
  - 

  TODO
  ====
  - is 38400 an efficient baud rate for arduino running at 16mhz?
  - include a 100 ohm resistor with the piezo buzzer
  - is 100kHz the fastest setting we can accomodate w/ Wire library?
  
  Warnings
  ========
  - 
  
  Suggestions
  ===========
  - 
  
  Author
  ======
  - Copyright ©2018, Daniel Murphy <dan-murphy@comcast.net>

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

  The Program
  ===========
  - Includes                                                                    */
#include <Wire.h>
//We always have to include the library
#include "LedControl.h"

#define BAUD_RATE           38400                                               
#define CREDITS_SLAVE_ADDR  16                                                  
#define DISPLAY_DELAY       5
#define DEBUG               1

#define BUZZER_DDR        DDRB
#define BUZZER_PORT       PORTB
#define BUZZER_PIN        DDB1
#define TONE_PIN          9                                                     // Pin you have speaker/piezo connected to (be sure to include a 100 ohm resistor).
#define BEEP_LENGTH       100
                                                                                // Now we need a LedControl to work with.
                                                                                // pin 12 is connected to the DataIn 
                                                                                // pin 11 is connected to the CLK 
                                                                                // pin 10 is connected to LOAD 
                                                                                // We have only a single MAX72XX.
LedControl lc=LedControl(12,11,10,1);

static const int slaveAddress = CREDITS_SLAVE_ADDR;  

long volatile theCredits[10] = {0L,0L,0L,0L,0L,0L,0L,0L,0L,0L};

signed long volatile displayedBalance = 0;
signed long volatile startingCreditBalance = 0;
signed long volatile endingCreditBalance;
signed int volatile increment;
boolean volatile updateDisplayFlag = false;

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

void debugMetric(const char myString[], signed int anInt) {
  if (DEBUG) {
    debugNoLF(myString);debugNoLF(": ");
    debugInt(anInt);
    Serial.print("\r\n");
  }
}

void debugMetricLong(const char myString[], signed long aLong) {
  if (DEBUG) {
    debugNoLF(myString);debugNoLF(": ");
    debugLong(aLong);
    Serial.print("\r\n");
  }
}

void beep() {
  BUZZER_PORT |= (1 << BUZZER_PIN);                                               // turn on buzzer
  delay(BEEP_LENGTH);
  BUZZER_PORT &= ~(1 << BUZZER_PIN);                                              // turn off the buzzer
}

void beepLong() {
  BUZZER_PORT |= (1 << BUZZER_PIN);                                               // turn on buzzer
  delay(1000);
  BUZZER_PORT &= ~(1 << BUZZER_PIN);                                              // turn off the buzzer
}

void setup() {     
  Serial.begin(BAUD_RATE);
  debug("setup()");

  BUZZER_DDR |= (1 << BUZZER_PIN);                                                // set buzzer pin to output
  BUZZER_PORT &= ~(1 << BUZZER_PIN);                                              // turn off the buzzer

  /*
   The MAX72XX is in power-saving mode on startup,
   we have to do a wakeup call
   */
  lc.shutdown(0,false);
  /* Set the brightness to a medium values */
  lc.setIntensity(0,8);
  /* and clear the display */
  lc.clearDisplay(0);

  Wire.begin(CREDITS_SLAVE_ADDR);                                                 // join i2c bus with address #32
  TWBR=32;                                                                        // == 100kHz SCL frequency  
  Wire.onReceive(receiveEvent);                                                   // register event

  scrollDigits();
  beepLong();
}

void loop() {
  //debug("loop()");
  if (updateDisplayFlag) {
    updateDisplay();
  }
}

void updateDisplay() {
  debug("updateDisplay()");

  for ( signed long displayBalance = startingCreditBalance; 
        displayBalance != endingCreditBalance; 
        displayBalance += increment) {
    showBalance(displayBalance);
  }
  showBalance(endingCreditBalance);
  beep();
  updateDisplayFlag = false;
}

void showBalance(signed long showBalance) {                                       // display showBalance on the 7-segment LED
  boolean negative = false;
  byte displayDigit = 0;

  if (showBalance < 0) {                                                          // if the balance is negative make it positive
    showBalance *= -1L;                                                           // for display purposes and
    negative = true;                                                              // set the negative flag to true for use next...
  }

  displayDigit = showBalance / 10000000L;                                         // extract the leftmost digit to display, the digit that's in the ten million's place
  if ((negative) && (displayDigit == 0)) {                                        
    lc.setChar(0, 7, '-', false);                                                 // if showBalance was negative display the negative sign here
  } else {
    lc.setDigit(0,7,displayDigit,false);                                          // otherwise just display the digit
  }
  showBalance = showBalance - ((showBalance / 10000000L) * 10000000L);              

  displayDigit = showBalance / 1000000L;                                          // extract the digit to display in the million's place
  lc.setDigit(0,6,displayDigit,false);                                            //      "
  showBalance = showBalance - ((showBalance / 1000000L) * 1000000L);              //      "
     
  displayDigit = showBalance / 100000L;                                           // and so on...
  lc.setDigit(0,5,displayDigit,false);                                            //      "
  showBalance = showBalance - ((showBalance / 100000L) * 100000L);                //      "

  displayDigit = showBalance / 10000L;             
  lc.setDigit(0,4,displayDigit,false);
  showBalance = showBalance - ((showBalance / 10000L) * 10000L);

  displayDigit = showBalance / 1000L;    
  lc.setDigit(0,3,displayDigit,false);
  showBalance = showBalance - ((showBalance / 1000L) * 1000L);

  displayDigit = showBalance / 100L;    
  lc.setDigit(0,2,displayDigit,false);
  showBalance = showBalance - ((showBalance / 100L) * 100L);

  displayDigit = showBalance / 10L;    
  lc.setDigit(0,1,displayDigit,false);
  showBalance = showBalance - ((showBalance / 10L) * 10L);

  displayDigit = showBalance;                                                     // finally, only the last digit to display remains.
  lc.setDigit(0,0,displayDigit,false);

  beep();
}

                                                                                  // This function executes whenever data is received from The 
                                                                                  // master.  The function is registered as an event (see setup()). 
void receiveEvent(int howMany) {
  debug("receiveEvent()");
  int i = 0;
  while (1 <= Wire.available()) { // loop through all
    theCredits[i] = Wire.read();
    i++;
  }
                                                                                  // transfer the array into startingCreditBalance
                                                                                  // Little endian, least significant byte stored first
  startingCreditBalance = theCredits[0];
  startingCreditBalance = startingCreditBalance | (theCredits[1] << 8 );
  startingCreditBalance = startingCreditBalance | (theCredits[2] << 16);
  startingCreditBalance = startingCreditBalance | (theCredits[3] << 24);

  increment = theCredits[4];
  increment = increment | (theCredits[5] << 8);

  char buffer[50];

  endingCreditBalance = theCredits[6];
  endingCreditBalance = endingCreditBalance | (theCredits[7] << 8 );
  endingCreditBalance = endingCreditBalance | (theCredits[8] << 16);  // when theCredits[8] == 255 value isn't appended to endingCreditBalance
  endingCreditBalance = endingCreditBalance | (theCredits[9] << 24);
  updateDisplayFlag = true;
}

void scrollDigits() {
  debug("scrollDigits()");
  for(int i=0;i<13;i++) {
    lc.setDigit(0,7,i,false);
    lc.setDigit(0,6,i+1,false);
    lc.setDigit(0,5,i+2,false);
    lc.setDigit(0,4,i+3,false);
    lc.setDigit(0,3,i,false);
    lc.setDigit(0,2,i+1,false);
    lc.setDigit(0,1,i+2,false);
    lc.setDigit(0,0,i+3,false);
    delay(100);
  }
  lc.clearDisplay(0);
}


