#include "util.h"

void cursor_to(int row, int col)
{
    #ifdef _WIN32
    COORD pos = {row, col};
    HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleCursorPosition(output, pos);
    
    #else
    fprintf(stdout, "\033[%d;%dH", col, row);

    #endif
}


void sleep_for_millis(int millis)
{
    // Solution from: https://stackoverflow.com/questions/5801813/c-usleep-is-obsolete-workarounds-for-windows-mingw
    #ifdef _WIN32
    __int64 micros = millis * 1000;
    HANDLE timer;
    LARGE_INTEGER ft;
    ft.QuadPart = -(10 * micros);
    timer = CreateWaitableTimer(NULL, TRUE, NULL);
    SetWaitableTimer(timer, &ft, 0, NULL, NULL, 0);
    WaitForSingleObject(timer, INFINITE);
    CloseHandle(timer);

    #else
    int micros = (int)millis * 1000;
    usleep(micros);

    #endif
}


void get_terminal_window_dimensions(int *rows, int *cols)
{
    #ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
    *rows = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
    *cols = csbi.srWindow.Right - csbi.srWindow.Left + 1;

    #else
    struct winsize w;
    ioctl(0, TIOCGWINSZ, &w);
    *rows = w.ws_row;
    *cols = w.ws_col;

    #endif
}


/* 
 * Return true (1) when Space key is pressed.
 * On Unix terminals, pre-configuring the terminal for 
 * non-blocking with ncurses is required for this to work.
 */
int space_key_pressed(void)
{
    #ifdef _WIN32
    // Solution from: https://stackoverflow.com/questions/41600981/how-do-i-check-if-a-key-is-pressed-on-c
    if (GetKeyState(' ') & 0x8000)
    {
        return 1;
    }
     return 0;

    #else
    int ch = getch();
    if (ch != ERR)
    {
        ungetch(ch);
        if (getch() == 32)
        {
            return 1;
        }
    }
    return 0;

    #endif
    return 0;
}
