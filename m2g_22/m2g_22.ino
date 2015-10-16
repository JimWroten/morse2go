/* Morse2Go
A communication device for persons with language and movement disabilities.
For more information, please visit -- www.morse2go.org

This device accepts Morse Code input from either one key switch or two keys
and translates the codes onto an LCD screen. Display can also be seen on a
Smartphone or a Computer Monitor.

The hardware is based on the Arduino Mega 2560 and Adafruit 2.8" TFT display.
Morse Code patterns and user defined abbreviations are stored on a micro SD card
so that code modifications can be made without changing program code.

Developed jointly by Adaptive Design Association, NYC (www.adaptivedesign.org),
Hall of Science Amateur Radio Club (www.hosarc.org),
and Jim Wroten, Technical Consultant (www.jimwroten.nyc).

This work is licensed 2014 by Jim Wroten (www.jimwroten.nyc) under a Creative
Commons Attribution-ShareAlike 4.0 International License. For more information
about this license, see www.creativecommons.org/licenses/by-sa/4.0/
-- Basically, you can use this software for any purpose for free,
as long as you say where you got it and that if you modify it, you don't remove
any lines above THIS line:
---------------------------------------------------------------

------------------ Release History  ---------------------------

9/6/14 Initial Release 1.0
  - basic functionality - user input of 1 or 2 button code
  - morse2go code table input from Google docs - support for mcode, scode and pcode
  - user input of timing values. persistent pcode values saved in usr_parm.csv file on Micro SD
  - basic checking of user input parmameters (pcode) upper and lower limits, 10% max change limits
  - /D delete - deletes the usr_parm.csv file and reboots. This brings in factory default timings
  - /L list - list current timing paramters
  - TODO: user input of short codes

10/1/14 Release 2.0
  - moved to Arduino Mega 2560 platform with Adafruit 2.8" TFT LCD shield and Micro SD card - reduces assembly complexity
  - added EMIC 2 text-to-speech module
  - construction note: Using the Adafruit 2.8" TFT with the Mega 2560 requires soldering closed the three ICSP jumpers,
  near the 6 pin jack. See: https://learn.adafruit.com/adafruit-2-8-tft-touch-shield-v2/connecting - search "Using with a Mega".
  It isn't necessary to cut the small line in between the #11, #12, #13 pads.

4/1/15 Release 2.1
  - Simplified user modified parameters - only saving the last change to parameters, which overrides the Google Doc parameters (pcodes)
  - Added pcodes /vc (voice)
  - removed one button operation - use button 1 for dit, button 2 for dah
  - added enter key and delete key (buttons 3 and 4)
  -   enter key - press to accept last entry in buffer. If there is no last entry, insert space, third space - say it
  -   delete key - press to delete current buffer. If buffer is empty, delete last character on screen, third delete clear screen

10/10/15 Release 2.s
  - Two Button and Four Button modes - a long press of button 1 is the same as a button 3 press. Long press of button 2 
    is the same as button 4. 
  - Added codes for space (..--), backspace (----), speak (.---.), and clear (.._..). These are defined on code.csv
  - Formatted the TFT screen when words wrap

*/

#include <Adafruit_GFX.h>    // Core graphics library
#include "Adafruit_ILI9341.h" // 2.8" touch screen
#include <SPI.h>
#include <SD.h>
#include <StopWatch.h>
#include <EEPROM.h>
#include "m2g.cpp"

#define TFT_DC 9
#define TFT_CS 10
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC);

// MicroSD
const int chipSelect = 4;
#define SD_CS 4

// data structure of morse code
mcodes mcode;

// data structure of short code
scodes scode;

// data structure of parameter code
pcodes pcode;

// data structure of a single letter
char_stk char_s;

// data structure of word stack
word_stk word_s;

// stopwatch -- used to time switch press
StopWatch SW;

// length of a long press (minimum in milliseconds)
unsigned long LongPress;

// flag that is set if long press done
short LongPress1, LongPress2, LongPress3;

// pin number can be changed
const int inPin1 = 41;     // button 1 - dit
int buttonState1 = HIGH;  // button state - dit
int lastButtonState1 = HIGH;
long lastDebounceTime1 = 0;  // the last time pin 1 was toggled
int readingPin1;  // current reading switch 1

// pin number can be changed
const int inPin2 = 43;     // button 2 - dah
int buttonState2 = HIGH;  // button state - dah
int lastButtonState2 = HIGH;
long lastDebounceTime2 = 0;  // the last time pin 2 was toggled
int readingPin2;  // current reading switch 2

// pin number can be changed
const int inPin3 = 47;     // button 3 - enter
int buttonState3 = HIGH;  // button state - enter
int lastButtonState3 = HIGH;
long lastDebounceTime3 = 0;  // the last time pin 3 was toggled
int readingPin3;  // current reading switch 3
short timesPressed3 = 0; // times enter key pressed in a row

