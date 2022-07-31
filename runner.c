#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <err.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <ncurses.h>

#define clear_screen() printf("\033[H\033[J")

#define WIN_ROW 32
#define WIN_COL 100

#define SKY_LENGTH (int) ((WIN_ROW / 2) - ((int) (WIN_ROW / 5)))
#define GROUND_LENGTH 3

#define GROUND_ROW (WIN_ROW - 1 - GROUND_LENGTH)

#define JUMP_AIRTIME 8
#define JUMP_HEIGHT 9
#define JUMP_WIDTH (2 * JUMP_HEIGHT + JUMP_AIRTIME)

#define OBSTACLE_WIDTH 11
#define OBSTACLE_CENTER_TO_EDGE 5

#define FRAME_DELAY 25000


int random_int(int, int);
int key_pressed(void);
int resize_window(void);

char **initiate_map(void);
void print_map(char**);
void erase_map_middle(char **);
void update_map(char **);
void free_map(char **);

int insert_player(char **, int, int);
int is_tree_edge_char(char **, int, int);
void remove_player(char **, int, int);

void insert_obstacle(char**, int, int);
void insert_obstacle_layer(char *, int, int, int, char *);

void insert_message(char **, char *);


int main(void) 
{
    srand(time(NULL));

    
    int player_row = GROUND_ROW;
    int player_col = (int) (WIN_COL / 5);

    int jump_state = 0;
    int jump_counter = 0;

    int obstacle_counter = random_int(50, 100); // number of frames between obstacles
    int obstacle_row = GROUND_ROW;
    int obstacle_col = WIN_COL + OBSTACLE_CENTER_TO_EDGE;

    int gameover = 0;

    
    // resize the window
    int resize_status = resize_window();
    if (resize_status == 1)
    {
        errx(1, "Failed to resize window to %dx%d", WIN_ROW, WIN_COL);
    }

    
    // allocate game map and insert player
    char **map = initiate_map();
    insert_player(map, player_row, player_col);
 
    
    // setup for making keystroke detection non-blocking
    // solution from: https://stackoverflow.com/questions/4025891/create-a-function-to-check-for-key-press-in-unix-using-ncurses
    initscr();
    cbreak();
    noecho();
    nodelay(stdscr, TRUE);
    scrollok(stdscr, TRUE);

    
    // game loop    
    while (!gameover)
    {
        print_map(map);
        
        // turn on jump state if space key detected (non-blocking)
        if (key_pressed())
        {
            if (getch() == 32) // space bar was pressed
            {
                jump_state = 1;
            }
        }

        // during jump state, update jump animation parameters
        if (jump_state)
        {
            if (jump_counter < JUMP_HEIGHT)
            {
                player_row--;
            }
            else if (jump_counter > JUMP_HEIGHT + JUMP_AIRTIME)
            {
                player_row++;
            }

            jump_counter++;

            if (jump_counter == JUMP_WIDTH)
            {
                jump_state = 0;
                jump_counter = 0;
                player_row = GROUND_ROW;
            }
        }

        // update map for next frame and check for game over
        erase_map_middle(map);
        update_map(map);

        // time for obstacle to come into frame
        if (obstacle_counter == 0)
        {
            insert_obstacle(map, obstacle_row, obstacle_col);
            
            if (obstacle_col + OBSTACLE_CENTER_TO_EDGE > 0)
            {
                obstacle_col--;
            }
            else // obstacle exits: reset its parameters
            {
                obstacle_counter = random_int(200, 300);
                obstacle_col = WIN_COL + OBSTACLE_CENTER_TO_EDGE;
            }
        }

        gameover = insert_player(map, player_row, player_col);

        if (!gameover) // prepare for next frame
        {
            usleep(FRAME_DELAY);
            
            if (obstacle_counter > 0)
            {
                obstacle_counter--;
            }
            
            move(0, 0); // reset the cursor back to top left to draw new frame
        }
    }

    // free map from memory
    free_map(map);

    // reset terminal config after using ncurses
    // https://invisible-island.net/ncurses/man/curs_initscr.3x.html#h3-endwin
    endwin();

    fprintf(stdout, "Game over :(\n");
    return 0;
}


