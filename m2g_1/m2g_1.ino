/* Morse2Go 
A communication device for persons with language and movement disabilities.
For more information, please visit -- www.morse2go.org

This device accepts Morse Code input from either one key switch or two keys
and translates the codes onto an LCD screen. Display can also be seen on a
Smartphone or a Computer Monitor. 

The hardware is based on the Arduino Due and Hitachi HD44780 20X4 LCD display.
Morse Code patterns and user defined abbreviations are input from a micro SD card.

Developed jointly by Adaptive Design Associates, NYC (www.adaptivedesign.org), 
Hall of Science Amateur Radio Club (www.hosarc.org),
and Jim Wroten, Technical Consultant (www.jimwroten.com).

This work is licensed 2014 by Jim Wroten ( www.jimwroten.com ) under a Creative 
Commons Attribution-ShareAlike 4.0 International License. For more information 
about this license, see www.creativecommons.org/licenses/by-sa/4.0/ 
-- Basically, you can use this software for any purpose for free, 
as long as you say where you got it and that if you modify it, you don't remove
any lines above THIS line:   -------------------------------------------------

------------------ Release History  ---------------------------

8/28/14 Initial Release 1.0 
  - basic functionality - user input of 1 or 2 button code
  - morse2go code table input from Google docs - support for mcode, scode and pcode
  - user input of timing values. persistent pcode values saved in usr_parm.csv file on Micro SD
  - basic checking of user input parmameters (pcode) upper and lower limits 
  - TODO: advanved editing of user input parmameters (percentage change limit)
  -      user input of short codes

*/

#include <SPI.h>
#include <SD.h>
#include <LiquidCrystal.h>
#include <StopWatch.h>
#include "m2g.cpp"

// LCD -- lines can be moved, but change here too
const int chipSelect = 10;
LiquidCrystal lcd(12, 11, 5, 4, 3, 2);

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

// stopwatches -- used to time switch presses
StopWatch SW[3];

// these can be changed
const int inPin1 = 7;     // button 1 - dit
const int inPin2 = 8;     // button 2 - dah
const int inSwit = 9;     // switch - 1 button or 2 button

// time constants - defined, but overridden by code or user file (in that order)
unsigned ms_do = MSDO;
unsigned ms_da = MSDA;
unsigned ms_lt = MSLT;
unsigned ms_cl = MSCL;

int readingPin1;           // the current readingPin1 from the input pin
int readingPin2;           // the current readingPin2 from the input pin
int readingSwit;           // the current from the switch
int SwitchMode;            // value of 1 or 2 button mode switch

// done once to load classes and setup     
void setup()
{
  char *p;
  int i = 0, j = 0, n = 0;
  int mode;

  // input key setup  
  pinMode(inPin1, INPUT);
  
  // Setup LCD size - send out hello message
  lcd.begin(NCOL, NROW);
  lcd.cursor();
  lcd.setCursor(0, 0);
  lcd.print("Adaptive Design");
  lcd.setCursor(0, 1);
  lcd.print("Assoc. & HOSARC");
  lcd.setCursor(0, 2);
  lcd.print("m2g - morse2go.org");
  lcd.setCursor(0, 3);
  lcd.print("Version 1.0");

  // show the hello message
  delay(2000);
  lcd.clear();
  lcd.home();
  lcd.setCursor(0, 0);

  word_s.clear();
  char_s.clear();
    
  // Open serial communications and wait for port to open:
  Serial.begin(9600);

  // start keyboard emulation
  Keyboard.begin();
  
  // used for LCD
  pinMode(10, OUTPUT);

  // SwitchMode == HIGH, use 2 button mode, LOW is for 1 button mode
  readingSwit = digitalRead(inSwit);
  if (readingSwit == HIGH)
     SwitchMode = HIGH;
  else
     SwitchMode = LOW; 

  // see if the card is present and can be initialized:
  if (!SD.begin(chipSelect)) {
    lcd.println("Card failed, or not present");
    // don't do anything more:
    return;
  }
  
  // load data file from Google Docs copied onto Micro SD
  mode = MODE_MORSE + MODE_SHORT + MODE_PARM; 
  ReadDataFile(CODE, mode);
  
  // load user parameter file 
  mode = MODE_PARM; 
  ReadDataFile(USR_PARM, mode);

  // setup timing variables
  setup_timing();

  // sort the codes
  mcode.sortcode();

}