// pin number can be changed
const int inPin4 = 49;     // button 4 - delete
int buttonState4 = HIGH;  // button state - delete
int lastButtonState4 = HIGH;
long lastDebounceTime4 = 0;  // the last time pin 3 was toggled
int readingPin4;  // current reading switch 4
short timesPressed4 = 0; // times delete key pressed in a row

const long debounceDelay = DEBOUNCEDELAY;     // delay in milliseconds

// voice param
int pr_vc = PRVC;
int pr_fn = PRFN;

// EEPROM data
int eeAdr = 0;

int inp_ch = -1;

// done once to load classes and setup
void setup()
{
  char *p;
  int i = 0, j = 0, n = 0, k, r, c;
  int mode;
  char buf[20];
  int v[NPARMS];

  // Open serial communications and wait for port to open:
  Serial.begin(9600);

  // Open serial communications for EMIC 2 Text to Speech module
  // Pin18 is TX pin - connect to SIN on EMIC 2
  // Pin19 is RX pin - connect to SOUT on EMIC 2
  // NOTE: This code is specific to Arduino Mega

  Serial1.begin(9600);
  Serial1.print('\n');             // Send a CR in case the system is already up
  delay(20);
  while (Serial1.read() != ':');   // When the Emic 2 has initialized and is ready, it will send a single ':' character, so wait here until we receive it
  sprintf(buf, "N%d\n", pr_vc);
  Serial1.print(buf);
  Serial1.flush();                 // Flush the receive buffer

  // read EEPROM data
  EEPromData EEp;
  EEPROM.get(eeAdr, EEp);
  LongPress3 = 0;

  // initialize first time - set to reasonable default values
  if (EEp.msLongPress < LONGPRESS || EEp.msLongPress > 3000) {
    EEp.msLongPress = LONGPRESS;
    memset(EEp.filler, 0, sizeof(EEp.filler));
    EEPROM.put(eeAdr, EEp);
  }
  LongPress = EEp.msLongPress;
  sprintf(buf, "LongPress : %u\n", LongPress);
  Serial.println(buf);

  // input keys setup
  pinMode(inPin1, INPUT);
  pinMode(inPin2, INPUT);
  pinMode(inPin3, INPUT);
  pinMode(inPin4, INPUT);

  digitalWrite(inPin1, HIGH);
  digitalWrite(inPin2, HIGH);
  digitalWrite(inPin3, HIGH);
  digitalWrite(inPin4, HIGH);

  // for Micro SD
  pinMode(chipSelect, OUTPUT);

  // see if the card is present and can be initialized:
  if (!SD.begin(chipSelect)) {
    tft.println(F("Card failed, or not present"));
    // don't do anything more:
    return;
  }

  // load data file from Google Docs copied onto Micro SD
  ReadDataFile(CODE);

  // load user parameter file if it exists
  ReadParmFile(USR_PARM);

  // setup timing variables
  userparms();

  // sort the codes
  mcode.sortcode();

  // set the voice for the EMIC 2
  setvoice(pr_vc);

  // setup TFT screen
  tft.begin();
  tft.setTextColor(ILI9341_BLACK);
  tft.setTextSize(2);  // font param

  tft.setRotation(1);
  bmpDraw("m2g.bmp", 0, 0);
  delay(2000);
  tft.fillScreen(0xFFFF);
  setcursor(1, -1, 4, 4, &c, &r);
  tft.setTextColor(ILI9341_BLACK);
  tft.print(F("M2G Version 2.2"));
  delay(1000);
  Serial1.print("S M 2 G Version 2.2\n");
  tft.setTextSize(pr_fn);  // font param
  cls(0);
  show_labels(0, 0);
}