/*
    Returns random integer within provided interval.
*/
int random_int(int lower, int upper)
{
    return rand() % (upper + 1 - lower) + lower;
}


/*
    Detect keystroke without blocking.
    Solution from: https://stackoverflow.com/questions/4025891/create-a-function-to-check-for-key-press-in-unix-using-ncurses
*/
int key_pressed(void)
{
    int ch = getch();
    if (ch != ERR)
    {
        ungetch(ch);
        return 1;
    }
    else
    {
        return 0;
    }
}


/*
    Resize terminal window to WIN_ROW x WIN_COL.
*/
int resize_window()
{
    // extract current window dimensions
    struct winsize w;
    ioctl(0, TIOCGWINSZ, &w);
    // return resize status code
    if (w.ws_row != WIN_ROW || w.ws_col != WIN_COL)
    {
        char escape_sequence_cmd[30];
        sprintf(escape_sequence_cmd, "printf '\e[8;%d;%dt'", WIN_ROW, WIN_COL);
        return system(escape_sequence_cmd);
    }
    // window dimension already compatible
    return 0;
}


/*
    Allocate and fill initial game map.
*/
char **initiate_map(void)
{
    // allocate rowsxcols 2d char array
    char **map = malloc(WIN_ROW * sizeof(char *));
    if (!map)
    {
        errx(1, "Memory allocation error.\n");
    }
    for (int i = 0; i < WIN_ROW; i++)
    {
        map[i] = malloc(WIN_COL);
        if (!map[i])
        {
            errx(1, "Memory allocation error.\n");
        }
        // fill current row with spaces
        for (int j = 0; j < WIN_COL; j++)
        {
            map[i][j] = ' ';
        }
    }

    // fill in sky region
    for (int i = 0; i < SKY_LENGTH; i++)
    {
        for (int j = 0; j < WIN_COL; j++)
        {
            map[i][j] = (rand() % 1000 < 10) ? '*' : ' ';
        }
    }

    // fill in ground region
    for (int i = WIN_ROW - GROUND_LENGTH; i < WIN_ROW - 1; i++)
    {
        for (int j = 0; j < WIN_COL; j++)
        {
            map[i][j] = (rand() % 100 < 50) ? '~' : '>';
        }
    }

    return map;
}


/*
    Print map to screen.
*/
void print_map(char **map)
{
    for (int i = 0; i < WIN_ROW; i++)
    {
        for (int j = 0; j < WIN_COL; j++)
        {
            fprintf(stdout, "%c", map[i][j]);
        }
    }
}


/*
    Covers the area between sky and ground with empty space.
    Erase player and obstacle to re-insert later.
*/
void erase_map_middle(char **map)
{
    for (int i = SKY_LENGTH ; i <= GROUND_ROW; i++)
    {
        for (int j = 0; j < WIN_COL; j++)
        {
            map[i][j] = ' ';
        }
    }
}


/*
    Update map with new frame.
*/
void update_map(char **map)
{
    for (int i = 0; i < WIN_ROW - 1; i++)
    {
        for (int j = 0; j < WIN_COL - 1; j++)
        {
            map[i][j] = map[i][j + 1];
        }
        if (i < SKY_LENGTH) // insert new stars
        {
            map[i][WIN_COL - 1] = (rand() % 1000 < 10) ? '*' : ' ';
        }
        else
        {
            map[i][WIN_COL - 1] = ' ';
        }
    }
    // insert new ground piece
    for (int i = WIN_ROW - GROUND_LENGTH; i < WIN_ROW - 1; i++)
    {
        map[i][WIN_COL - 1] = (rand() % 100 < 50) ? '~' : '>';
    }
    
}


