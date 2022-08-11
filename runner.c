#include <stdlib.h>
#include <sys/ioctl.h>
#include <err.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <ncurses.h>

#define MIN_WIN_ROW 30
#define MIN_WIN_COL 60

#define JUMP_AIRTIME 8 // Number of frames player remains at JUMP_HEIGHT
#define JUMP_HEIGHT 10 // 
#define JUMP_WIDTH 2 * JUMP_HEIGHT + JUMP_AIRTIME

#define OBSTACLE_WIDTH 11
#define OBSTACLE_CENTER_TO_EDGE 5


/*
    Function declarations.
*/
int random_int(int, int);
int key_pressed(void);
int insert_player(char **, int, int, int);
int is_obstacle_edge_char(char **, int, int);
void insert_obstacle(char **, int, int, int);
void insert_obstacle_layer(char *, int, int, int, int, char *);


int main (void) 
{
    srand(time(NULL));

    /*
    * Get and check terminal window dimensions.
    */
    struct winsize w;
    ioctl(0, TIOCGWINSZ, &w);
    if (w.ws_row < MIN_WIN_ROW || w.ws_col < MIN_WIN_COL)
    {
        errx(1, "Terminal window size too small to render on.\nMust be larger than %dx%d.", MIN_WIN_ROW, MIN_WIN_COL);
    }

    /*
    * Declare and initialise all game parameters.
    */
    // window dimensions
    int WIN_ROW = w.ws_row;
    int WIN_COL = w.ws_col;
    
    // environment parameters
    int frame_delay = 18000;
    int sky_length = (int) ((WIN_ROW / 2) - (WIN_ROW / 9));
    int ground_length = (int)(WIN_ROW / 6);
    int ground_row = WIN_ROW - 1 - ground_length;
    int difficulty = 0;
    int gameover = 0;

    // player parameters
    int player_row = ground_row;
    int player_col = (int)(WIN_COL / 5);
    int player_score = 0;
    int player_jump_state = 0;
    int player_jump_counter = 0;

    // obstacle parameters
    int obstacle_min_gen_gap = 2 * JUMP_HEIGHT + JUMP_AIRTIME + 4;
    int obstacle_max_gen_gap = obstacle_min_gen_gap * 4;
    int obstacle_timer = random_int(obstacle_min_gen_gap, obstacle_max_gen_gap);
    int obstacle_col = WIN_COL + OBSTACLE_CENTER_TO_EDGE;
    

    /*
    * Allocate and initiate game map, then insert player.
    */
    // allocate rowsxcols 2d char array of spaces
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
        for (int j = 0; j < WIN_COL; j++)
        {
            map[i][j] = ' ';
        }
    }
    // fill in sky region with random stars
    for (int i = 0; i < sky_length; i++)
    {
        for (int j = 0; j < WIN_COL; j++)
        {
            map[i][j] = (rand() % 1000 < 10) ? '*' : ' ';
        }
    }
    // fill in ground region with pattern
    for (int j = 0; j < WIN_COL; j++)
    {
        map[WIN_ROW - ground_length][j] = '#';
    }
    for (int i = WIN_ROW - ground_length + 1; i < WIN_ROW - 1; i++)
    {
        for (int j = 0; j < WIN_COL; j++)
        {
            map[i][j] = (rand() % 100 < 50) ? '.' : ',';
        }
    }
 
    /*
    * Configure the terminal to allow non-blocking while awaiting for 
    * keystroke during the game loop.
    * Terminal will automatically clear the screen because of init
    */
    initscr();
    cbreak();
    noecho();
    nodelay(stdscr, TRUE);
    scrollok(stdscr, TRUE);

    
    /*
    * Game loop.
    */
    while (!gameover)
    {
        /*
        * Print map to screen.
        */
        for (int i = 0; i < WIN_ROW; i++)
        {
            for (int j = 0; j < WIN_COL; j++)
            {
                fprintf(stdout, "%c", map[i][j]);
            }
        }

        /*
        * Update and display player score.
        */
        player_score += 2;
        char score_message[WIN_COL];
        if (!sprintf(score_message, "Score: %d", player_score))
        {
            gameover = 1;
        }
        for (int i = 0; i < strlen(score_message); i++)
        {
            map[WIN_ROW - 1][i] = score_message[i];
        }

        /*
        * Update game difficulty based on player score.
        */
        if (
            player_score == 250 ||
            player_score == 500 ||
            player_score == 1000 ||
            player_score == 1500 ||
            player_score == 3500 ||
            player_score == 5000 ||
            player_score == 7000)
        {
            frame_delay -= 200;
            difficulty += (obstacle_min_gen_gap / 2);
        }
        if (player_score > 7000 && player_score % 100 == 0)
        {
            frame_delay -= 25;
        }

        /*
        * Check if space key is pressed, turn on player jump state to trigger animation.
        * Terminal must already be configured for non-blocking.
        */
        if (key_pressed())
        {
            if (getch() == 32) // space bar was pressed
            {
                player_jump_state = 1;
            }
        }

        /*
        * Maintain jump animation while player is jumping.
        */
        if (player_jump_state)
        {
            if (player_jump_counter < JUMP_HEIGHT)
            {
                player_row--;
            }
            else if (player_jump_counter > JUMP_HEIGHT + JUMP_AIRTIME)
            {
                player_row++;
            }

            player_jump_counter++;

            if (player_jump_counter == JUMP_WIDTH)
            {
                player_jump_state = 0;
                player_jump_counter = 0;
                player_row = ground_row;
            }
        }

        /*
        * Update map for next frame.
        */
        // shift elements to the left.
        for (int i = 0; i < WIN_ROW - 1; i++)
        {
            for (int j = 0; j < WIN_COL - 1; j++)
            {
                map[i][j] = map[i][j + 1];
            }
        }
        // erase map middle (player + obstacle)
        for (int i = sky_length; i <= ground_row; i++)
        {
            for (int j = 0; j < WIN_COL; j++)
            {
                map[i][j] = ' ';
            }
        }
        // update sky (add new random stars)
        for (int i = 0; i < sky_length; i++)
        {
            map[i][WIN_COL - 1] = (rand() % 1000 < 10) ? '*' : ' ';
        }
        // update ground
        map[ground_row + 1][WIN_COL - 1] = '#';
        for (int i = ground_row + 2; i < WIN_ROW - 1; i++)
        {
            map[i][WIN_COL - 1] = (rand() % 100 < 50) ? '.' : ',';
        }

        /*
        * Insert obstacle and update its animation.
        */
        if (obstacle_timer == 0)
        {
            insert_obstacle(map, WIN_COL, obstacle_col, ground_row);
            // maintain obstacle animation
            if (obstacle_col + OBSTACLE_CENTER_TO_EDGE > 0)
            {
                obstacle_col--;
            }
            else // obstacle exits: reset its parameters
            {
                obstacle_timer = random_int(obstacle_min_gen_gap, obstacle_max_gen_gap - difficulty);
                obstacle_col = WIN_COL + OBSTACLE_CENTER_TO_EDGE;
            }
        }

        // 
        gameover = insert_player(map, player_row, player_col, player_jump_state);

        if (!gameover) // prepare for next frame
        {
            usleep(frame_delay);
            
            if (obstacle_timer > 0)
            {
                obstacle_timer--;
            }
            
            move(0, 0); // reset the cursor back to top left to draw new frame
        }
    }

    int final_score = player_score;

    /*
    * Free map.
    */
    for (int i = 0; i < WIN_ROW; i++)
    {
        free(map[i]);
    }
    free(map);

    /*
    * Reset terminal after messing with its configurations.
    */
    endwin();

    fprintf(stdout, "Game over :(\n");
    fprintf(stdout, "Final Score: %d\n", final_score);

    return 0;
}

