#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <err.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <ncurses.h>

#define MIN_WIN_ROW 30
#define MIN_WIN_COL 60

#define JUMP_AIRTIME 8
#define JUMP_HEIGHT 10
#define JUMP_WIDTH 2 * JUMP_HEIGHT + JUMP_AIRTIME

#define OBSTACLE_WIDTH 11
#define OBSTACLE_CENTER_TO_EDGE 5

#define FRAME_DELAY 18000

#define PLAYER_BOTTOM_CHAR_SET "WWWWMMMM"

/*
    Only way to control game's gameplay and animation.
*/
typedef struct GAME_s
{
    int WIN_ROW;
    int WIN_COL;

    int sky_length;

    int ground_length;
    int ground_row;

    int player_row;
    int player_col;
    int player_bottom_animation_counter;
    
    int jump_state;
    int jump_counter;
    
    int obstacle_timer;
    int obstacle_row;
    int obstacle_col;
    
    int gameover;
} GAME;


/*
    Function declarations.
*/
int random_int(int, int);
void config_terminal(void);
int key_pressed(void);

void initialise_game_settings(GAME *);

char **initiate_map(GAME *);
void print_map(char **, GAME *);
void erase_map_middle(char **, GAME *);
void update_map(char **, GAME *);
void free_map(char **, GAME *);

int insert_player(char **, GAME *);
int is_obstacle_edge_char(char **, int, int);

void update_jump_state(GAME *g);

void insert_obstacle(char**, GAME *);
void update_obstacle_state(GAME *);
void insert_obstacle_layer(char *, int, int, int, int, char *);

void insert_message(char **, char *, GAME *);



int main(void) 
{
    srand(time(NULL));


    // get and check current terminal window dimensions
    struct winsize w;
    ioctl(0, TIOCGWINSZ, &w);
    if (w.ws_row < MIN_WIN_ROW || w.ws_col < MIN_WIN_COL)
    {
        errx(1, "Terminal window size too small to render on.\nMust be larger than %dx%d", MIN_WIN_ROW, MIN_WIN_COL);
    }


    // create and initialise game settings structure
    GAME *g = malloc(sizeof(GAME));
    if (!g)
    {
        errx(1, "Memory allocation error.\n");
    }
    g->WIN_ROW = w.ws_row;
    g->WIN_COL = w.ws_col;
    initialise_game_settings(g);
    

    // allocate game map and insert player
    char **map = initiate_map(g);
    insert_player(map, g);
 
    
    // keystroke detection must be non-blocking in game loop.
    // because of ncurses call to initscr(), terminal will automatically clear the screen.
    config_terminal();
    

    // ncurses clean screen procedure
    erase();
    refresh();

    
    // game loop
    while (!g->gameover)
    {
        print_map(map, g);
        

        // turn on jump state if space key detected
        // this must be non-blocking so it must be called after terminal_config()
        if (key_pressed())
        {
            if (getch() == 32) // space bar was pressed
            {
                g->jump_state = 1;
            }
        }

        // during jump state, update jump animation parameters
        if (g->jump_state)
        {
            update_jump_state(g);
        }

        // update map for next frame
        update_map(map, g);
        
        // if obstacle_timer done, insert obstacle into frame and reset timer
        if (g->obstacle_timer == 0)
        {
            insert_obstacle(map, g);
            update_obstacle_state(g);
        }

        // 
        g->gameover = insert_player(map, g);

        if (!g->gameover) // prepare for next frame
        {
            usleep(FRAME_DELAY);
            
            if (g->obstacle_timer > 0)
            {
                g->obstacle_timer--;
            }
            
            move(0, 0); // reset the cursor back to top left to draw new frame
        }
    }

    // free resources
    free_map(map, g);
    free(g);

    // reset terminal config
    endwin();

    fprintf(stdout, "Game over :(\n");
    return 0;
}



void initialise_game_settings(GAME *g)
{
    g->sky_length = (int) ((g->WIN_ROW / 2) - (g->WIN_ROW / 9));
    
    g->ground_length = (int) (g->WIN_ROW / 6);
    g->ground_row = g->WIN_ROW - 1 - g->ground_length;
    
    g->player_row = g->ground_row;
    g->player_col = (int)(g->WIN_COL / 5);
    g->player_bottom_animation_counter = 0;

    g->jump_state = 0;
    g->jump_counter = 0;

    g->obstacle_timer = random_int(50, 100);
    g->obstacle_row = g->ground_row;
    g->obstacle_col = g->WIN_COL + OBSTACLE_CENTER_TO_EDGE;

    g->gameover = 0;
}


