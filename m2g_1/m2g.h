/* m2g.h -- header file
 * see morse2go.org for more into
 *
This work is licensed 2014 by Jim Wroten ( www.jimwroten.com ) under a Creative 
Commons Attribution-ShareAlike 4.0 International License. For more information 
about this license, see www.creativecommons.org/licenses/by-sa/4.0/ 
-- Basically, you can use this software for any purpose for free, 
as long as you say where you got it and that if you modify it, you don't remove
any lines above THIS line. 

------------------ Release History  ---------------------------

8/28/14 Initial Release 1.0 

*/

#define MAXDD 20
#define MAXCHAR 80
#define MAXMCODES 200
#define MAXSCODES 100
#define MAXPCODES 20
#define BUFSZ 3000
#define BUFMCODE 2000
#define BUFSCODE 2000
#define BUFPCODE 1000
#define SIZPWORD 20
#define SIZMESG 80
#define NCOL 20
#define NROW 4
#define DEBUG 0
#define MSDO 225
#define MSDA 1000
#define MSLT 2000
#define MSCL 5000
#define CODE "CODE.CSV"
#define USR_PARM "USR_PARM.CSV"

// bitwise ops
#define MODE_MORSE 1
#define MODE_SHORT 2
#define MODE_PARM 4

// morse codes 
class mcodes {
	public:
	long mkey[MAXMCODES];
	char mval[MAXMCODES];
	int cnt;
        mcodes();
	int loadcode(char*);
	int sortcode();
	int getcode(long, char *);
        int dumpcodes (char *); // for testing
        
        private:
        void q_sort(long*, char*, int, int); 
};

// short codes 
class scodes {
	public:
	char skey[MAXSCODES][10];
	char sval[MAXSCODES][80];
	int cnt;
        scodes();
	int loadcode(char *);
	int sortcode();
	int getcode(char *, char *);
};

// parameter codes 
class pcodes {
	public:
	char pkey[MAXPCODES][10];
	unsigned pval[MAXPCODES];
	int cnt;
        pcodes();
	int loadcode(char *);
	int sortcode();
	int getcode(char *, unsigned *);
        int clear();
};

// char class - stack of dits and dahs
class char_stk {
  public:
    int ditdah[MAXDD]; // stack of dit and dahs
    int ptr; // pointer to top of ditdah stack
    char_stk();
    int push(int); // push a ditdah  
    int pop(); // pop last ditdah
    int clear(); // clear stack
    long get_charval(); // get character value, eg, 12 or 2121
};

// word class -- stack of characters and spaces
class word_stk {
   public:
     char words[MAXCHAR]; // word stack [characters]
     int ptr; // pointer to current char
     int prev_ptr; // pointer to prev value of ptr, when it was a space -- used to find last token
     word_stk();
     int clear(); // clear the stack
     int push(char); // push a char onto word stack
     int push_words(char *); // push a bunch of characters onto the word stack
     int pop(); // pop last character that was just pushed
     int nextword(); // go to next word
     int get_words(char *); // get string of word stack
     int save_ptr(int); // save pointer in prev_ptr
     int get_ptr(int); // get ptr or prev_ptr
     int get_pword(char *); // get previous word
     int trim_words(); // trim length of words stack 
}; 