/*
* Returns random integer between bounds.
*/
int random_int(int upper, int lower)
{
    return rand() % (upper + 1 - lower) + lower;
}


/*
* Detect keystroke without blocking.
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
* Insert player drawing into the map.
*/
int insert_player(char **map, int player_row, int player_col, int player_jump_state)
{
    // If any player char insertion triggers collision return 1 to signal game over.
    if (is_obstacle_edge_char(map, player_row, player_col) ||
        is_obstacle_edge_char(map, player_row - 1, player_col) ||
        is_obstacle_edge_char(map, player_row - 1, player_col - 1) ||
        is_obstacle_edge_char(map, player_row - 1, player_col + 1) ||
        is_obstacle_edge_char(map, player_row - 2, player_col))
    {
        return 1;
    }
    else
    {
        map[player_row][player_col] = (rand() % 2 == 0) ? 'W' : 'M';
        map[player_row - 1][player_col] = 'O';
        if (player_jump_state)
        {
            map[player_row - 2][player_col - 1] = '\\';
            map[player_row - 2][player_col + 1] = '/';
        }
        else
        {
            map[player_row - 1][player_col - 1] = '/';
            map[player_row - 1][player_col + 1] = '\\';
        }
        map[player_row - 2][player_col] = '@';
        return 0;
    }
}


/*
* insert_player() helper.
*/
int is_obstacle_edge_char(char **map, int r, int c)
{
    return ((map[r][c] == '#') || (map[r][c] == 'o'));
}


/*
* Add obstacle render to game map.
*/
void insert_obstacle(char **map, int WIN_COL, int obstacle_col, int ground_row)
{
    int num_visible_chars = 0;
    int insertion_type = 0;

    // obstacle entering frame account for left edge
    // num_visible_chars captures number of left most chars already visible
    if (obstacle_col >= WIN_COL)
    {
        num_visible_chars = WIN_COL - (obstacle_col - OBSTACLE_CENTER_TO_EDGE);
        insertion_type = 1;
    }

    // obstacle exitting frame
    // num_visible_chars captures number of right most chars still visible
    if (obstacle_col - OBSTACLE_CENTER_TO_EDGE < 0)
    {
        num_visible_chars = obstacle_col + OBSTACLE_CENTER_TO_EDGE;
        insertion_type = 2;
    }

    insert_obstacle_layer(map[ground_row - 0], WIN_COL, insertion_type, num_visible_chars, obstacle_col, "    |||    ");
    insert_obstacle_layer(map[ground_row - 1], WIN_COL, insertion_type, num_visible_chars, obstacle_col, "    |||    ");
    insert_obstacle_layer(map[ground_row - 2], WIN_COL, insertion_type, num_visible_chars, obstacle_col, " ###\\|/#o# ");
    insert_obstacle_layer(map[ground_row - 3], WIN_COL, insertion_type, num_visible_chars, obstacle_col, "#o#\\#|#/###");
    insert_obstacle_layer(map[ground_row - 4], WIN_COL, insertion_type, num_visible_chars, obstacle_col, "#o#\\#|#/###");
    insert_obstacle_layer(map[ground_row - 5], WIN_COL, insertion_type, num_visible_chars, obstacle_col, "   #o###   ");
}


/*
* insert_obstacle() helper.
* Adds the string (obstacle layer) to predefined section of the row.
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