/*
    Returns random integer within provided interval.
*/
int random_int(int lower, int upper)
{
    return rand() % (upper + 1 - lower) + lower;
}

/*
    Use ncurses to configure terminal such that keystroke input is non-blocking.
    Solution from: https://stackoverflow.com/questions/4025891/create-a-function-to-check-for-key-press-in-unix-using-ncurses
*/
void config_terminal(void)
{
    initscr();
    cbreak();
    noecho();
    nodelay(stdscr, TRUE);
    scrollok(stdscr, TRUE);
}


/*
    Detect keystroke without blocking.
    Only works if config_terminal() has already been called.
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
    Allocate and fill initial game map.
*/
char **initiate_map(GAME *g)
{
    // allocate rowsxcols 2d char array
    char **map = malloc(g->WIN_ROW * sizeof(char *));
    if (!map)
    {
        errx(1, "Memory allocation error.\n");
    }
    for (int i = 0; i < g->WIN_ROW; i++)
    {
        map[i] = malloc(g->WIN_COL);
        if (!map[i])
        {
            errx(1, "Memory allocation error.\n");
        }
        // fill current row with spaces
        for (int j = 0; j < g->WIN_COL; j++)
        {
            map[i][j] = ' ';
        }
    }

    // fill in sky region
    for (int i = 0; i < g->sky_length; i++)
    {
        for (int j = 0; j < g->WIN_COL; j++)
        {
            map[i][j] = (rand() % 1000 < 10) ? '*' : ' ';
        }
    }

    // fill in ground region
    for (int j = 0; j < g->WIN_COL; j++)
    {
        map[g->WIN_ROW - g->ground_length][j] = '#';
    }
    for (int i = g->WIN_ROW - g->ground_length + 1; i < g->WIN_ROW - 1; i++)
    {
        for (int j = 0; j < g->WIN_COL; j++)
        {
            map[i][j] = (rand() % 100 < 50) ? '.' : ',';
        }
    }

    return map;
}


/*
    Print map to screen.
*/
void print_map(char **map, GAME *g)
{
    for (int i = 0; i < g->WIN_ROW; i++)
    {
        for (int j = 0; j < g->WIN_COL; j++)
        {
            fprintf(stdout, "%c", map[i][j]);
        }
    }
}



/*
    Update map with new frame.
*/
void update_map(char **map, GAME *g)
{
    // update entire map
    for (int i = 0; i < g->WIN_ROW - 1; i++)
    {
        for (int j = 0; j < g->WIN_COL - 1; j++)
        {
            map[i][j] = map[i][j + 1];
        }
    }
    // erase map middle (player + obstacle)
    for (int i  = g->sky_length; i <= g->ground_row; i++)
    {
        for (int j = 0; j < g->WIN_COL; j++)
        {
            map[i][j] = ' ';
        }
    }
    // update sky
    for (int i = 0; i < g->sky_length; i++)
    {
        map[i][g->WIN_COL - 1] = (rand() % 1000 < 10) ? '*' : ' ';
    }
    // update ground
    map[g->ground_row + 1][g->WIN_COL - 1] = '#';
    for (int i = g->ground_row + 2; i < g->WIN_ROW - 1; i++)
    {
        map[i][g->WIN_COL - 1] = (rand() % 100 < 50) ? '.' : ',';
    }
}


/*
    Insert player drawing into the map.
*/
int insert_player(char **map, GAME *g)
{
    g->player_bottom_animation_counter++;
    // check if player's character insertions will overwrite an
    // obstacle's edge character.
    // If true return 1 to signal game over and end game loop.
    if (is_obstacle_edge_char(map, g->player_row, g->player_col) ||
        is_obstacle_edge_char(map, g->player_row - 1, g->player_col) ||
        is_obstacle_edge_char(map, g->player_row - 1, g->player_col - 1) ||
        is_obstacle_edge_char(map, g->player_row - 1, g->player_col + 1) ||
        is_obstacle_edge_char(map, g->player_row - 2, g->player_col))
    {
        return 1;
    }
    else
    {
        map[g->player_row][g->player_col] = PLAYER_BOTTOM_CHAR_SET[g->player_bottom_animation_counter % strlen(PLAYER_BOTTOM_CHAR_SET)];
        map[g->player_row - 1][g->player_col] = 'O';
        if (g->jump_state)
        {
            map[g->player_row - 2][g->player_col - 1] = '\\';
            map[g->player_row - 2][g->player_col + 1] = '/';
        }
        else
        {
            map[g->player_row - 1][g->player_col - 1] = '/';
            map[g->player_row - 1][g->player_col + 1] = '\\';
        }
        map[g->player_row - 2][g->player_col] = '@';
        return 0;
    }
}