// repeat this forever, checking for a keypress or key release event
void loop() {

  char buf[MAXWORD_TXT + 20];
  unsigned long timebtn1, timebtn2, timebtn3, timebtn4;
  int Btn1, Btn2, Btn3, Btn4, PushCode, clen, lenmesg, CurRow1, CurRow2, ptr, isedge, ReFmt;
  int speakit, speaklen, new_word, csize, lenpword;
  char speaktxt[MAXWORD_TXT];
  char pword[SIZPWORD];
  char smesg[SIZMESG];
  char *p;
  static int PrvRow;

  // check button status
  readingPin1 = digitalRead(inPin1);
  readingPin2 = digitalRead(inPin2);
  readingPin3 = digitalRead(inPin3);
  readingPin4 = digitalRead(inPin4);

  Btn1 = Btn2 = Btn3 = Btn4 = 0;
  PushCode = 0;
  LongPress1 = LongPress2 = 0;

  //  Button 1 - 4 Pressed
  if ((readingPin1 == LOW && lastButtonState1 == HIGH) ||
      (readingPin2 == LOW && lastButtonState2 == HIGH) ||
      (readingPin3 == LOW && lastButtonState3 == HIGH) ||
      (readingPin4 == LOW && lastButtonState4 == HIGH)) {
    SW.reset(); // reset timer
    SW.start(); // start timer
  }

  // button 1 Released
  if (readingPin1 == HIGH && lastButtonState1 == LOW) {
    timebtn1 = SW.elapsed();

    if (timebtn1 > DEBOUNCEDELAY) {
      SW.stop();
      if (timebtn1 > LongPress)
        LongPress1 = 1;
      else {
        PushCode = 1;  // dot
        Btn1++;
      }
    }
    LongPress3 = 0;
  }

  // button 2 Released
  if (readingPin2 == HIGH && lastButtonState2 == LOW) {
    timebtn2 = SW.elapsed();

    if (timebtn2 > DEBOUNCEDELAY) {
      SW.stop();
      if (timebtn2 > LongPress)
        LongPress2 = 1;
      else {
        PushCode = 2;  // dash
        Btn2++;
      }
    }
    LongPress3 = 0;
  }

  // button 3 Released
  if (readingPin3 == HIGH && lastButtonState3 == LOW) {
    timebtn3 = SW.elapsed();

    if (timebtn3 > DEBOUNCEDELAY) {
      SW.stop();
      if (timebtn3 > LongPress && LongPress3 == 0) {
        LongPress3 = 1;
        //sprintf(buf, "S Long Press is now %d milli seconds. Hold button three for the duration of the long press. Click any key to cancel", LongPress);
        //Serial1.println(buf);
        //Serial1.flush();
      }
      else if (timebtn3 > LongPress && LongPress3 == 1) {
        //sprintf(buf, "S Long Press duration is now %d milliseconds", timebtn3);
        //Serial1.println(buf);
        //Serial1.flush();

        // Change EEprom here

        LongPress3 = 0;
      }
      else {
        Btn3++;
        LongPress3 = 0;
      }
    }

    // to do next
    //UserMsg(0, 5000);

  }

  // button 4 Released
  if (readingPin4 == HIGH && lastButtonState4 == LOW) {
    timebtn4 = SW.elapsed();

    if (timebtn4 > DEBOUNCEDELAY) {
      SW.stop();
      Btn4++;
    }
    LongPress3 = 0;
  }

  // dit or dah pressed - show results at bottom of screen
  if (Btn1 || Btn2) {
    clen = char_s.push(PushCode);
    if (clen <= MAXDD)
      inp_ch = show_cbuf();
    timesPressed3 = 0;
    timesPressed4 = 0;
    show_labels(timesPressed3, timesPressed4);
  }

  if (Btn3 || LongPress1) {
    timesPressed4 = 0;
    if (inp_ch == 'p' or timesPressed3 == 1) { // insert a space
      new_word = 1; // set new word flag
      lenmesg = -1;

      // get previous word entered -- was it a short code? OR was it a parameter code?
      lenpword = word_s.get_pword(pword);

      // debug
      //char buf1[200];
      //sprintf(buf1, "length of pword: %d\n", lenpword);
      //Serial.println(buf1);

      // save ptr to make it easy to find previous word
      //word_s.save_ptr(0);

      // debug
      //int i2 = word_s.get_ptr(-1);
      //int i3 = word_s.get_ptr(0);
      //sprintf(buf1, "ptr %d, prev_ptr %d, pword: %s", i3, i2, pword);
      //Serial.println(buf1);

      // was this a short code? If so, display message
      if (lenpword > 0 && pword[0] == ':') {
        p = pword + (char) 1;
        lenmesg = scode.getcode(p, smesg);
        //debug
        //Serial.println("Here 110");

        if (lenmesg > 0) {
          sprintf(buf, " - %s", smesg);
          outstr(0, 0, 0, buf);
          // redisplay message for word wrapping
          PrvRow = TftReformat();
          //sprintf(buf1, "c. PrvRow: %d", PrvRow);
          //Serial.println(buf1);

        }
      }
      else {
        //debug
        //Serial.println("Here 111");
        //sprintf(buf1, "a. CurRow1: %d, CurRow2: %d, PrvRow: %d", CurRow1, CurRow2, PrvRow);
        //Serial.println(buf1);

        ptr = word_s.get_ptr(0);
        isedge = ptr % NCOL;  

        if (isedge == 0 || isedge == (NCOL-1)) {
             CurRow2 += SIZER;
             word_s.save_prev_fmt(ptr); 
        }
        else
            CurRow2 = outc(1, 1, ' ', 0);
            
        //sprintf(buf1, "b. CurRow1: %d, CurRow2: %d, PrvRow: %d", CurRow1, CurRow2, PrvRow);
        //Serial.println(buf1);

        if ((CurRow1 > PrvRow || CurRow2 > PrvRow))  // if char or space went to next line, reformat text
          PrvRow = TftReformat();
      }
      //debug
      //Serial.println("Here 112");

      timesPressed3 = 2;
    }
    else if (timesPressed3 == 2 || inp_ch == '.' || inp_ch == 's') {  // enter pressed 3 times - speak
      speaklen = word_s.get_words(speaktxt);
      if (speaklen) {
        word_filter(1, speaktxt);
        sprintf(buf, "S%s\n", speaktxt);
        Serial1.print(buf);
      }
      timesPressed3 = 0;
    }
    else if (inp_ch == 'b') {  // special case - backspace
      backspace();
      timesPressed3 = 1;
    }
    else if (inp_ch == 'c') {  // special case - clear screen
      cls(0);
      timesPressed3 = 0;
    }
    else if (inp_ch == -1) {
      timesPressed3 = 0;
    }
    else {
      CurRow1 = outc(1, 1, inp_ch, ReFmt);  // normal case - display character
      timesPressed3 = 1;
    }

    inp_ch = -1;
    char_s.clear();
    clr_buf(3);
    show_labels(timesPressed3, timesPressed4);
  }

  if (Btn4 || LongPress2) {
    timesPressed3 = 1;    // changed 0 to 1 --- debug
    if (timesPressed4 == 0)  // delete character
      timesPressed4 = 1;
    else if (timesPressed4 == 1) {  // del pressed twice - backspace
      backspace();
      timesPressed4 = 2;
    }
    else if (timesPressed4 == 2) {  // del pressed 3 times - clear screen
      cls(0);
      timesPressed4 = 0;
    }
    inp_ch = -1;
    char_s.clear();
    clr_buf(3);
    show_labels(timesPressed3, timesPressed4);
  }

  lastButtonState1 = readingPin1;
  lastButtonState2 = readingPin2;
  lastButtonState3 = readingPin3;
  lastButtonState4 = readingPin4;
}