// repeat this forever, checking for a keypress or key release event
void loop() {
  unsigned long msec_press1, msec_nopress1_char, msec_nopress1_word;  // button time
  int i, j;
  int pos_op; // position of operator (eg, = sign)
  int parm_new; // new value of input parm
  int parm_val; // value of parameter
  static int ctr;
  char buf[40];
  char buf1[40];
  char buf2[40];
  char inp_ch;
  char inp_word[20];
  char buf_word[200];
  char pword[SIZPWORD];
  char smesg[SIZMESG];
  unsigned pval;
  static char pval_str[10];
  static char parm[20];
  static int pval_len;
  static int prevPin1;        // previous value of pin1
  static int prevPin2;        // previous value of pin1
  char c;
  char *p;
  char *pchr;
  char *p_pword;
  int eqop = 61;
  int shown;
  int wordShown;
  int bsdone = 0;
  void outc(int, int, int, char);
  
  if (!ctr) {
    prevPin1 = LOW;  
    prevPin2 = LOW;  
    ctr++;
  }
  
  // initial values
  msec_press1 = msec_nopress1_char = msec_nopress1_word = 0; 
    
  readingPin1 = digitalRead(inPin1);  // value of button 1
  readingPin2 = digitalRead(inPin2);  // value of button 2
 
 // key was pressed 
 if ((readingPin1 == HIGH && prevPin1 == LOW) || (SwitchMode == HIGH && readingPin2 == HIGH && prevPin2 == LOW)) {
     if (readingPin1 == HIGH) 
         prevPin1 = HIGH;
     if (readingPin2 == HIGH) 
         prevPin2 = HIGH;
     shown = 0; // keypress, not shown yet
     wordShown = 0;
     for (i=0; i<3; i++) SW[i].stop(); // stop end of char, end of word
     SW[0].reset(); // reset beg timer
     SW[0].start(); // start beg timer
     SW[2].reset(); // reset end of word
 }
 
 // key 1 was released
 if (readingPin1 == LOW && prevPin1 == HIGH) {
   prevPin1 = LOW;
   bsdone = 0; // allow backspace
   msec_press1 = SW[0].elapsed();  // how long key was pressed?
   for (i=0; i<3; i++) SW[i].stop(); // stop end of char, end of word
   for (i=0; i<3; i++) SW[i].reset(); // stop end of char, end of word
   SW[1].start();
  
  // check if dit(1) or dah(2) was pressed - push result
  if (SwitchMode == LOW && msec_press1 > 50 && msec_press1 < ms_do)
      char_s.push(1);
  else if (SwitchMode == LOW && msec_press1 >= ms_do && msec_press1 < ms_da)
      char_s.push(2);
  else if (SwitchMode == HIGH && msec_press1 >= 50 && msec_press1 < ms_da)
      char_s.push(1);
      
   shown = 0; 
 }

   // key 2 was released
   if (SwitchMode == HIGH && readingPin2 == LOW && prevPin2 == HIGH) {
        prevPin2 = LOW;
        bsdone = 0; // allow backspace
        msec_press1 = SW[0].elapsed();  // how long key was pressed?
        for (i=0; i<3; i++) SW[i].stop(); // stop end of char, end of word
        for (i=0; i<3; i++) SW[i].reset(); // stop end of char, end of word
        SW[1].start();
       
       if (msec_press1 > 10) 
           char_s.push(2);
       shown = 0; 
   }

 // process the backspace key
 if (msec_press1 >= ms_da && msec_press1 < ms_cl && bsdone == 0) {
    prevPin1 = LOW;
    prevPin2 = LOW;
    outc(1, 0, 0, '\b');  // send backspace only to the Keyboard
    lcdbackspace(); // send backspace to the LCD
    bsdone = 1; // backspace done already
    shown = 1;
    for (i=1; i<3; i++) { SW[i].stop(); SW[i].reset(); } // stop and reset end of char, end of word
 
 }
 
  // clear displey - key held > 5 sec
  if (msec_press1 >= ms_cl && bsdone == 0) {
    prevPin1 = LOW;
    prevPin2 = LOW;
    bsdone = 1; // backspace done already
    shown = 1;
    outc(1, 0, 0, '\n');  // send newline to the Keyboard
    lcd.clear();
    lcd.setCursor(0,0);
    word_s.clear();
    char_s.clear(); 
    for (i=1; i<3; i++) { SW[i].stop(); SW[i].reset(); } // stop and reset end of char, end of word
  }
  
   // nothing pressed for 1 sec -- end of letter
   msec_nopress1_char = SW[1].elapsed();

  if (msec_nopress1_char >= ms_da && msec_nopress1_char < ms_lt && shown == 0 && bsdone == 0) {
    int chr = char_s.get_charval();
    int i1 = mcode.getcode(chr, &inp_ch);

    // display on lcd
    lcd_display(inp_ch); 
    
    // display on all devices but LCD
    outc(1, 0, 1, inp_ch);

    // clean up - get ready for next letter
    char_s.clear();
    shown = 1;
    SW[1].stop();  // stop the timer to form a character
    SW[1].reset();
    SW[2].start(); // start the timer to form a word
  }

   // nothing pressed for more than 1 sec -- end of word
   msec_nopress1_char = SW[2].elapsed();

  if (msec_nopress1_char >= ms_lt && wordShown == 0) {
     wordShown = 1;
     outc(1, 0, 1, ' ');
     SW[2].stop();
     SW[2].reset();

     // get previous word entered -- was it a short code? OR was it a parameter code?
     word_s.get_pword(pword);
     
     // save ptr to make it easy to find previous word
     word_s.save_ptr(0);
     
     // push a space onto word stack
     word_s.nextword();

     // pointer to previous word without the slash
     p_pword = pword + 1;
     pos_op = strpos(p_pword, eqop, 2);

    // was this a short code? If so, display message
    if (pword[0] == ':' && strlen(pword) > 1) {
        p = pword + (char) 1;
        int lenmesg = scode.getcode(p, smesg);
        if (lenmesg > 0) {
          for (int j = 0; j < lenmesg; j++)
            outc(1, 0, 1, smesg[j]);
          word_s.push_words(smesg);  // push message onto words stack
          lcd_display(0);     // display 
        }
    }
    // parameter code lookup only - display value
    else if (pword[0] == '/' && strlen(pword) > 1 && pos_op < 0) {
        parm_val = get_parm_code(p_pword, &pval);
        if (parm_val > -1) {
          sprintf(pval_str, "%d", pval);
          pval_len = strlen(pval_str);
          for (int j = 0; j < pval_len; j++)
            outc(1, 0, 1, pval_str[j]);
          word_s.push_words(pval_str);  // push message onto words stack
          lcd_display(0);     // display 
        }
    }
    else if (pword[0] == '/' && strlen(pword) > 1 && pos_op != -1){
          j = chk_parm(p_pword, &parm_val, &parm_new, parm);
          if (j == -1) 
              word_s.push_words("ERR101");  // push message onto words stack
          else if (j == -2) 
              word_s.push_words("ERR102"); 
          else if (j == -3) 
              word_s.push_words("ERR103"); 
          else if (j == -6) 
              word_s.push_words("ERR106"); 
          if (j < 0) 
              lcd_display(0);     // display 
          if (!j) {  // no error, update param and user file
              int j1 = update_parm_code(parm, parm_new);
              if (j1 == -4) {
                  word_s.push_words("ERR104");  // push message onto words stack
                  lcd_display(0);     // display 
              }
              else if (j1 == -5) {
                  word_s.push_words("ERR105");  // push message onto words stack
                  lcd_display(0);     // display 
              }
              else {
                  word_s.push_words("OK");  // push message onto words stack
                  lcd_display(0);     // display 
              }   
          }
       }
   }
}