/*
    insert_player() helper.
    Returns true if argument is part of tree edge characters '#o'
*/
int is_obstacle_edge_char(char **map, int r, int c)
{
    return ((map[r][c] == '#') || (map[r][c] == 'o'));
}


/*
    Free map from memory.
*/
void free_map(char **map, GAME *g)
{
    for (int i = 0; i < g->WIN_ROW; i++)
    {
        free(map[i]);
    }
    free(map);
}


/*
    Add obstacle render to game map.
*/
void insert_obstacle(char **map, GAME *g)
{
    int to_render = 0;
    int insertion_type = 0;

    // obstacle entering frame account for left edge
    // to_render captures number of left most chars already visible
    if (g->obstacle_col >= g->WIN_COL)
    {
        to_render = g->WIN_COL - (g->obstacle_col - OBSTACLE_CENTER_TO_EDGE);
        insertion_type = 1;
    }

    // obstacle exitting frame
    // to_render captures number of right most chars still visible
    if (g->obstacle_col - OBSTACLE_CENTER_TO_EDGE < 0)
    {
        to_render = g->obstacle_col + OBSTACLE_CENTER_TO_EDGE;
        insertion_type = 2;
    }

    insert_obstacle_layer(map[g->ground_row - 0], g->WIN_COL, insertion_type, to_render, g->obstacle_col, "    |||    ");
    insert_obstacle_layer(map[g->ground_row - 1], g->WIN_COL, insertion_type, to_render, g->obstacle_col, "    |||    ");
    insert_obstacle_layer(map[g->ground_row - 2], g->WIN_COL, insertion_type, to_render, g->obstacle_col, " ###\\|/#o# ");
    insert_obstacle_layer(map[g->ground_row - 3], g->WIN_COL, insertion_type, to_render, g->obstacle_col, "#o#\\#|#/###");
    insert_obstacle_layer(map[g->ground_row - 4], g->WIN_COL, insertion_type, to_render, g->obstacle_col, "#o#\\#|#/###");
    insert_obstacle_layer(map[g->ground_row - 5], g->WIN_COL, insertion_type, to_render, g->obstacle_col, "   #o###   ");
}


/*
    If obstacle still in frame, update it's current position.
    Otherwise reset obstacle_timer.
*/
void update_obstacle_state(GAME *g)
{
    if (g->obstacle_col + OBSTACLE_CENTER_TO_EDGE > 0)
    {
        g->obstacle_col--;
    }
    else // obstacle exits: reset its parameters
    {
        g->obstacle_timer = random_int(200, 300);
        g->obstacle_col = g->WIN_COL + OBSTACLE_CENTER_TO_EDGE;
    }
}


/*
    insert_obstacle() helper.
    adds the string (obstacle layer) to predefined section of the row.
*/
void insert_obstacle_layer(char *row, int row_length, int insertion_type, int to_render, int obstacle_col, char *layer)
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
            row[row_length - to_render + i] = layer[i];
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
void insert_message(char **map, char *message, GAME *g)
{
    int message_length = strlen(message);
    for (int i = 0; i < message_length; i++)
    {
        map[g->WIN_ROW - 1][i] = message[i];
    }
}

/*
    If jump state active, update jump_counter and player height.
    Otherwise reset jump state parameters.
*/
void update_jump_state(GAME *g)
{
    if (g->jump_counter < JUMP_HEIGHT)
    {
        g->player_row--;
    }
    else if (g->jump_counter > JUMP_HEIGHT + JUMP_AIRTIME)
    {
        g->player_row++;
    }

    g->jump_counter++;

    if (g->jump_counter == JUMP_WIDTH)
    {
        g->jump_state = 0;
        g->jump_counter = 0;
        g->player_row = g->ground_row;
    }
}