////////////////////////// End of Loop ///////////////////////////////////////////
// begin functions

// read datafile into classes
// fn - file name
void ReadDataFile(char *fn) {
  char str[100];
  char *p;

  char buf[100];
  sprintf(buf, "file %s", fn);
  Serial.println(buf);

  File dataFile = SD.open(fn);
  if (dataFile) {

    while ( SD_fgets (str, 100, dataFile) != NULL ) {

      // remove newline
      if ((p = strchr(str, '\n')) != NULL)
        * p = '\0';

      switch (str[0]) {
        case 'm':
          mcode.loadcode(str);  // load the morse code
          break;
        case 's':
          scode.loadcode(str);  // load the short code
          break;
        case 'p':
          pcode.loadcode(str);  // load the parameter code
          break;
      }
    }
  }
  dataFile.close();
}

// read the User Parm File for changes in MC input timing
//

void ReadParmFile(char *fn) {
  int j, len;
  char buf_pcode[BUFPCODE]; // buffer for pcode data file

  memset(buf_pcode, 0, BUFPCODE);

  File dataFile = SD.open(fn);

  if (dataFile) {
    j = (int)dataFile.size();
    dataFile.read(buf_pcode, (uint16_t)j);
    dataFile.close();
    pcode.loadparmcode(buf_pcode);
  }
}

// show labels below char buffer
void show_labels(int Lab3, int Lab4) {
  int r, c;

  // clear label line
  setcursor(1, -1, 2, 7, &c, &r);
  tft.fillRect(c, r, 320, 25, ILI9341_WHITE);
  tft.setTextSize(2);

  switch (Lab3) {
    case 0:
      if (inp_ch == -1)
        tft.setTextColor(0xEEEE);
      tft.println("<Enter>");
      break;
    case 1:
      tft.println("<Space>");
      break;
    case 2:
      tft.println("<Say It>");
      break;
  }
  setcursor(1, -1, 10, 7, &c, &r);
  tft.setTextColor(ILI9341_BLACK);

  switch (Lab4) {
    case 0:
      if (char_s.size() < 1)
        tft.setTextColor(0xEEEE);
      tft.println("<Delete>");
      break;
    case 1:
      tft.println("<Backspace>");
      break;
    case 2:
      tft.println("<Clear>");
      break;
  }
  tft.setTextColor(ILI9341_BLACK);
  tft.setTextSize(pr_fn);  // restore font size
}

