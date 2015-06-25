#ifndef __DOSLIB_H
#define __DOSLIB_H

#include <curses.h>

#ifdef __MSDOS__
#include <conio.h>
// #include <dos.h>
#else
/* conio.h stuff  */
void        clreol( void );
void        clrscr( void );
void        gotoxy( int __x, int __y );
int         wherex( void );
int         wherey( void );
int         getch_( void );
int         getch_( void );
int         getche( void );
int         putchar_( int c );
int         kbhit( void );
int         putch( int c );
void        doscroll(int from, int to);
void        dos_init(void);
void        dos_end(void);

void       delay( unsigned __milliseconds );

#endif
#endif
