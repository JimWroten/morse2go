/* m2g.cpp -- class file
 * see morse2go.org for more into
 *
This work is licensed 2014 by Jim Wroten ( www.jimwroten.com ) under a Creative 
Commons Attribution-ShareAlike 4.0 International License. For more information 
about this license, see www.creativecommons.org/licenses/by-sa/4.0/ 
-- Basically, you can use this software for any purpose for free, 
as long as you say where you got it and that if you modify it, you don't remove
any lines above THIS line. 

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
*/

#include <stdlib.h>
//#include <cstring.h>
#include "m2g.h"
//#include <arduino.h>

#if defined(ARDUINO) && ARDUINO >= 100
#include "Arduino.h"
#else
#include "WProgram.h"
#endif


// morse code functions

// -----------  morse codes -----------------
inline mcodes::mcodes() {
  cnt = 0; 
}

inline int mcodes::loadcode(char* buf) { 
	char * p;
	char * p1;
	char buf1[60];
	char typcode[10];
	char ktok[10];
	char vtok[10];
	long key;

        strcpy(buf1, buf);

	p1 = strtok(buf1, ",");
	strcpy(typcode, p1);  // type of code, eg, mcode

	p1 = strtok(NULL, ",");
	strcpy(ktok, p1);
	key = strtol(ktok, NULL, 10);  // key of code, eg, 12, 2111 (a, b)
	
	p1 = strtok(NULL, ",");
	strcpy(vtok, p1); // value of code, eg A, B

	mkey[cnt] = key;  
	mval[cnt] = vtok[0];
	cnt++;
}

inline int mcodes::sortcode(){ 

   q_sort(mkey, mval, 0, cnt-1);
  
}

// quick sort - code copied from 
// (http://) p2p.wrox.com/visual-c/66347-quick-sort-c-code.html 

inline void mcodes::q_sort(long *mkeys, char *mvals, int left, int right) {
  long pivot, l_hold, r_hold;
  char pivot_v;
 
  l_hold = left;
  r_hold = right;
  pivot = mkeys[left];
  pivot_v = mvals[left];
    
  while (left < right)
  {    
    while ((mkeys[right] >= pivot) && (left < right))
      right--;
    if (left != right)
    {
      mkeys[left] = mkeys[right];
      mvals[left] = mvals[right];
      left++;
    }
    while ((mkeys[left] <= pivot) && (left < right))
      left++;
    if (left != right)
    {
      mkeys[right] = mkeys[left];
      mvals[right] = mvals[left];
      right--;
    }
  }
  mkeys[left] = pivot;
  mvals[left] = pivot_v;
  pivot = left;
  pivot_v = left;
  left = l_hold;
  right = r_hold;
  if (left < pivot)
    q_sort(mkeys, mvals, left, pivot-1);
  if (right > pivot)
    q_sort(mkeys, mvals, pivot+1, right);
}

// binary search -- adapted from
// (http://) rosettacode.org/wiki/Binary_search
inline int mcodes:: getcode(long key, char *val){ 
    int low = 0, high = cnt-1, mid;
        
    while(low <= high) {
         mid = (low + high) / 2;
         if(mkey[mid] < key) {
                        low = mid + 1; 
                }
                else if(mkey[mid] == key) {
                    *val = mval[mid];
                     return 0;
                }
                else if(mkey[mid] > key) {
                     high = mid-1;
                }
        }
        *val = '?';
        Serial.print("not found: ");
        Serial.println(key, DEC);
        return -1;
}

inline int mcodes:: dumpcodes(char *str){
   char buf[100];
  
   for (int i = 0; i < cnt; i++) {
       memset(buf, 1, 100); 
       sprintf(buf, "%d- %ld : %c\n", i, mkey[i], mval[i]);
       strcat(str, buf);
   }
}

// -----------  short codes -----------------
inline scodes::scodes() {
  cnt = 0; 
}

// load short codes into class
inline int scodes::loadcode(char *buf) { 
	char * p;
	char * p1;
        char * p2;
	char buf1[100];
	char typcode[10];
	char ktok[10];
	char vtok[MAXSCODE_TXT];
	int lineno, i; 

        memset(buf1, 0, 100); 
	strncpy(buf1, buf, MAXSCODE_TXT-1);
        memset(ktok, 0, 10);
        memset(vtok, 0, MAXSCODE_TXT);

        p1 = strtok(buf1, ",");
	strcpy(typcode, p1);  // type of code, eg, scode

	p1 = strtok(NULL, ",");
	strncpy(ktok, p1+1, 2);  // key of code, eg, ":ih", ":it" 
        int klen = strlen(ktok);
        for (int k = 0; k < klen; k++)  // convert to uppercase
            ktok[k] = toupper(ktok[k]); 
	
	p1 = strtok(NULL, ",");
	strcpy(vtok, p1);  // value of code, eg, "I am Thirsty"
  
	strcpy(skey[cnt], ktok);  
	strcpy(sval[cnt], vtok);
        cnt++;
}

inline int scodes::sortcode(){ 
}

// send k (key), return v (value)
inline int scodes::getcode(char *k, char *v){ 
  int i, rc, lenk, lencode;
	int fnd = -1;
  char buf[100];
        	
	for (i=0; i< cnt && fnd < 0; i++) {
		  if (strcmp(k, skey[i]) == 0) { // code found
			    strcpy(v, sval[i]);
			    fnd = i;
		  }
	}
	if (fnd < 0) {
      Serial.begin(9600);
      Serial.print("not found: ");
      Serial.println(k);
	    strcpy(v, "?");
         rc = fnd;
  }
      else
         rc = strlen(v);
	return (rc);
}