// show what's in the char buffer on the bottom line
int show_cbuf() {
  long cval;
  char buf[MAXDD + 1], buf1[25], buf2[25], inp_ch, ch1[2];
  int n, i, k, rc, r, c;
  int ch[MAXDD + 1];

  cval = char_s.get_charval(n, ch);
  k = mcode.getcode(cval, &inp_ch);

  memset(buf, 0, MAXDD + 1);
  for (i = 0; i < n; i++) {
    ch1[0] = ch1[1] = 0;
    if (ch[i] == 1)
      ch1[0] = 219; // dot
    else if (ch[i] == 2)
      ch1[0] = 195;  // dash
    else
      ch1[0] = ' ';

    strcat(buf, ch1);
  }
  setcursor(1, -1, 3, 6, &c, &r);
  tft.println(buf);
  if (k > -1) {

    // clear the right side
    clr_buf(2);

    // write the new char
    tft.setTextColor(ILI9341_BLACK);

    // handles the special cases of space, backspace, speak, and CLS
    switch (inp_ch) {
      case 'p':
        strcpy(buf1, "Space");
        break;
      case 'b':
        strcpy(buf1, "Bksp");
        break;
      case 's':
        strcpy(buf1, "Speak");
        break;
      case 'c':
        strcpy(buf1, "CLS");
        break;
      default:
        sprintf(buf1, "%c", inp_ch);
    }
    setcursor(1, -1, 11, 6, &c, &r);
    tft.println(buf1);
    rc = inp_ch;
  }
  else { // char not found - clear space
    clr_buf(2);
    rc = -1;
  }
  return rc;
}

// clear the screen and buffers
// mode 0 - clear buffers and print "OK"
// mode 1 - clear only char buff, no "OK"
void cls(int mode) {
  if (mode == 0) {
    word_s.clear();
    char_s.clear();
    tft.fillScreen(0xFFFF);
    outstr(1, 1, 0, "OK>");
    //word_s.save_ptr(-1);
  }
  else if (mode == 1)
    tft.fillScreen(0xFFFF);
}

// clear buffer area - side=1 left, side=2 right, both=3
void clr_buf(int side) {
  int r, c;

  if (side == 1) { // left
    setcursor(1, -1, 0, 6, &c, &r);
    tft.fillRect(c, r, 199, 25, ILI9341_WHITE);
  }
  else if (side == 2) { // right
    setcursor(1, -1, 11, 6, &c, &r);
    tft.fillRect(c, r, 100, 50, ILI9341_WHITE);
  }
  else {
    setcursor(1, -1, 0, 6, &c, &r);
    tft.fillRect(c, r, 320, 25, ILI9341_WHITE);
  }
}

// output a string to the tft
// lc - display on lcd
// ser - display on serial
// wrap - wrap the message properly
void outstr(int lc, int ser, int wrap, char *str) {
  int i, j, len;
  char c;

  len = strlen(str);
  for (i = 0; i < len; i++) {
    c = str[i];
    outc(lc, ser, c, 0);
  }
}

// backspace on the tft -
// -1 flags to erase last character and pop the stack
void backspace() {
  outc(1, 1, -1, 0);
}

// output to tft or Serial Terminal
int outc(int tft, int ser, char c, int ReFmt) {
  int CurRow;

  CurRow = tft_display(tft, c, ReFmt);
  if (ser) Serial.print(c);
  return CurRow;
}

// send output to the tft
// c is the character to display
// if c == -1, backspace
// manage the cursor underscore _ that leads the text
int tft_display(int Tft, char chr, int ReFmt) {
  int i, i1, j, nr, lenw, lenbuf, ptr;
  //int col, row, c, r;
  static int cursor_c, cursor_r;
  char ch, c1[3], buf[20];

  // setup pointer to cursor position
  ptr = word_s.get_ptr(0);

  // if char input, push to word stack
  if (chr > 0) {
    ptr = word_s.push(chr);
  }
  // if char input and tft enabled
  if (chr > 0 && Tft) {
    if (ptr < MAXWORD_TXT - 1) {
      CursorMgt(0, 0);  // turn off previously set cursor
      tft.setTextColor(ILI9341_BLACK);
      setcursor(1, ptr, 0, 0, &cursor_c, &cursor_r);
      tft.print(chr);
      CursorMgt(1, ptr + 1); // turn on new cursor
    }
  }
  else if (chr < 1 && Tft) { // backspace
    CursorMgt(0, 0);  // turn off previously set cursor
    i = word_s.pop();
    ptr = word_s.get_ptr(0);
    setcursor(1, ptr + 1, 0, 0, &cursor_c, &cursor_r);
    tft.fillRect(cursor_c, cursor_r, 50, 25, ILI9341_WHITE);
    CursorMgt(1, ptr + 1); // turn on cursor
  }
  return cursor_r;
}

// output a string to the tft - don't use word storage
// str is the string to display
// beg is the offset
// wrap > 0 - wrap the message properly
int outstr_direct(char *str, int beg, int wrap) {
  int i, j, len, pos, row;
  char c;
  char buf[200];

  len = strlen(str);
  for (i = 0; i < len; i++) {
    c = str[i];
    pos = i + beg;
    row = tft_display_direct(c, pos, 0);
  }
  tft_display_direct(c, len, 1);

  // debug
  //sprintf(buf, "buf: %s", str);
  //Serial.println(buf);
  return row;
}

