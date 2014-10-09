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

9/6/14 Initial Release 1.0 
  - basic functionality - user input of 1 or 2 button code
  - morse2go code table input from Google docs - support for mcode, scode and pcode
  - user input of timing values. persistent pcode values saved in usr_parm.csv file on Micro SD
  - basic checking of user input parmameters (pcode) upper and lower limits, 10% max change limits 
  - /U undo - removes last timing parameter entered
  - /D delete - deletes the usr_parm.csv file and reboots. This brings in factory default timings
  - /L list - list current timing paramters 
  - TODO: user input of short codes
  -      user input of short codes

10/1/13 Release 2.0


*/

#include <Adafruit_GFX.h>    // Core graphics library
#include "Adafruit_ILI9341.h" // Hardware-specific library
#include <SPI.h>
#include <SD.h>
#include <StopWatch.h>
#include "m2g.cpp"

#define TFT_DC 9
#define TFT_CS 10
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC);


// MicroSD
const int chipSelect = 4;

int resetPin = 6;  // reset pin - 1k resistor goes to RESET pin

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
// SW[0] - intercharacter time
// SW[1] - interword time
StopWatch SW[2];

// button timer variables - triggered by ISR
volatile long btn1_time;
volatile byte btn1_evnt = 0;
volatile byte btn1_state = HIGH;

volatile long btn2_time;
volatile byte btn2_evnt = 0;
volatile byte btn2_state = HIGH;

// these can be changed
const int inPin1 = 20;     // button 1 - dit
const int inPin2 = 21;     // button 2 - dah
const int inSwit = 49;     // switch - 1 button or 2 button

// time constants - defined, but overridden by code or user file (in that order)
unsigned ms_do = MSDO;
unsigned ms_da = MSDA;
unsigned ms_lt = MSLT;
unsigned ms_cl = MSCL;

int readingPin1;           // the current readingPin1 from the input pin
int readingPin2;           // the current readingPin2 from the input pin
int readingSwit;           // the current from the switch
int SwitchMode;            // value of 1 or 2 button mode switch
int loopctr = 0;

// done once to load classes and setup     
void setup()
{
  char *p;
  int i = 0, j = 0, n = 0, k;
  int mode;

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
  Serial1.flush();                 // Flush the receive buffer

  // input keys setup  
  pinMode(inPin1, INPUT);
  pinMode(inPin2, INPUT);
  pinMode(inSwit, INPUT);

 digitalWrite(inPin1, HIGH);
 digitalWrite(inPin2, HIGH);
 digitalWrite(inSwit, HIGH);
 
 // Interrupt for button presses
 attachInterrupt(3, ckbtn1, CHANGE);  // button 1  
 attachInterrupt(2, ckbtn2, CHANGE);  // button 2
   
  // setup TFT screen 
  tft.begin();
  tft.fillScreen(0xFFFF);
  tft.setCursor(0, 0);
  tft.setTextColor(ILI9341_BLACK);  
  tft.setTextSize(2);
  tft.setRotation(1);
  tft.print("tft starting"); 

  // for Micro SD
  pinMode(chipSelect, OUTPUT);

  // see if the card is present and can be initialized:
  if (!SD.begin(chipSelect)) {
    tft.println(F("Card failed, or not present"));
    // don't do anything more:
    return;
  }

  // initialize reset
  //digitalWrite(resetPin, HIGH);
  //delay(20);
  //pinMode(resetPin, OUTPUT);
  
  //k = helloFile(2);
  k = 0;
  
  // Setup LCD size - send out hello message
  if (k == 0) {
    tft.setCursor(0, 0);
    tft.fillScreen(0xFFFF);
    tft.print(F("Adaptive Design"));
    tft.setCursor(0, 20);
    tft.print(F("Assoc. & HOSARC"));
    tft.setCursor(0, 40);
    tft.print(F("m2g - morse2go.org"));
    tft.setCursor(0, 60);
    tft.print(F("Version 2.0"));
    Serial1.print("S M 2 G Version 2\n"); 

    // show the hello message
    delay(2000);
    k = 1;
  }

  //k = helloFile(0); // delete for next boot

  tft.fillScreen(0xFFFF);
  tft.setCursor(0, 0);
  tft.print(F("OK>"));

  word_s.clear();
  char_s.clear();
    
  // start keyboard emulation
  //Keyboard.begin();
  
  // SwitchMode == HIGH, use 1 button mode, LOW is for 2 button mode
  readingSwit = digitalRead(inSwit);
  if (readingSwit == LOW)
     SwitchMode = LOW;
  else
     SwitchMode = HIGH; 

  // load data file from Google Docs copied onto Micro SD
  ReadDataFile(CODE);
  
  // load user parameter file if it exists
  ReadParmFile(USR_PARM);

  // setup timing variables
  setup_timing();

  // sort the codes
  mcode.sortcode();

}

