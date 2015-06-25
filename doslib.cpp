// Unix patches for manip (PH July, 2010)
#include <curses.h>

static int didInit = 0;

//PH
unsigned char   inportb( unsigned __portid ) { return (unsigned char)0; }
unsigned        inport ( unsigned __portid ) { return (unsigned char)0; }
int             inp( unsigned __portid ) { return 0; }
unsigned        inpw( unsigned __portid ) { return 0; }
void            outportb( unsigned __portid, unsigned char __value ) {  }
void            outport ( unsigned __portid, unsigned __value ) {  }
int             outp( unsigned __portid, int __value ) { return 0; }
unsigned        outpw( unsigned __portid, unsigned __value ) { return 0; }

#define printw(...) printw(__VA_ARGS__)

//PH
int wherex() {
    int x,y;
    getyx(stdscr,y,x);
    return x;
}
int wherey() {
    int x,y;
    getyx(stdscr,y,x);
    return y;
}
void doscroll(int from, int to)
{
    setscrreg(from,to);
    scrl(1);
//	move(1,to);
//	clrtoeol();
}

void clreol() { clrtoeol(); }
void clrscr() { erase(); }
void gotoxy(int x,int y) { move(y,x); }
int getch_() { return getch(); }
int kbhit() {
    timeout(0);
    int ch = getch();
    timeout(-1);
    if (ch) ungetch(ch);
    return ch;
}
int getche(){
    int ch = getch();
    if (ch > 0 && ch != 0x1b && ch < 0x7f) printw("%c",ch);
    return ch;
}

void dos_init() {
    initscr();
    scrollok(stdscr,1);
    cbreak();
    noecho();
    didInit = 1;
}

void dos_end()
{
    if (didInit) {
        didInit = 0;
        endwin();
    }
}

int putchar_(int c)
{
    return addch(c);
}