// direct display to tft
//
int tft_display_direct(char chr, int pos, int cur) {
  int col, row, i;

  if (!cur) {
    tft.setTextColor(ILI9341_BLACK);
    setcursor(1, pos, 0, 0, &col, &row);
    tft.print(chr);
  }
  else {
    CursorMgt(1, pos + 1); // turn on cursor
  }
  return row;
}

// create and destroy cursor
// c_on: 1 turns on cursor
// c_on: 0 turns off previously set cursor
// pos: position of cursor
void CursorMgt(int c_on, int pos) {
  int c, r;
  static int tftcol, tftrow;

  if (c_on) {
    tft.setTextColor(ILI9341_BLACK);
    setcursor(1, pos, 0, 0, &tftcol, &tftrow);
    tft.print("_");
  }
  else {
    tft.setTextColor(ILI9341_WHITE);
    setcursor(1, -1, -1, -1, &tftcol, &tftrow);
    tft.print("_");
  }
}

// set the tft cursor in the position needed
// if pos is provided, placement is relative to 0, 0 with wrapping as each line is filled
// if row/col provided, placement is absolute
// -1 in pos indicates that row/col placement to be used
// -1 in row indicates that tftcol and tftrow be used for placement
// mode - 0 do nothing but return values in tftcol and tftrow. non-zero move cursor and return values
void setcursor(short mode, int pos, int col, int row, int *tftcol, int *tftrow) {
  short pos1, r, c;
  char buf5[50];

  if (pos != -1) {  // based on pointer - lines can overflow
    pos1 = pos - 1; // subtract one to make zero based
    if (pos1 < NCOL) {
      *tftrow = 0;
      *tftcol = pos1 * SIZEC;
    }
    else {
      r = (short) pos1 / NCOL;
      *tftrow = SIZER * r;
      *tftcol = SIZEC * (pos1 - (r * NCOL));
    }
  }
  else if (row != -1) {
    *tftrow = SIZER * row;
    *tftcol = SIZEC * col;
  }
  if (mode)
    tft.setCursor(*tftcol, *tftrow);
}


void userparms() {
  int rc;
  int v[NPARMS];
  char buf[100];

  // lookup code file - override defaults if found

  rc = pcode.getcode(v);

  // only voice code parm is being used
  pr_vc = v[0];
}

// similar to php function
// used to find = sign in parmameter input from user
int strpos(char *hay, int need, int offset) {
  char *ptr;
  char *hay1;
  int rc;

  hay1 = hay;
  if (offset > 0)
    hay1 += offset;

  ptr = strchr(hay1, need);

  if (ptr)
    rc = ptr - hay1 + offset;
  else
    rc = -1;

  return rc;
}

// filter text
// filter 1 - find "OK>" and delete it
void word_filter(int mode, char *txt) {
  char *p, *p2;
  char txt1[MAXWORD_TXT];
  char txt2[MAXWORD_TXT];
  char buf[100];
  int period[10];
  int nperiod = 0;
  int i, len;

  for (i = 0; i < 10; i++) period[i] = 0;
  nperiod - -1;
  memset(txt1, 0, MAXWORD_TXT);
  memset(txt2, 0, MAXWORD_TXT);
  strcpy(txt1, txt);
  p = txt1;
  len = strlen(txt1);

  // say text since the last period
  if (mode == 2) {
    for (i = 0; i < len; i++) {
      if (txt1[i] == '.') {
        period[nperiod] = i;
        nperiod++;
      }
    }
    if (nperiod > 1)
      p += period[nperiod - 2] + 1;
  }

  strcpy(txt2, p);
  p2 = txt2;

  if (!strncmp(txt2, "OK>", 3))
    p2 += 3;

  strcpy(txt, p2);
}
// SD Card fgets
//
char *SD_fgets(char *str, int sz, File fp)
{
  int ch;
  char *buf = str;

  while (--sz > 0 && (ch = fp.read()) != EOF) {
    if ((*buf++ = ch) == '\n')  /* EOL */
      break;
  }
  *buf = '\0';
  return (ch == EOF && buf == str) ? NULL : str;
}

void setvoice(int v) {
  char buf[20];

  v = 3;
  sprintf(buf, "N%d\n", v);
  Serial1.print(buf);
  Serial1.flush();
}

// message to screen, with delay
// at end of delay, re-display previous screen
void UserMsg(int nMsg, int dlay) {
  char buf[150];
  int len, i;
  char words[MAXWORD_TXT];

  cls(1);

  switch (nMsg) {
    case 0:
      strcpy(buf, "Hello, World");
      outstr_direct(buf, 1, 0);
      break;
  }

  delay(dlay);

  cls(1);

  len = word_s.get_words(words);
  outstr_direct(words, 1, 0);
}