// repeat this forever, checking for a keypress or key release event
void loop() {
  unsigned long btn_open_char;
  unsigned long btn_open_word;
  long chr;
  int i, j, k;
  int pos_op; // position of operator (eg, = sign)
  int parm_new; // new value of input parm
  char inp_ch;
  char pword[SIZPWORD];
  char smesg[MAXSCODE_TXT];
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
  int shown = 0;
  int wordShown = 0;
  int bsdone = 0;
  int lenmesg;
  int speakit;
  char speaktxt[MAXWORD_TXT];
  int speaklen;  
  void outc(int, int, int, char);
  
  // flag to speak message
  speakit = 0; 
  
  // startup with switches open
  if (!loopctr) {
    prevPin1 = HIGH;  
    prevPin2 = HIGH;  
    loopctr=1;
  }
  
  if (SwitchMode == HIGH) { // one button mode
    
    if (btn1_evnt) { // button 1 was pressed and released
      
       // key press was a dot
       if (btn1_time > 50 && btn1_time < ms_do){
         char_s.push(1);
       }
         
       // key press was a dash
       if (btn1_time >= ms_do && btn1_time < ms_da) {
         char_s.push(2);
       }
       
       // key press was clear
       if (btn1_time >= ms_cl) {
         char_s.clear();
         word_s.clear();
         tft.fillScreen(0xFFFF);
         tft.setCursor(0, 0);
         tft.print("OK>");
         SW[0].reset();
         btn1_evnt = 0;
         return;
       }

      // start timer for inter-character, inter-word space
      for (i=0; i< 2; i++) {
        SW[i].stop();     
        SW[i].reset();
        SW[i].start();
      }
      btn1_evnt = 0;
    }
  }
  else if (SwitchMode == LOW) { // two button mode

    if (btn1_evnt) { // button 1 was pressed and released
      if (btn1_time < ms_cl)
        char_s.push(1);

      // key press was clear
      if (btn1_time >= ms_cl){
        char_s.clear();
        word_s.clear();
        tft.fillScreen(0xFFFF);
        tft.setCursor(0, 0);
        tft.print("OK>");
        SW[0].reset();
        btn1_evnt = 0;
        return;
      }

      // start timer for inter-character, inter-word space
      for (i=0; i< 2; i++) {
        SW[i].stop();     
        SW[i].reset();
        SW[i].start();
      }
      btn1_evnt = 0;
    }

    if (btn2_evnt) { // button 2 was pressed and released
      char_s.push(2);
      
      // start timer for inter-character, inter-word space
      for (i=0; i< 2; i++) {
        SW[i].stop();     
        SW[i].reset();
        SW[i].start();
      }
      btn2_evnt = 0;
    }
  }


  btn_open_char = SW[0].elapsed();

  // was previous char complete?
  if (btn_open_char >= ms_da) {

    chr = char_s.get_charval();
      k = mcode.getcode(chr, &inp_ch);
             
      // display on all devices
      outc(1, 1, 1, inp_ch);
          
      // clean up - get ready for next letter
      char_s.clear();
      SW[0].stop();     
      SW[0].reset();
      
  }

  btn_open_word = SW[1].elapsed();

  // was previous word complete
  if (btn_open_word >= ms_lt && !wordShown) {
       
      SW[1].stop();     
      SW[1].reset();
      wordShown = 1;  

     // get previous word entered -- was it a short code? OR was it a parameter code?
     word_s.get_pword(pword);

     // output space 
     outc(1, 1, 1, ' ');

     // previous word without a leading character (like the /). 
     p_pword = pword + 1;
     
     // position of = operator, if any
     pos_op = strpos(p_pword, eqop, 2);

     // check to see if we're going to speak the text
     if (strchr(pword, '.') != NULL || strchr(pword, '?') != NULL && strlen(pword) > 1)
       speakit = 1;

     // todo
     // backspace

     // was this a short code? If so, display message
     if (pword[0] == ':' && strlen(pword) > 1) {
        lenmesg = scode.getcode(p_pword, smesg);
        if (lenmesg > 0) {
           Serial.println(smesg); 
           tft.print(smesg); 
           word_s.push_words(smesg);  // push message onto words stack
           speakit = 1;  // always speak short code message
        }
     }

     
     // List function - list current values of parms
     else if (!strcmp(pword, "/L")) 
        listparm();
     
     // todo
     // undo current parm on stack
     //else if (!strcmp(pword, "/U")) 
        //undoparm();

      // todo 
     // undo current parm on stack
     //else if (!strcmp(pword, "/D")) 
        //delparm();
        
    // todo
    /*    
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
              tft_display(0);     // display 
          if (!j) {  // no error, update param and user file
              int j1 = update_parm_code(parm, parm_new);
              if (j1 == -4) {
                  word_s.push_words("ERR104");  // push message onto words stack
                  tft_display(0);     // display 
              }
              else if (j1 == -5) {
                  word_s.push_words("ERR105");  // push message onto words stack
                  tft_display(0);     // display 
              }
              else {
                  word_s.push_words("OK");  // push message onto words stack
                  tft_display(0);     // display 
              }   
          }
       }
     */ 
     //////////////////////////
     // end of todo list //////   
     //////////////////////////
     
   }
   if (speakit) {
      speaklen = word_s.get_words(speaktxt); 
      if (speaklen) { 
        Serial1.print('S');
        Serial1.print(speaktxt);
        Serial1.print('\n'); 
     }
     speakit = 0; 
   }
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

    while( SD_fgets (str, 100, dataFile)!=NULL ) {

      // remove newline
      if ((p=strchr(str, '\n')) != NULL)
        *p = '\0';
      
      switch(str[0]) {
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

//
// controls hello message for normal startup vs reset
// return 0 -- no hello file -- for normal startup with Hello Message
// return 1 -- file present - use for no hello on reset
// mode 2 read file and return above codes
// mode 1 create file with short message
// mode 0 delete file
int helloFile(int mode) {

  int rc, j;
  File dataFile;
  char buf[100];
  char buf1[300];
  
  memset(buf, 0, 100);
  
  // delete file
  if (mode == 0) {
    SD.remove(HELLO_FILE);
    rc = -1;
  }
  else if (mode == 1) { 
    SD.remove(HELLO_FILE);
    dataFile = SD.open(HELLO_FILE, FILE_WRITE);
    dataFile.print(F("Hello, World!"));
    dataFile.close();
    rc = -1;
  }
  else if (mode == 2) {
    dataFile = SD.open(HELLO_FILE);
    if (dataFile) {
       //j = (int)dataFile.size();
       j = 5;
       dataFile.read(buf, (uint16_t)j);
       dataFile.close();
       rc = 1; // file present
    }
    else
      rc = 0; // no file, normal startup with message
  }
  return rc;
} 

// output to Keyboard, LCD or Serial Terminal
void outc(int kb, int lc, int ser, char c) {
  //if (kb) Keyboard.print(c);
  if (lc) tft_display(c);  
  if (ser) Serial.print(c);
}

// send backspace to the LCD
// this is done by clearing the last character,
// clearing the LCD and resending the data
void tftbackspace() {
  word_s.pop(); // - pop the last entered character from the word stack
  tft_display(0);  // update the lcd
}


// send output to the lcd
// if output is approaching screen size, 
// cut off some of the beginning
//
void tft_display(char c) {
  static char buf[MAXWORD_TXT];
  char buf1[NROW][NCOL];
  char *p;
  int i, i1, j, nr, lenw, lenbuf;

  for (i = 0; i < NROW; i++)
      memset(buf1[i], 0, NCOL); 
     
  memset(buf, 0, MAXWORD_TXT);
  
  // if char input, push to word stack
  if (c > 0) { 
    word_s.push(c);
    tft.print(c); 
    return;
  }
  
  // arg = 0, display the buffer 
 
  // not being used now - rewrite 
  return;
}
/* 
  word_s.get_words(buf);
  lenbuf = strlen(buf);

  // message is getting too long to fit - cut off beginning
  if (lenbuf > (MAXWORD_TXT - 20))
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
      tft.setCursor(0, i*20);   // column, row order
      tft.print(buf1[i]);
      j = strlen(buf1[i]);
      if (j) {
          lenbuf = j;
          nr = i;
      }
  }
  tft.setCursor(lenbuf-1, nr); 
} // above isn't being used now
*/

void setup_timing() {
  int rc;
  unsigned v[4];
  char buf[100];
  
  // lookup code file - override defaults if found
  rc = pcode.getcode_pop(-2, v);

  ms_do = v[0];
  ms_da = v[1];
  ms_lt = v[2];
  ms_cl = v[3];
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
int update_parm_code(char *p, unsigned val) {
  unsigned v[4];
  int rc = 0;
    
  // update value 
  if (!strcmp(p, "DO"))
    ms_do = val;
  else if (!strcmp(p, "DA"))
    ms_da = val;
  else if (!strcmp(p, "LI"))
    ms_lt = val;
  else if (!strcmp(p, "CL"))
    ms_cl = val;
  else
    rc = -4; 
  
  // push new value on stack
  if (!rc) {
    v[0] = ms_do;
    v[1] = ms_da;
    v[2] = ms_lt;
    v[3] = ms_cl;
    pcode.push(v);
    rc = updateUserParmFile();
  }
  
  return rc;
}  

// update user parm file
int updateUserParmFile() {
  int rc = 0, cnt, i, j;
  unsigned v[4];
  char buf[BUFPCODE];
  char buf1[40];

    memset(buf, 0, BUFPCODE);

    cnt = pcode.getcode_pop(-2, v); // get cnt - size of stack
    
    memset(buf, 0, BUFPCODE);
    SD.remove(USR_PARM); // delete file in case it exists
    delay(300);
    File dataFile = SD.open(USR_PARM, FILE_WRITE); 
      if (dataFile) {
         for (i=0; i < cnt; i++) {
            j = pcode.getcode_pop(i, v); // get next element of stack
            sprintf(buf1, "%d,%d,%d,%d\n", v[0], v[1], v[2], v[3]);
            strcat(buf, buf1);
         }
         dataFile.print(buf);
         dataFile.close();
      }
      else
        rc = -5;
 
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
    unsigned prev_val, ival, ival_upper, ival_lower;
    
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
    
    //range check
    if (!rc) {
        ival = atol(val); 
        if (ival < 20 || ival > 10000)
           rc = -2; 
    }
 
    // delta check
    if (!rc) {
      ival_upper = prev_val * 1.10;
      ival_lower = prev_val * .90; 
      if (ival < ival_lower || ival > ival_upper)
          rc = -3;
    }
    
    if (!rc)
      *Parm_new = ival;
    else
      *Parm_new = 0; 
      
    return rc;
}

// /L function
// show current values of timing parms 
int listparm(){
  unsigned v[4];
  char buf0[NCOL];
  char buf1[NCOL];
  int i;
  
  i = pcode.getcode_pop(-2, v);
  sprintf(buf0, "DOT:%d DASH:%d", v[0], v[1]);
  sprintf(buf1, "LTR:%d CLS: %d", v[2], v[3]);

  tft.fillScreen(0xFFFF);
  tft.setCursor(0, 0);
  tft.print(buf0);
  tft.setCursor(0, 20);
  tft.print(buf1);
}

// /U function
// undo last parm entered from top of stack
int undoparm(){
  int i;
  unsigned v[4];
  char buf[50];
  
  i = pcode.getcode_pop(-1, v);
  
  if (i > 0) { // stack has something on it, save it
    updateUserParmFile();
    word_s.push_words("OK");  // push message onto words stack
    tft_display(0);     // display 
  }
  else if (i == 0) { // stack is empty - delete user file
    SD.remove(USR_PARM); // delete file if nothing on stack
    delay(250); 
    i = helloFile(1); // create hello file - no startup message
    delay(250); 
    digitalWrite(resetPin, LOW);
  }
}

// /D function
//  delete user parm file
int delparm(){
  int i;
 
  SD.remove(USR_PARM); // delete file if nothing on stack
  delay(250); 
  i = helloFile(1); // create hello file - no startup message
  delay(250); 
  digitalWrite(resetPin, LOW);
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

// button 1 closed or open
void ckbtn1() {
    static long lastDebounceTime;
    static long btn_closed;
  
    if ((millis() - lastDebounceTime) > DEBOUNCEDELAY) {
    
      if (btn1_state == HIGH) { // was open, has been pressed
          btn_closed = millis();
          btn1_state = LOW;
      }
      else { // was closed, has been released
          btn1_time = millis() - btn_closed;
          btn1_evnt = 1;  // btn released - time held in btn_time 
          btn1_state = HIGH;
      }
    }
    lastDebounceTime = millis();
}

// button 2 closed or open
void ckbtn2() {
    static long lastDebounceTime;
    static long btn_closed;
  
    if ((millis() - lastDebounceTime) > DEBOUNCEDELAY) {
    
      if (btn2_state == HIGH) { // was open, has been pressed
          btn_closed = millis();
          btn2_state = LOW;
      }
      else { // was closed, has been released
          btn2_time = millis() - btn_closed;
          btn2_evnt = 1;  // btn released - time held in btn_time 
          btn2_state = HIGH;
      }
    }      
    lastDebounceTime = millis();
}