/*
    Insert player drawing into the map.
*/
int insert_player(char **map, int player_row, int player_col)
{
    if (is_tree_edge_char(map, player_row, player_col) ||
        is_tree_edge_char(map, player_row - 1, player_col) ||
        is_tree_edge_char(map, player_row - 1, player_col - 1) ||
        is_tree_edge_char(map, player_row - 1, player_col + 1) ||
        is_tree_edge_char(map, player_row - 2, player_col))
    {
        return 1;
    }
    else
    {
        map[player_row][player_col] = 'M';
        map[player_row - 1][player_col] = 'O';
        map[player_row - 1][player_col - 1] = '/';
        map[player_row - 1][player_col + 1] = '\\';
        map[player_row - 2][player_col] = '@';
        return 0;
    }
}


/*
    Returns true if argument is part of tree edge characters '#o'
*/
int is_tree_edge_char(char **map, int r, int c)
{
    return ((map[r][c] == '#') || (map[r][c] == 'o'));
}


/*
    Replaces player with whitespace in map.
*/
void remove_player(char **map, int player_row, int player_col)
{
    map[player_row][player_col] = ' ';
    map[player_row - 1][player_col] = ' ';
    map[player_row - 1][player_col - 1] = ' ';
    map[player_row - 1][player_col + 1] = ' ';
    map[player_row - 2][player_col] = ' ';
}


/*
    Free map from memory.
*/
void free_map(char **map)
{
    for (int i = 0; i < WIN_ROW; i++)
    {
        free(map[i]);
    }
    free(map);
}


/*
    Add obstacle render to game map.
*/
void insert_obstacle(char **map, int obstacle_row, int obstacle_col)
{
    int to_render = 0;
    int insertion_type = 0;

    // obstacle entering frame account for left edge
    // to_render captures number of left most chars already visible
    if (obstacle_col >= WIN_COL)
    {
        to_render = WIN_COL - (obstacle_col - OBSTACLE_CENTER_TO_EDGE);
        insertion_type = 1;
    }

    // obstacle exitting frame
    // to_render captures number of right most chars still visible
    if (obstacle_col - OBSTACLE_CENTER_TO_EDGE < 0)
    {
        to_render = obstacle_col + OBSTACLE_CENTER_TO_EDGE;
        insertion_type = 2;
    }

    insert_obstacle_layer(map[GROUND_ROW - 0], insertion_type, to_render, obstacle_col, "    |||    ");
    insert_obstacle_layer(map[GROUND_ROW - 1], insertion_type, to_render, obstacle_col, "    |||    ");
    insert_obstacle_layer(map[GROUND_ROW - 2], insertion_type, to_render, obstacle_col, " ###\\|/#o# ");
    insert_obstacle_layer(map[GROUND_ROW - 3], insertion_type, to_render, obstacle_col, "#o#\\#|#/###");
    insert_obstacle_layer(map[GROUND_ROW - 4], insertion_type, to_render, obstacle_col, "#o#\\#|#/###");
    insert_obstacle_layer(map[GROUND_ROW - 5], insertion_type, to_render, obstacle_col, "   #o###   ");
}


/*
    insert_obstacle() helper.
    adds the string (obstacle layer) to predefined section of the row.
*/
void insert_obstacle_layer(char *row, int insertion_type, int to_render, int obstacle_col, char *layer)
{
    int layer_length = strlen(layer);

    if (insertion_type == 0)
    {
        for (int i = 0; i < layer_length; i++)
        {
            row[obstacle_col - OBSTACLE_CENTER_TO_EDGE + i] = layer[i];
        }
    }
    else if (insertion_type == 1)
    {
        for (int i = 0; i < to_render; i++)
        {
            row[WIN_COL - to_render + i] = layer[i];
        }
    }
    else
    {
        for (int i = 0; i < to_render; i++)
        {
            row[i] = layer[layer_length - to_render + i];
        }
    }
}


/*
    Insert string at the bottom of the map.
*/
void insert_message(char **map, char *message)
{
    int message_length = strlen(message);
    for (int i = 0; i < message_length; i++)
    {
        map[WIN_ROW-1][i] = message[i];
    }
}