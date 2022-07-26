#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <err.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define clear_screen() printf("\033[H\033[J")
#define cursor_to(x, y) printf("\033[%d;%dH", (y), (x))


int resize_window(int, int);
char **map_initiate(int, int, int);
void map_print(char**, int, int);
void map_update(char **, int, int, int, int, int, int);
void map_free(char **, int);


/* 
    Resize window to argument dimensions (rowsXcolumns) 
    and return status code.
*/
int resize_window(int rows, int cols) {
    // extract current window dimensions
    struct winsize w;
    ioctl(0, TIOCGWINSZ, &w);
    // return resize status code
    if (w.ws_col != cols || w.ws_row != rows) {
        char escape_sequence_cmd[30];
        sprintf(escape_sequence_cmd, "printf '\e[8;%d;%dt'", rows, cols);
        return system(escape_sequence_cmd);
    }
    // window dimension already compatible
    return 0;
}


/*
    Allocate and fill initial game map.
*/
char **map_initiate(int rows, int cols, int sky_length) {
    // allocate rowsxcols 2d char array
    char **map = malloc(rows * sizeof(char *));
    if (!map) errx(1, "Memory allocation error.\n");
    for (int i = 0; i < rows; i++) {
        map[i] = malloc(cols);
        if (!map[i]) errx(1, "Memory allocation error.\n");
        // fill current row with spaces
        for (int j = 0; j < cols; j++) map[i][j] = ' ';
    }
    // fill in sky region
    for (int i = 0; i < sky_length; i++) {
        for (int j = 0; j < cols; j++)
            map[i][j] = (rand() % 1000 < 10) ? '*' : ' ';
    }
    // fill in ground
    for (int i = 0; i < cols; i++) map[rows-1][i] = (rand() % 100 < 15) ? '-' : '#';

    return map;
}


/*
    Print map to screen.
*/
void map_print(char **map, int rows, int cols) {
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++)
            fprintf(stdout, "%c", map[i][j]);
    }
}


/*
    Update map with new frame.
*/
void map_update(char **map, int rows, int cols, int player_row, int player_col, int sky_length, int jump_level) {
    // update sky
    for (int i = 0; i < sky_length; i++) {
        for (int j = 0; j < cols - 1; j++)
            map[i][j] = map[i][j + 1];
        map[i][cols - 1] = (rand() % 1000 < 10) ? '*' : ' ';
    }
    // update ground
    for (int i = 0; i < cols; i++)
        map[rows - 1][i] = map[rows - 1][i + 1];
    map[rows - 1][cols - 1] = (rand() % 100 < 25) ? '-' : '#';
    // update player
    map[player_row + jump_level][player_col] = 'M';
    map[player_row + jump_level-1][player_col] = 'O';
    map[player_row + jump_level-1][player_col - 1] = '/';
    map[player_row + jump_level-1][player_col + 1] = '\\';
    map[player_row + jump_level-2][player_col] = '@';
}


/*
    Free map from memory.
*/
void map_free(char **map, int rows) {
    for (int i = 0; i < rows; i++)
        free(map[i]);
    free(map);
}


int main() {

    // define window dimensions
    int WIN_ROW = 32;
    int WIN_COL = 100;
    // define player position
    int PLAYER_ROW = 30;
    int PLAYER_COL = 8;
    // number of rows sky should occupy
    int SKY_LENGTH = 20;
    // current player jump level (altitude)
    int JUMP_LEVEL = 0;
    //  how long to sleep (microsec) between frames
    int FRAME_DELAY = 25000;

    // resize the window
    int resize_status = resize_window(WIN_ROW, WIN_COL);
    if (resize_status == 1)
        errx(1, "Failed to resize window to %dx%d", WIN_ROW, WIN_COL);

    // allocate game map
    char **map = map_initiate(WIN_ROW, WIN_COL, SKY_LENGTH);
    
    // game loop
    clear_screen();
    while (1) {
        map_print(map, WIN_ROW, WIN_COL);
        map_update(map, WIN_ROW, WIN_COL, PLAYER_ROW, PLAYER_COL, SKY_LENGTH, JUMP_LEVEL);
        cursor_to(0, 0); // reset the cursor back to top left to draw new frame
        usleep(FRAME_DELAY);
    }
    
    // free map
    map_free(map, WIN_ROW);

    return 0;

}