// -----------  parameter codes - timing variables -----------------
// constructor
inline pcodes::pcodes() {
    pvc = -1;
}

// load paramter codes into class
// input is Google Doc, pcode section
// push one set of codes onto stack

inline int pcodes::loadcode(char *buf) { 
	char * p1;
	char buf1[100];
	char typcode[10];
	char ktok[10];
	char vtok[10];
	int val;
        int len;
        int k;
        
	strcpy(buf1, buf);
        memset(ktok, 0, 10);
        memset(vtok, 0, 10);
 
        p1 = strtok(buf1, ",");
	strcpy(typcode, p1);  // type of code - pcode

	p1 = strtok(NULL, ",");
	strcpy(ktok, p1+1);

        len = strlen(ktok);
        for (k = 0; k < len; k++)  // convert to lowercase
            ktok[k] = tolower(ktok[k]); 

	p1 = strtok(NULL, ",");
	strcpy(vtok, p1);
        val = atoi(vtok);

       if (!strcmp(ktok, "vc"))
             pvc = val;
}

// load user input paramter codes into class
// input is microSD card - file USR_PARM.CSV
inline int pcodes::loadparmcode(char *str) { 
	char *p, *p1;
        char buf[BUFPCODE];
        char v_str[10];
	int lineno, i, j; 

        strcpy(buf, str);
	p1 = strtok(buf, ",");
	strcpy(v_str, p1);
        pvc = atoi(v_str);          
}

// put new values of parm codes into struct
inline int pcodes::putcode(int *v) {
    pvc = v[0];
}

// lookup parameter codes that exist in struct
inline int pcodes::getcode(int *v){                
        v[0] = pvc;
}
 
inline int pcodes::clear(){ 
       pvc = 0;
}

// ---------------- character functions ------------------

inline char_stk::char_stk() {
  clear();
}

inline int char_stk::clear() {
  for (int i=0; i < MAXDD; i++)
    ditdah[i] = 0;
  ptr = 0;
}

inline int char_stk::push(int c) {
  if (ptr < MAXDD)
    ditdah[ptr++] = c;
  return ptr;    
}

inline int char_stk::size() {
    return ptr; 
}

inline int char_stk::pop() {
  if (ptr >= 0) {
    ditdah[ptr] = 0;
    ptr--;
  } 
}

// get value of character, eg, 12 or 2112
// returns value of ptr, ditdah[] (n and dd)
inline long char_stk::get_charval(int &n, int *dd) {
  long mult = 1;
  int i;
  long ch = 0;
  
  n = ptr; 
  for (i = ptr-1; i > -1; i--) {
    dd[i] = ditdah[i];
    ch = ch + (ditdah[i] * mult);
    mult = mult * 10;
  }
  return ch;
}

// -------------- word functions ----------------

inline word_stk::word_stk() {
  clear();
}

// clear class
inline int word_stk::clear() {
  int i;
  memset(words, 0, MAXWORD);
  ptr = 0; 
}

// push a character onto the word stack
inline int word_stk::push(char c) {
  words[ptr] = c;
  if (ptr < MAXWORD)
    ptr++;
  return ptr; 
}

// -- backspace -- 
// pop the last character in the word stack
// return character pop'd
inline int word_stk::pop() {
  int done = 0;  // break flag
  int ptr1;
  char buf[25], c;
  int i, len;
  
  // erase char
  if (ptr > 0) {
    ptr--;
    c = words[ptr];
    words[ptr] = 0;
  }
  if (ptr == 0) 
    clear();
    
  return (int) c; 
}

// get value of ptr (i==o) or prev_ptr i < 0)
inline int word_stk::get_ptr() {
      return ptr;
}

// get value of previous word
inline int word_stk::get_pword(char* pwrd) {

  if (ptr == 0) {
     memset(pwrd, 0, SIZPWORD); 
     return 0;
  }
  strcpy(pwrd, words); 
  int i = strlen(pwrd);   
  return i; 
  
  /*
  memset(buf1, 0, 5);
  strncpy(buf1, words, 3); 
  p = words; 
  if (!strcmp(buf1, "OK>"))
      p +=3; 
  
  strcpy(buf, p); 
  memset(pwrd, 0, SIZPWORD);

  tok = strtok(buf, s);
  while(tok != NULL)  {
     strcpy(pwrd, tok); 
     tok = strtok(NULL, s);
  }
  i = strlen(pwrd);   
  return i;
  */ 
}

// ----------- Message functions ---------------
inline message_stk::message_stk() {
  msg = (char **)malloc(MAXWORDS * sizeof(char *));
  ptr = 0; 
}

// clear stack 
inline int message_stk::clear() {
   int i; 
   for (i=0; i< ptr; i++)
       free(msg[i]); 
   ptr = 0; 
}

// push a word to the stack 
inline int message_stk::push(char* wrd) {
    int i; 

    // if stack full, return with no action
    if (ptr > MAXWORDS-1) 
        return ptr;
        
    i = strlen(wrd); 
    msg[ptr] = (char *)malloc(i + 1);
    strcpy(msg[ptr], wrd);
    ptr++;
    return ptr; 
}

// pop a word from the stack
inline int message_stk::pop(char* wrd) {
   ptr--;
   free(msg[ptr]); 
   return ptr; 
}

// get word n from the stack 
inline int message_stk::get_msg(int n, char *wrd) {
   if (n >= 0 && n < ptr) 
       strcpy(wrd, msg[n]); 
   else
       strcpy(wrd, msg[ptr-1]); 

   return n;
}

// get ptr  
inline int message_stk::get_ptr() {
   return ptr;
} 