////////////////////////// End of Loop ///////////////////////////////////////////
// begin functions 

// read datafile
// fn - file name
// mode - bitwise operator to set which functions will be run
// 
void ReadDataFile(char *fn, int mode) {
  int j;
  char buf[BUFSZ];
  char buf_mcode[BUFMCODE]; // buffer for mcode data file
  char buf_scode[BUFSCODE]; // buffer for scode data file
  char buf_pcode[BUFPCODE]; // buffer for pcode data file

  memset(buf, 0, BUFSZ);
  memset(buf_mcode, 0, BUFMCODE);
  memset(buf_scode, 0, BUFSCODE);
  memset(buf_pcode, 0, BUFPCODE);

  File dataFile = SD.open(fn);

  if (dataFile) {
       j = (int)dataFile.size();
       dataFile.read(buf, (uint16_t)j);
       dataFile.close();
    
       if (mode & MODE_MORSE) { 
           filterdata (buf, buf_mcode, "mcode"); 
           mcode.loadcode(buf_mcode);  // load the morse code
       }
       if (mode & MODE_SHORT) { 
           filterdata (buf, buf_scode, "scode"); 
           scode.loadcode(buf_scode);  // load the short code
       }
       if (mode & MODE_PARM) { 
           filterdata (buf, buf_pcode, "pcode");
           pcode.clear(); 
           pcode.loadcode(buf_pcode);  // load the parameter code
       }  
  }
  // if the file isn't open, pop up an error:
  else {
      sprintf(buf, "Error opening %s", fn);
      lcd.print(buf);
  }
}