// reformat word stack to wrap properly on the TFT
// add spaces to prevent word splits
int TftReformat() {
  char buf[MAXWORD_TXT + 1];  // copy of the word stack
  char buf1[MAXWORD_TXT + 1];  // another copy of the word stack
  char cp_words[MAXWORD_TXT + 1];  // copy of the word stack
  char buf2[300];
  char *tok, *p;
  char **a = (char **)malloc(MAXWORDS * sizeof(char *));
  char s[2];
  int i, i1, n, len_words, len_nxtword, len_tot_words, len_lines, len_open_space, len_cp_words, len_mod_cp_words_NCOL, lno;
  int prev_fmt, len, row;

  strcpy(s, " ");
  prev_fmt = word_s.save_prev_fmt(0);
  memset(cp_words, 0, MAXWORD_TXT + 1);
  memset(buf1, 0, MAXWORD_TXT + 1);

  // debug
  //sprintf(buf2, "prev_fmt %d\n", prev_fmt);
  //Serial.println(buf2);


  // get text
  word_s.get_words(buf);
  word_s.clear();

  // debug
  //i = strlen(buf);
  //sprintf(buf2, "original: %s, len=%d\n", buf, i);
  //Serial.println(buf2);


  // copy part of words already formatted
  if (prev_fmt) {
    len = strlen(buf) - prev_fmt;
    strncpy(cp_words, buf, prev_fmt);
    p = buf + prev_fmt;
    strncpy(buf1, p, len);
  }
  else
    strcpy(buf1, buf);

  // debug
  //sprintf(buf2, "prev words: %s, new words: %s\n", cp_words, buf1);
  //Serial.println(buf2);

  // get text into array
  tok = strtok(buf1, s);
  n = 0;
  //sprintf(buf2, "a. n: %d, tok: %s\n", n, tok);
  //Serial.println(buf2);

  while (tok != NULL)  {
    len = strlen(tok);
    if (len >= MAXWORD)
      len = MAXWORD - 1;
    a[n] = (char *)malloc(MAXWORD + 1);
    memset(a[n], 0, MAXWORD + 1);
    strncpy(a[n], tok, len);
    tok = strtok(NULL, s);
    //if (n < MAXWORDS)
    n++;
    //sprintf(buf2, "b. n: %d, tok: %s\n", n, tok);
    //Serial.println(buf2);
    if (n >= MAXWORDS)
      break;
  }

  // format text
  for (i = 0; i < n; i++) {
    sprintf(buf2, "i: %d, a[i]: %s\n", i, a[i]);
    Serial.println(buf2);

    len_words = strlen(cp_words); // length of current string
    len_nxtword = strlen(a[i]);     // length of incoming word
    len_tot_words = len_words + len_nxtword + 1;  // total of above, with space
    lno = (int) (len_words / NCOL) + 1; // actual number of lines used (1-based)
    len_lines = lno * NCOL;       // number of possible chars in currently used lines
    len_open_space = len_lines - len_tot_words;      // line space remaining - positive: OK, negative: overflow

    // debug
    //sprintf(buf2, "ln = %d, len_lines = %d, len_open_space = %d -- a[i]: %s\n", lno, len_lines, len_open_space, a[i]);
    //Serial.println(buf2);

    if (len_open_space < 0) {
      len_cp_words = strlen(cp_words);
      len_mod_cp_words_NCOL = len_cp_words % NCOL;

      while (len_mod_cp_words_NCOL)  {
        strcat(cp_words, " ");
        len_cp_words = strlen(cp_words);
        len_mod_cp_words_NCOL = len_cp_words % NCOL;
        // debug
        //sprintf(buf2, "    len_cp_words = %d, len_mod_cp_words_NCOL = %d\n", len_cp_words, len_mod_cp_words_NCOL);
        //Serial.println(buf2);
      }
    }
    else {
      len_cp_words = strlen(cp_words);
      len_mod_cp_words_NCOL = len_cp_words % NCOL;
      // debug
      //sprintf(buf2, "    len_cp_words = %d, len_mod_cp_words_NCOL = %d\n", len_cp_words, len_mod_cp_words_NCOL);
      //Serial.println(buf2);
      if ((len_mod_cp_words_NCOL != 0) && len_cp_words > prev_fmt)
        strcat(cp_words, " ");
    }

    strcat(cp_words, a[i]);
    // debug
    //sprintf(buf2, "cp_words now (i=%d): %s\n", i, cp_words);
    //Serial.println(buf2);

  }

  // free malloc'ed memory
  for (i = 0; i < n; i++)
    free(a[i]);
  free(a);

  // resave text
  strcat(cp_words, " ");
  len_words = strlen(cp_words);
  prev_fmt = word_s.save_prev_fmt(len_words);
  //word_s.save_ptr(0);
  word_s.push_words(cp_words);
  cls(1);
  row = outstr_direct(cp_words, 1, 0);

  // debug
  //sprintf(buf2, "cp_words: %s\n", cp_words);
  //Serial.println(buf2);

  return row;
}

