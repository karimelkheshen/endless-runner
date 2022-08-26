#ifndef _UTIL_H
#define _UTIL_H


#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <sys/ioctl.h>
    #include <ncurses.h>
    #include <unistd.h>
#endif


void cursor_to(int, int);
void sleep_for_millis(int);
void get_terminal_window_dimensions(int *, int *);
int space_key_pressed(void);


#endif