// output to Keyboard, LCD or Serial Terminal
void outc(int kb, int lc, int ser, char c) {
  if (kb) Keyboard.print(c);
  if (lc) lcd_display(c);  
  if (ser) Serial.print(c);
}

// send backspace to the LCD
// this is done by clearing the last character,
// clearing the LCD and resending the data
void lcdbackspace() {
  word_s.pop(); // - pop the last entered character from the word stack
  lcd_display(0);  // update the lcd
}


// send output to the lcd
// if output is approaching screen size, 
// cut off some of the beginning
//
void lcd_display(char c) {
  static char buf[MAXCHAR];
  char buf1[NROW][NCOL];
  char *p;
  int i, i1, j, nr, lenw, lenbuf;

  for (i = 0; i < NROW; i++)
      memset(buf1[i], 0, NCOL); 
     
  memset(buf, 0, MAXCHAR);
  
  // if char input, push to word stack
  if (c > 0) 
    word_s.push(c);
  
  // clear the lcd  
  lcd.begin(NCOL, NROW);
  lcd.cursor();
  lcd.setCursor(0, 0);
 
  word_s.get_words(buf);
  lenbuf = strlen(buf);

  // if buffer is less than 1 line skip wordwrap section entirely
  if (lenbuf < NCOL-1) { 
      lcd.print(buf);
      return;
  }

  // message is getting too long to fit - cut off beginning
  if (lenbuf > (MAXCHAR - 20))
     word_s.trim_words(); 
 
  // format LCD Screen with NCOL and NROW dimensions
  p = strtok(buf, " ");
  nr = 0; 
  while (p) {
     lenw = strlen(p);
     lenbuf = strlen(buf1[nr]);
     if ((lenbuf + lenw) > NCOL - 2 and nr < NROW) 
         nr++;
         
     if (lenbuf == 0)
         strcpy(buf1[nr], p); 
     else
         strcat(buf1[nr], p);
     strcat(buf1[nr], " "); 
     p = strtok(NULL, " ");  
  }
  
  nr = lenbuf = -1; 
  for (i = 0; i < NROW; i++) {
      lcd.setCursor(0, i);   // column, row order
      lcd.print(buf1[i]);
      j = strlen(buf1[i]);
      if (j) {
          lenbuf = j;
          nr = i;
      }
  }
  lcd.setCursor(lenbuf-1, nr); 
  
}

