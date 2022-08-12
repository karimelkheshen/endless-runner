#include <stdlib.h>
#include <sys/ioctl.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <ncurses.h>

#define MIN_WIN_ROW 30
#define MIN_WIN_COL 70

#define FRAME_DELAY 17000 // number of microseconds to sleep between processing each frame

#define JUMP_HEIGHT 10 // player vertical jump height (number of rows)
#define JUMP_AIRTIME 8 // number of frames player should stay at JUMP_HEIGHT
#define JUMP_WIDTH 2 * JUMP_HEIGHT + JUMP_AIRTIME // total number of frames for jump animation

#define OBSTACLE_WIDTH 11 // number of columns obstacle takes up
#define OBSTACLE_CENTER_TO_EDGE 5

#define GAME_LENGTH 4 // see declaration of obs_max_gen_gap
#define GAME_MAX_DIFF_SCORE 4000 // see updating game difficulty in game loop


/*
    Function declarations.
*/
int random_int_between(int, int);
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
    * (!) Will cause issues on Windows.
    */
    struct winsize w;
    ioctl(0, TIOCGWINSZ, &w);
    if (w.ws_row < MIN_WIN_ROW || w.ws_col < MIN_WIN_COL)
    {
        fprintf(stdout, "Terminal window size too small to render on.\n");
        fprintf(stdout, "Must be larger than %dx%d.\n", MIN_WIN_ROW, MIN_WIN_COL);
        exit(EXIT_SUCCESS);
    }


    /*
    * Declare and initialise all game parameters.
    */
    // window dimensions
    int WIN_ROW = w.ws_row;
    int WIN_COL = w.ws_col;
    
    // environment parameters
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
    int obs_min_gen_gap = JUMP_WIDTH;
    int obs_max_gen_gap = GAME_LENGTH * obs_min_gen_gap;
    int obs_timer = random_int_between(obs_min_gen_gap, obs_max_gen_gap);
    int obs_col = WIN_COL + OBSTACLE_CENTER_TO_EDGE;
    

    /*
    * Allocate and initiate game map, then insert player.
    */
    // allocate rowsxcols 2d char array of spaces
    char **map = malloc(WIN_ROW * sizeof(char *));
    if (!map)
    {
        fprintf(stderr, "Memory allocation error trying to allocate game map.\n");
        exit(EXIT_FAILURE);
    }
    for (int i = 0; i < WIN_ROW; i++)
    {
        map[i] = malloc(WIN_COL);
        if (!map[i])
        {
            fprintf(stderr, "Memory allocation error trying to allocate game map.\n");
            exit(EXIT_FAILURE);
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
    * Configure the terminal to allow non-blocking while awaiting for keystroke during the game loop.
    * Terminal will automatically clear the screen because of initscr().
    * (!) Will cause issues on Windows.
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
        if (!sprintf(score_message, " SCORE: %d", player_score))
        {
            gameover = 1;
        }
        for (int i = 0; i < strlen(score_message); i++)
        {
            map[WIN_ROW - 1][i] = score_message[i];
        }


        /*
        * Update game difficulty based on player score.
        * The divisor is what I found to be a large enough score for player to reach max difficulty.
        */
        if (player_score < GAME_MAX_DIFF_SCORE)
        {
            difficulty = (player_score * ((obs_min_gen_gap * GAME_LENGTH) - obs_min_gen_gap + 1)) / GAME_MAX_DIFF_SCORE;
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
        if (obs_timer == 0)
        {
            insert_obstacle(map, WIN_COL, obs_col, ground_row);
            // maintain obstacle animation
            if (obs_col + OBSTACLE_CENTER_TO_EDGE > 0)
            {
                obs_col--;
            }
            else // obstacle exits: reset its parameters
            {
                obs_timer = random_int_between(obs_min_gen_gap, obs_max_gen_gap - difficulty);
                obs_col = WIN_COL + OBSTACLE_CENTER_TO_EDGE;
            }
        }


        gameover = insert_player(map, player_row, player_col, player_jump_state);

        if (!gameover) // prepare for next frame
        {
            usleep(FRAME_DELAY);
            
            if (obs_timer > 0)
            {
                obs_timer--;
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
int random_int_between(int upper, int lower)
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
void insert_obstacle(char **map, int WIN_COL, int obs_col, int ground_row)
{
    int num_visible_chars = 0;
    int insertion_type = 0;

    // obstacle entering frame account for left edge
    // num_visible_chars captures number of left most chars already visible
    if (obs_col >= WIN_COL)
    {
        num_visible_chars = WIN_COL - (obs_col - OBSTACLE_CENTER_TO_EDGE);
        insertion_type = 1;
    }

    // obstacle exitting frame
    // num_visible_chars captures number of right most chars still visible
    if (obs_col - OBSTACLE_CENTER_TO_EDGE < 0)
    {
        num_visible_chars = obs_col + OBSTACLE_CENTER_TO_EDGE;
        insertion_type = 2;
    }

    insert_obstacle_layer(map[ground_row - 0], WIN_COL, insertion_type, num_visible_chars, obs_col, "    |||    ");
    insert_obstacle_layer(map[ground_row - 1], WIN_COL, insertion_type, num_visible_chars, obs_col, "    |||    ");
    insert_obstacle_layer(map[ground_row - 2], WIN_COL, insertion_type, num_visible_chars, obs_col, " ###\\|/#o# ");
    insert_obstacle_layer(map[ground_row - 3], WIN_COL, insertion_type, num_visible_chars, obs_col, "#o#\\#|#/###");
    insert_obstacle_layer(map[ground_row - 4], WIN_COL, insertion_type, num_visible_chars, obs_col, "#o#\\#|#/###");
    insert_obstacle_layer(map[ground_row - 5], WIN_COL, insertion_type, num_visible_chars, obs_col, "   #o###   ");
}


/*
* insert_obstacle() helper.
* Adds the string (obstacle layer) to predefined section of the row.
*/
void insert_obstacle_layer(char *row, int row_length, int insertion_type, int to_render, int obs_col, char *layer)
{
    int layer_length = strlen(layer);

    if (insertion_type == 0)
    {
        for (int i = 0; i < layer_length; i++)
        {
            row[obs_col - OBSTACLE_CENTER_TO_EDGE + i] = layer[i];
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
