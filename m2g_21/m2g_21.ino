/* Morse2Go 
A communication device for persons with language and movement disabilities.
For more information, please visit -- www.morse2go.org

This device accepts Morse Code input from either one key switch or two keys
and translates the codes onto an LCD screen. Display can also be seen on a
Smartphone or a Computer Monitor. 

The hardware is based on the Arduino Mega 2560 and Adafruit 2.8" TFT display.
Morse Code patterns and user defined abbreviations are input from a micro SD card.

Developed jointly by Adaptive Design Associates, NYC (www.adaptivedesign.org), 
Hall of Science Amateur Radio Club (www.hosarc.org),
and Jim Wroten, Technical Consultant (www.jimwroten.com).

This work is licensed 2014 by Jim Wroten ( www.jimwroten.com ) under a Creative 
Commons Attribution-ShareAlike 4.0 International License. For more information 
about this license, see www.creativecommons.org/licenses/by-sa/4.0/ 
-- Basically, you can use this software for any purpose for free, 
as long as you say where you got it and that if you modify it, you don't remove
any lines above THIS line:   
-------------------------------------------------

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
  - added enter key and delete key
  -   enter key - press to accept last entry in buffer. If there is no last entry, insert space, third space - say it
  -   delete key - press to delete current buffer. If buffer is empty, delete last character on screen, third delete clear screen
  
  
*/

#include <Adafruit_GFX.h>    // Core graphics library
#include "Adafruit_ILI9341.h" // 2.8" touch screen
#include <SPI.h>
#include <SD.h>
#include "m2g.cpp"

#define TFT_DC 9
#define TFT_CS 10
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC);

// MicroSD
const int chipSelect = 4;
#define SD_CS 4

//int resetPin = 6;  // reset pin - 1k resistor goes to RESET pin

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

int inp_ch = -1;

// done once to load classes and setup     
void setup()
{
  char *p;
  int i = 0, j = 0, n = 0, k, r, c;
  int mode;
  char buf[10];
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
   tft.print(F("M2G Version 2.1"));
   delay(2000);
   Serial1.print("S M 2 G Version 2.1\n"); 
   tft.setTextSize(pr_fn);  // font param
   cls();
   show_labels(0, 0); 
}