void bmpDraw(char *filename, uint8_t x, uint16_t y) {
  File     bmpFile;
  int      bmpWidth, bmpHeight;   // W+H in pixels
  uint8_t  bmpDepth;              // Bit depth (currently must be 24)
  uint32_t bmpImageoffset;        // Start of image data in file
  uint32_t rowSize;               // Not always = bmpWidth; may have padding
  uint8_t  sdbuffer[3 * BUFFPIXEL]; // pixel buffer (R+G+B per pixel)
  uint8_t  buffidx = sizeof(sdbuffer); // Current position in sdbuffer
  boolean  goodBmp = false;       // Set to true on valid header parse
  boolean  flip    = true;        // BMP is stored bottom-to-top
  int      w, h, row, col;
  uint8_t  r, g, b;
  uint32_t pos = 0, startTime = millis();

  if ((x >= tft.width()) || (y >= tft.height())) return;

  Serial.println();
  Serial.print(F("Loading image '"));
  Serial.print(filename);
  Serial.println('\'');

  // Open requested file on SD card
  if ((bmpFile = SD.open(filename)) == NULL) {
    Serial.print(F("File not found"));
    return;
  }

  // Parse BMP header
  if (read16(bmpFile) == 0x4D42) { // BMP signature
    Serial.print(F("File size: ")); Serial.println(read32(bmpFile));
    (void)read32(bmpFile); // Read & ignore creator bytes
    bmpImageoffset = read32(bmpFile); // Start of image data
    Serial.print(F("Image Offset: ")); Serial.println(bmpImageoffset, DEC);
    // Read DIB header
    Serial.print(F("Header size: ")); Serial.println(read32(bmpFile));
    bmpWidth  = read32(bmpFile);
    bmpHeight = read32(bmpFile);
    if (read16(bmpFile) == 1) { // # planes -- must be '1'
      bmpDepth = read16(bmpFile); // bits per pixel
      Serial.print(F("Bit Depth: ")); Serial.println(bmpDepth);
      if ((bmpDepth == 24) && (read32(bmpFile) == 0)) { // 0 = uncompressed

        goodBmp = true; // Supported BMP format -- proceed!
        Serial.print(F("Image size: "));
        Serial.print(bmpWidth);
        Serial.print('x');
        Serial.println(bmpHeight);

        // BMP rows are padded (if needed) to 4-byte boundary
        rowSize = (bmpWidth * 3 + 3) & ~3;

        // If bmpHeight is negative, image is in top-down order.
        // This is not canon but has been observed in the wild.
        if (bmpHeight < 0) {
          bmpHeight = -bmpHeight;
          flip      = false;
        }

        // Crop area to be loaded
        w = bmpWidth;
        h = bmpHeight;
        if ((x + w - 1) >= tft.width())  w = tft.width()  - x;
        if ((y + h - 1) >= tft.height()) h = tft.height() - y;

        // Set TFT address window to clipped image bounds
        tft.setAddrWindow(x, y, x + w - 1, y + h - 1);

        for (row = 0; row < h; row++) { // For each scanline...

          // Seek to start of scan line.  It might seem labor-
          // intensive to be doing this on every line, but this
          // method covers a lot of gritty details like cropping
          // and scanline padding.  Also, the seek only takes
          // place if the file position actually needs to change
          // (avoids a lot of cluster math in SD library).
          if (flip) // Bitmap is stored bottom-to-top order (normal BMP)
            pos = bmpImageoffset + (bmpHeight - 1 - row) * rowSize;
          else     // Bitmap is stored top-to-bottom
            pos = bmpImageoffset + row * rowSize;
          if (bmpFile.position() != pos) { // Need seek?
            bmpFile.seek(pos);
            buffidx = sizeof(sdbuffer); // Force buffer reload
          }

          for (col = 0; col < w; col++) { // For each pixel...
            // Time to read more pixel data?
            if (buffidx >= sizeof(sdbuffer)) { // Indeed
              bmpFile.read(sdbuffer, sizeof(sdbuffer));
              buffidx = 0; // Set index to beginning
            }

            // Convert pixel from BMP to TFT format, push to display
            b = sdbuffer[buffidx++];
            g = sdbuffer[buffidx++];
            r = sdbuffer[buffidx++];
            tft.pushColor(tft.color565(r, g, b));
          } // end pixel
        } // end scanline
        Serial.print(F("Loaded in "));
        Serial.print(millis() - startTime);
        Serial.println(" ms");
      } // end goodBmp
    }
  }

  bmpFile.close();
  if (!goodBmp) Serial.println(F("BMP format not recognized."));
}

// These read 16- and 32-bit types from the SD card file.
// BMP data is stored little-endian, Arduino is little-endian too.
// May need to reverse subscript order if porting elsewhere.

uint16_t read16(File &f) {
  uint16_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read(); // MSB
  return result;
}

uint32_t read32(File &f) {
  uint32_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read();
  ((uint8_t *)&result)[2] = f.read();
  ((uint8_t *)&result)[3] = f.read(); // MSB
  return result;
}