int filterdata(char* buf, char* codebuf, char* codetag){
  char *p;
  int codelen, i;
  static char buf1[BUFSZ];
  char buf2[200];
  int lineno = 0;
 
  memset(buf1, 0, BUFSZ);
  memset(codebuf, 0, BUFMCODE); 
  strcpy(buf1, buf); 
  codelen = strlen(codetag);
  
  p = strtok(buf1, "\n"); 
  while(p != NULL) {
    if (!strncmp(codetag, p, codelen)) {
        sprintf(buf2, "%s>>", p); 
        if (!lineno) 
	  strcpy(codebuf, buf2);
        else
          strcat(codebuf, buf2);
       lineno++;
     }
	p = strtok(NULL, "\n");
  }
  return lineno;
}

void setup_timing() {
  int rc;
  unsigned val;
  char dot[3] = "DO";
  char da[3] = "DA";
  char lt[3] = "LT";
  char cl[3] = "CL";
  
  // lookup code file - override defaults if found
  rc = pcode.getcode(dot, &val);
  if (rc > -1) ms_do = val;

  rc = pcode.getcode(da, &val);
  if (rc > -1) ms_da = val;

  rc = pcode.getcode(lt, &val);
  if (rc > -1) ms_lt = val;

  rc = pcode.getcode(cl, &val);
  if (rc > -1) ms_cl = val;

}

// lookup current value of parm code
int get_parm_code(char *p, unsigned *v) {
  
  int rc = -1;
  
  if (!strcmp(p, "DO"))
    rc = ms_do;
  else if (!strcmp(p, "DA"))
    rc = ms_da;
  else if (!strcmp(p, "LI"))
    rc = ms_lt;
  else if (!strcmp(p, "CL"))
    rc = ms_cl;
  
  *v = rc;
  return rc;
}

// update current value of parm code
int update_parm_code(char *p, unsigned v) {
  
  int i;
  int rc = 0;
  char buf[300];
  char buf1[50];
  
  if (!strcmp(p, "DO"))
    ms_do = v;
  else if (!strcmp(p, "DA"))
    ms_da = v;
  else if (!strcmp(p, "LI"))
    ms_lt = v;
  else if (!strcmp(p, "CL"))
    ms_cl = v;
  else
    rc = -4; 
  
  // update user parm file
  if (!rc) {
    memset(buf, 0, 300);
    SD.remove(USR_PARM); // delete file in case it exists
    File dataFile = SD.open(USR_PARM, FILE_WRITE); 
      if (dataFile) {
         sprintf(buf1, "pcode,/do,%d,dot time in ms\n", ms_do);
         strcpy(buf, buf1);  
         sprintf(buf1, "pcode,/da,%d,dash time in ms\n", ms_da);  
         strcat(buf, buf1);  
         sprintf(buf1, "pcode,/lt,%d,next letter timeout in ms\n", ms_lt);  
         strcat(buf, buf1);  
         sprintf(buf1, "pcode,/cl,%d,clear screen in ms\n", ms_cl);  
         strcat(buf, buf1);  
         dataFile.print(buf);
         dataFile.close();
      }
      else
        rc = -5;
  }
  return rc;
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

// check input parm - is it valid?
// return -1 if non-numeric value is found
// return -2 if value is out of range
// return -3 if value is changed too much
// return value if OK
int chk_parm(char *p_pword, int *Parm_val, int *Parm_new, char *parm){
    char buf[40];
    char val[20];
    char *p;
    int i, i1, j, rc;
    unsigned prev_val;
    
    rc = 0;
    *Parm_new = 0;
    strcpy(buf, p_pword);
    p = strtok(buf, "=");
    strcpy(parm, p);
    p = strtok(NULL, "=");
    strcpy(val, p);
    get_parm_code(parm, &prev_val);
        
    j = strlen(val);
    if (!j)
      rc = -6;
      
    for (i = 0, i1=0; i< j; i++) {
        if (isdigit(val[i]));
          i1++;
    }
    if (!rc && i1 != j)
      rc = -1;
    
    if (!rc) {
        i = atol(val); 
        if (i < 50 || i > 10000)
           rc = -2; 
        else
            *Parm_new = i;
    }
    
      // TODO do more editing here - percent checks -- rc = -3 
    
    return rc;
}