// repeat this forever, checking for a keypress or key release event
void loop() {
  unsigned long btn_open_char;
  unsigned long btn_open_word;
  char buf[20];
  long chr;
  int i, j, j1, k, bksp, clen, csize;
  int readingPin3;
  int readingPin4;
  int pos_op; // position of operator (eg, = sign)
  int parm_new; // new value of input parm
  int parm_val;
  char pword[SIZPWORD];
  char smesg[MAXSCODE_TXT];
  char word1[MAXWORD_TXT];
  unsigned pval;
  long mstime;
  static char pval_str[10];
  static char parm[20];
  static int pval_len;
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
  int new_word;  
  void outc(int, int, char);
  
  // flag to speak message
  speakit = 0;
  bksp = 0;
  mstime = millis();
  
  // set new word to no
  new_word = 0; 

 // dit Button (#1) Pressed 
  readingPin1 = digitalRead(inPin1);    
  if (readingPin1 != lastButtonState1) {
    lastDebounceTime1 = mstime;    
  }
  if ((mstime - lastDebounceTime1) > debounceDelay) {
    if (readingPin1 != buttonState1) {
        buttonState1 = readingPin1;

        // dit pressed
        if (buttonState1 == LOW) {

          clen = char_s.push(1);
          if (clen <= MAXDD)
             inp_ch = show_cbuf();
          timesPressed3 = 0;
          timesPressed4 = 0;
          show_labels(timesPressed3, timesPressed4); 
        }
    }
  }

 // dah Button (#2) Pressed 
  readingPin2 = digitalRead(inPin2);    
  if (readingPin2 != lastButtonState2) {
    lastDebounceTime2 = mstime;
  }
  if ((mstime - lastDebounceTime2) > debounceDelay) {
    if (readingPin2 != buttonState2) {
        buttonState2 = readingPin2;

        // dah pressed
        if (buttonState2 == LOW) {

           clen = char_s.push(2);   
           if (clen <= MAXDD)
              inp_ch = show_cbuf();
           timesPressed3 = 0;
           timesPressed4 = 0;
           show_labels(timesPressed3, timesPressed4);
        }
      } 
    }

 // Enter Button (#3) Pressed 
  readingPin3 = digitalRead(inPin3);    
  if (readingPin3 != lastButtonState3) {
    lastDebounceTime3 = mstime;
  }
  if ((mstime - lastDebounceTime3) > debounceDelay) {
    if (readingPin3 != buttonState3) {
        buttonState3 = readingPin3;

        // Enter Button Pressed - display
        if (buttonState3 == LOW) {
          
            timesPressed4 = 0;
            timesPressed3++;
            if (inp_ch != -1) {  // character has been constructed
               outc(1, 1, inp_ch);
               if (inp_ch == '.') // speak it 
                  speakit = 1;
               inp_ch = -1;
               char_s.clear();
               clr_buf(1);      
               clr_buf(2);
            }
            else if (timesPressed3 == 2) {  // buffer is empty -  insert a space
                new_word = 1; // set new word flag
                csize = char_s.size();
                if (csize == 0) 
                    outc(1, 1, ' ');
            }
            else if (timesPressed3 == 3) {  // enter pressed 3 times - speak
                speaklen = word_s.get_words(speaktxt); 
                if (speaklen) { 
                    word_filter(1, speaktxt); 
                    Serial1.print('S');
                    Serial1.print(speaktxt);
                    Serial1.print('\n'); 
                }
                timesPressed3 = 0; 
            }
            show_labels(timesPressed3, timesPressed4); 
         }
      }
   }

  // Delete Button (#4) Pressed 
  readingPin4 = digitalRead(inPin4);    
  if (readingPin4 != lastButtonState4) {
    lastDebounceTime4 = mstime;
  }
  if ((mstime - lastDebounceTime4) > debounceDelay) {
    if (readingPin4 != buttonState4) {
        buttonState4 = readingPin4;

        // Enter Button Pressed - display
        if (buttonState4 == LOW) {
          
            timesPressed3 = 0;
            timesPressed4++;
            csize = char_s.size();
            if (csize > 0) {  // character has been constructed
               inp_ch = -1;
               char_s.clear();
               clr_buf(3);      
            }
            else if (timesPressed4 == 2) {  // del pressed twice - backspace
                backspace();
            }
            else if (timesPressed4 == 3) {  // del pressed 3 times - clear screen
                cls();
                timesPressed4 = 0;               
            }
            show_labels(timesPressed3, timesPressed4); 
         }
      }
   }

     // get previous word entered -- was it a short code? OR was it a parameter code?
    if (new_word) {
       word_s.get_pword(pword);
       p_pword = pword + 1;
       pos_op = strpos(p_pword, eqop, 2);
 
       // save pointer
       word_s.save_ptr(-1);

       // was this a short code? If so, display message
       if (pword[0] == ':' && strlen(pword) > 1) {
          lenmesg = scode.getcode(p_pword, smesg);
          if (lenmesg > 0)
             outstr(1, 1, smesg);
       }
       // List function - list current values of parms
       else if (!strcmp(pword, "/L"))
          listparm();
          
       // show Voice Parm
       else if (!strcmp(pword, "/VC")) 
          show_parm(0);

       // delete user parm file
       else if (!strcmp(pword, "/D")) 
          delparm();
          
       else if (pword[0] == '/' && strlen(pword) > 1 && pos_op != -1){
          j = chk_parm(p_pword, &parm_val, &parm_new, parm);
          if (j) {
              sprintf(buf, " ERR10%d ", j); 
              outstr(1, 1, buf); 
          }
          else 
              j1 = update_parm_code(parm, parm_new);
              if (j1 == 4)
                  outstr(1, 1, " ERR104 ");
       }
    }          
    if (speakit) {
       speaklen = word_s.get_words(speaktxt);
       if (speaklen) {
         word_filter(2, speaktxt); 
         Serial1.print('S');
         Serial1.print(speaktxt);
         Serial1.print('\n'); 
       }
       speakit = 0; 
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
int show_cbuf(){
  long cval;
  char buf[MAXDD+1], buf1[MAXDD+1], buf2[25], inp_ch, ch1[2];
  int n, i, k, rc, r, c; 
  int ch[MAXDD+1]; 

  cval = char_s.get_charval(n, ch); 
  k = mcode.getcode(cval, &inp_ch);
 
  memset(buf, 0, MAXDD+1);
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
     sprintf(buf1, "%c", inp_ch);
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
void cls() {
     word_s.clear();
     char_s.clear();
     tft.fillScreen(0xFFFF);
     outstr(1, 2, "OK>");
     word_s.save_ptr(-1);
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
        tft.fillRect(c, r, 30, 50, ILI9341_WHITE); 
    }
    else {
        setcursor(1, -1, 0, 6, &c, &r);  
        tft.fillRect(c, r, 320, 25, ILI9341_WHITE); 
    }    
}

// output a string to the tft

void outstr(int lc, int ser, char *str) {
    int i, j, len;
    char c; 
    
    len = strlen(str);
    for (i=0; i < len; i++) {
         c = str[i];
         outc(1, 1, c);     
    }
}

// backspace on the tft -
// -1 flags to erase last character and pop the stack
void backspace() {
    outc(1, 1, -1);   
}
  
// output to Keyboard, LCD or Serial Terminal
void outc(int lc, int ser, char c) {
  if (lc) tft_display(c);  
  if (ser) Serial.print(c);
}

// send output to the tft
// c is the character to display
// if c == -1, backspace
// manage the cursor underscore _ that leads the text
void tft_display(char chr) {
  int i, i1, j, nr, lenw, lenbuf, ptr;
  int col, row, c, r;
  static int cursor_c, cursor_r;
  char ch, c1[3], buf[20]; 
  
  // setup pointer to cursor position
  ptr = word_s.get_ptr(0);
    
  // if char input, push to word stack
  if (chr > 0) { 
    ptr = word_s.push(chr);

    if (ptr < MAXWORD_TXT-1) {
      setcursor(1, -1, -1, -1, &cursor_c, &cursor_r);
      tft.setTextColor(ILI9341_WHITE);  
      tft.print("_");
    
      tft.setTextColor(ILI9341_BLACK);  
      setcursor(1, ptr, 0, 0, &c, &r);
      tft.print(chr);
      setcursor(1, ptr+1, 0, 0, &cursor_c, &cursor_r);
      tft.print("_");
    }
  }
  else { // backspace
     setcursor(1, -1, -1, -1, &cursor_c, &cursor_r);
     tft.setTextColor(ILI9341_WHITE);  
     tft.print("_");

     i = word_s.pop();
     ptr = word_s.get_ptr(0);
     setcursor(1, ptr+1, 0, 0, &c, &r);   
     tft.fillRect(c, r, 50, 25, ILI9341_WHITE);

     setcursor(1, ptr+1, 0, 0, &cursor_c, &cursor_r);
     tft.setTextColor(ILI9341_BLACK);  
     tft.print("_");
  }
  
  return;
}

// set the tft cursor in the position needed
// if pos is provided, placement is relative to 0, 0 with wrapping as each line is filled
// if row/col provided, placement is absolute
// -1 in pos indicates that row/col placement to be used
// -1 in row indicates that tftcol and tftrow be used for placement
// mode - 0 do nothing but return values in tftcol and tftrow. non-zero move cursor and return values
void setcursor(short mode, int pos, int col, int row, int *tftcol, int *tftrow) {
static short charSizeC=17;
static short charSizeR=30;
short pos1, r, c;
char buf5[50];

    if (pos != -1) {  // based on pointer - lines can overflow
       pos1 = pos -1; // subtract one to make zero based
       if (pos1 < NCOL) {
            *tftrow = 0;
            *tftcol = pos1 * charSizeC;
        }
        else {
            r = (short) pos1 / NCOL; 
            *tftrow = charSizeR * r;
            *tftcol = charSizeC * (pos1 - (r * NCOL)); 
        }
    }
    else if (row != -1) {
       *tftrow = charSizeR * row;
       *tftcol = charSizeC * col;
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

// lookup current value of parm code
int get_parm_code(char *p, int *v) {
  
  int rc = -1;
  
  if (!strcmp(p, "VC"))
    rc = *v = pr_vc;    
  
  return rc;
}

// update current value of parm code
int update_parm_code(char *p, int val) {
  int v[NPARMS];
  int rc = 0;
     
  // new value for voice (0 - 8)
  pr_vc = val;  
  v[0] = pr_vc;
  pcode.putcode(v);
  rc = updateUserParmFile();
  setvoice(pr_vc);

  return rc;
}  

// update user parm file
int updateUserParmFile() {
  int rc = 0, i, j;
  int v[NPARMS];
  char buf[BUFPCODE];
  
    if (SD.exists(USR_PARM))
        SD.remove(USR_PARM); // delete file in case it exists
    delay(300);
    File dataFile = SD.open(USR_PARM, FILE_WRITE);

    if (dataFile) {
        pcode.getcode(v); // get next element of stack
        sprintf(buf, "%d,\n", v[0]);
        dataFile.print(buf);
        dataFile.flush();
        dataFile.close();
        rc = 0; 
    }
    else
      rc = 4;        
 
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
    
    for (i=0; i < 10; i++) period[i] = 0; 
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
            p += period[nperiod -2] + 1;
    }

   strcpy(txt2, p); 
    p2 = txt2;
    
    if (!strncmp(txt2, "OK>", 3))
        p2 += 3; 

    strcpy(txt, p2);    
}

// check input parm - is it valid?
// return 0 OK 
// return 1 parm not found
// return 2 parm not numeric
// return 3 range error
// return 4 file error
int chk_parm(char *p_pword, int *Parm_val, int *Parm_new, char *parm){
    char buf[40];
    char val[20];
    char *p;
    int i, i1, j, rc;
    int prev_val, ival, ival_upper, ival_lower;
    
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
      rc = 1;
      
    for (i = 0, i1=0; i< j; i++) {
        if (isdigit(val[i]));
          i1++;
    }
    if (!rc && i1 != j)
      rc = 2;

    ival = atol(val); 
    
    // first check Voice and Font Parameters
    if (!(strcmp(parm, "VC")) && (ival < 0 || ival > 8))
       rc = 3;    
 
    if (!rc)
      *Parm_new = ival;
    else
      *Parm_new = 0; 
      
    return rc;
}

// /L function
// show current values of parms 
int listparm(){
  int v[NPARMS];
  char buf[20];
  int i;
  
  i = pcode.getcode(v);
  sprintf(buf, "Voice: %d ", v[0]);
  outstr(1, 1, buf); 
  
  sprintf(buf, "Voice: %d, i=%d ", v[0], i);  
  Serial.println(buf); 
}

// show current values of Voice or Font Parm
int show_parm(int n){
  int v[NPARMS];
  char buf[20];
  char buf0[25];
  int i;

  if (n == 0)
      strcpy(buf, "Voice:"); 

  // lookup parms - get voice
  i = pcode.getcode(v);  
  sprintf(buf0, "%s %d ", buf, v[n]);
  outstr(1, 1, buf0); 

  sprintf(buf, "Voice: %d, i=%d ", v[0], i);  
  Serial.println(buf); 
}

// /D function
//  delete user parm file
int delparm(){ 
  
  if (SD.exists(USR_PARM))
    SD.remove(USR_PARM); // delete file if nothing on stack

  outstr(1, 1, " OK "); 
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
   
   sprintf(buf, "N%d\n", v);
   Serial1.print(buf); 
   Serial1.flush();
}

void bmpDraw(char *filename, uint8_t x, uint16_t y) {
  File     bmpFile;
  int      bmpWidth, bmpHeight;   // W+H in pixels
  uint8_t  bmpDepth;              // Bit depth (currently must be 24)
  uint32_t bmpImageoffset;        // Start of image data in file
  uint32_t rowSize;               // Not always = bmpWidth; may have padding
  uint8_t  sdbuffer[3*BUFFPIXEL]; // pixel buffer (R+G+B per pixel)
  uint8_t  buffidx = sizeof(sdbuffer); // Current position in sdbuffer
  boolean  goodBmp = false;       // Set to true on valid header parse
  boolean  flip    = true;        // BMP is stored bottom-to-top
  int      w, h, row, col;
  uint8_t  r, g, b;
  uint32_t pos = 0, startTime = millis();

  if((x >= tft.width()) || (y >= tft.height())) return;

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
  if(read16(bmpFile) == 0x4D42) { // BMP signature
    Serial.print(F("File size: ")); Serial.println(read32(bmpFile));
    (void)read32(bmpFile); // Read & ignore creator bytes
    bmpImageoffset = read32(bmpFile); // Start of image data
    Serial.print(F("Image Offset: ")); Serial.println(bmpImageoffset, DEC);
    // Read DIB header
    Serial.print(F("Header size: ")); Serial.println(read32(bmpFile));
    bmpWidth  = read32(bmpFile);
    bmpHeight = read32(bmpFile);
    if(read16(bmpFile) == 1) { // # planes -- must be '1'
      bmpDepth = read16(bmpFile); // bits per pixel
      Serial.print(F("Bit Depth: ")); Serial.println(bmpDepth);
      if((bmpDepth == 24) && (read32(bmpFile) == 0)) { // 0 = uncompressed

        goodBmp = true; // Supported BMP format -- proceed!
        Serial.print(F("Image size: "));
        Serial.print(bmpWidth);
        Serial.print('x');
        Serial.println(bmpHeight);

        // BMP rows are padded (if needed) to 4-byte boundary
        rowSize = (bmpWidth * 3 + 3) & ~3;

        // If bmpHeight is negative, image is in top-down order.
        // This is not canon but has been observed in the wild.
        if(bmpHeight < 0) {
          bmpHeight = -bmpHeight;
          flip      = false;
        }

        // Crop area to be loaded
        w = bmpWidth;
        h = bmpHeight;
        if((x+w-1) >= tft.width())  w = tft.width()  - x;
        if((y+h-1) >= tft.height()) h = tft.height() - y;

        // Set TFT address window to clipped image bounds
        tft.setAddrWindow(x, y, x+w-1, y+h-1);

        for (row=0; row<h; row++) { // For each scanline...

          // Seek to start of scan line.  It might seem labor-
          // intensive to be doing this on every line, but this
          // method covers a lot of gritty details like cropping
          // and scanline padding.  Also, the seek only takes
          // place if the file position actually needs to change
          // (avoids a lot of cluster math in SD library).
          if(flip) // Bitmap is stored bottom-to-top order (normal BMP)
            pos = bmpImageoffset + (bmpHeight - 1 - row) * rowSize;
          else     // Bitmap is stored top-to-bottom
            pos = bmpImageoffset + row * rowSize;
          if(bmpFile.position() != pos) { // Need seek?
            bmpFile.seek(pos);
            buffidx = sizeof(sdbuffer); // Force buffer reload
          }

          for (col=0; col<w; col++) { // For each pixel...
            // Time to read more pixel data?
            if (buffidx >= sizeof(sdbuffer)) { // Indeed
              bmpFile.read(sdbuffer, sizeof(sdbuffer));
              buffidx = 0; // Set index to beginning
            }

            // Convert pixel from BMP to TFT format, push to display
            b = sdbuffer[buffidx++];
            g = sdbuffer[buffidx++];
            r = sdbuffer[buffidx++];
            tft.pushColor(tft.color565(r,g,b));
          } // end pixel
        } // end scanline
        Serial.print(F("Loaded in "));
        Serial.print(millis() - startTime);
        Serial.println(" ms");
      } // end goodBmp
    }
  }

  bmpFile.close();
  if(!goodBmp) Serial.println(F("BMP format not recognized."));
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

