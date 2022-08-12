#include <stdlib.h>
#include <sys/ioctl.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <ncurses.h>

#define clear_screen() printf("\033[H\033[J")
#define cursor_to(x, y) printf("\033[%d;%dH", (y), (x))

#define MIN_WIN_ROW 30
#define MIN_WIN_COL 70

#define FRAME_DELAY 15000 // number of microseconds to sleep between processing each frame

#define JUMP_HEIGHT 10 // number of frames player needs to reach max jump height
#define JUMP_AIRTIME 8 // number of frames player hovers for when JUMP_HEIGHT is reached
#define JUMP_WIDTH 2 * JUMP_HEIGHT + JUMP_AIRTIME // total number of frames for player jump

#define OBSTACLE_LENGTH 6
#define OBSTACLE_WIDTH 11
#define OBSTACLE_CENTER_TO_EDGE 5 // number of columns half of obstacle takes up

#define GAME_LENGTH 4 // see declaration of obs_max_gen_gap
#define GAME_MAX_DIFF_SCORE 4000 // see updating game difficulty in game loop

void insert_obstacle(char **map, int WIN_COL, int obs_col, int ground_row);
void insert_obstacle_layer(char *row, int row_length, int insertion_type, int to_render, int obs_col, char *layer);
int insert_player(char **map, int player_row, int player_col, int player_jump_state);
int is_obstacle_edge_char(char **map, int r, int c);


int random_int_between(int upper, int lower)
{
    return rand() % (upper + 1 - lower) + lower;
}


/*
 * Returns 1 is keystroke is detected.
 * Terminal must be already be configured for non-blocking for this to work.
 */
int key_pressed(void)
{
    int ch = getch();
    if (ch != ERR)
    {
        ungetch(ch);
        return 1;
    }
    return 0;
}


int main (void)
{
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


    srand(time(NULL));

    /*
    * Character map of obstacle, used for drawing.
    */
    const char obstacle_char_map[OBSTACLE_LENGTH][OBSTACLE_WIDTH] =
    {
        {' ', ' ', ' ', ' ', '|', '|', '|', ' ', ' ', ' ', ' '},
        {' ', ' ', ' ', ' ', '|', '|', '|', ' ', ' ', ' ', ' '},
        {' ', '#', '#', '#', '\\', '|', '/', '#', 'o', '#', ' '},
        {'#', 'o', '#', '\\', '#', '|', '#', '/', '#', '#', '#'},
        {'#', 'o', '#', '\\', '#', '|', '#', '/', '#', '#', '#'},
        {' ', ' ', ' ', '#', 'o', '#', '#', '#', ' ', ' ', ' '}
    };


    /*
     * Declare and initialise all game parameters.
     */
    // window dimensions
    const int win_row = w.ws_row;
    const int win_col = w.ws_col;
    
    // environment parameters
    const int num_rows_for_sky = (int) ((win_row / 2) - (win_row / 9));
    const int num_rows_for_ground = (int)(win_row / 6);
    const int ground_row = win_row - num_rows_for_ground - 1;
    int difficulty = 0;
    int gameover = 0;

    // player parameters
    int player_row = ground_row;
    const int player_col = (int) (win_col / 5);
    int player_score = 0;
    int player_jumped = 0;
    int player_jump_timer = 0;

    // obstacle parameters
    const int obs_min_gen_gap = JUMP_WIDTH;
    int obs_max_gen_gap = GAME_LENGTH * obs_min_gen_gap;
    int obs_timer = random_int_between(obs_min_gen_gap, obs_max_gen_gap);
    int obs_col = win_col + OBSTACLE_CENTER_TO_EDGE;

    
    /*
    * Allocate and initiate game map, then insert player.
    */
    // allocate 2d char array to fill current window dimensions.
    char **map = malloc(win_row * sizeof(char *));
    if (!map)
    {
        fprintf(stderr, "Memory allocation error trying to allocate game map.\n");
        exit(EXIT_FAILURE);
    }
    for (int i = 0; i < win_row; i++)
    {
        map[i] = malloc(win_col);
        if (!map[i])
        {
            fprintf(stderr, "Memory allocation error trying to allocate game map.\n");
            exit(EXIT_FAILURE);
        }
        for (int j = 0; j < win_col; j++)
        {
            map[i][j] = ' ';
        }
    }
    // fill in sky region with random stars (start from top)
    for (int i = 0; i < num_rows_for_sky; i++)
    {
        for (int j = 0; j < win_col; j++)
        {
            map[i][j] = (rand() % 1000 < 10) ? '*' : ' ';
        }
    }
    // fill in ground region with ground pattern (start from bottom)
    for (int j = 0; j < win_col; j++)
    {
        map[win_row - num_rows_for_ground][j] = '#';
    }
    for (int i = win_row - num_rows_for_ground + 1; i < win_row - 1; i++)
    {
        for (int j = 0; j < win_col; j++)
        {
            map[i][j] = (rand() % 100 < 50) ? '.' : ',';
        }
    }

 

    /*
    * Configure the terminal to allow waiting for a keystroke during game loop to be non-blocking.
    * (!) Will cause issues on Windows because of ncurses.
    */
    initscr(); // Will cause terminal to automatically clear the screen
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
        cursor_to(0, 0);
        for (int i = 0; i < win_row; i++)
        {
            for (int j = 0; j < win_col; j++)
            {
                fprintf(stdout, "%c", map[i][j]);
            }
        }


        /*
         * Update map for next frame.
         */
        // shift all chars to the left to animate motion
        for (int i = 0; i < win_row - 1; i++)
        {
            for (int j = 0; j < win_col - 1; j++)
            {
                map[i][j] = map[i][j + 1];
            }
        }
        // erase map middle to redraw player and/or obstacle
        for (int i = num_rows_for_sky; i <= ground_row; i++)
        {
            for (int j = 0; j < win_col; j++)
            {
                map[i][j] = ' ';
            }
        }
        // add new column of random stars to sky region
        for (int i = 0; i < num_rows_for_sky; i++)
        {
            map[i][win_col - 1] = (rand() % 1000 < 10) ? '*' : ' ';
        }
        // add new column of ground pattern to ground region
        map[ground_row + 1][win_col - 1] = '#';
        for (int i = ground_row + 2; i < win_row - 1; i++)
        {
            map[i][win_col - 1] = (rand() % 100 < 50) ? '.' : ',';
        }


        /*
         * If space key is pressed, turn on player jump state to trigger jump animation.
         */
        if (key_pressed())
        {
            if (getch() == 32) // space bar was pressed
            {
                player_jumped = 1;
            }
        }


        /*
        * Update and display player score.
        */
        player_score += 2;

        char score_message[win_col];
        if (!sprintf(score_message, " SCORE: %d", player_score))
        {
            gameover = 1;
        }
        for (int i = 0; i < strlen(score_message); i++)
        {
            map[win_row - 1][i] = score_message[i];
        }


        /*
        * Update game difficulty based on player score.
        * GAME_MAX_DIFF_SCORE is assumed to be a large enough score for game to reach max difficulty.
        */
        if (player_score < GAME_MAX_DIFF_SCORE)
        {
            difficulty = (player_score * ((obs_min_gen_gap * GAME_LENGTH) - obs_min_gen_gap + 1)) / GAME_MAX_DIFF_SCORE;
        }


        /*
         * Draw obstacle while it enters and exits frame.
         */
        if (obs_timer == 0)
        {
            int num_visible_chars = 0; // number of columns obstacle still visible in

            // still entering
            if (obs_col >= win_col)
            {
                num_visible_chars = win_col - (obs_col - OBSTACLE_CENTER_TO_EDGE);
                for (int i = 0; i < OBSTACLE_LENGTH; i++)
                {
                    for (int j = 0; j < num_visible_chars; j++)
                    {
                        map[ground_row - i][win_col - num_visible_chars + j] = obstacle_char_map[i][j];
                    }
                }
            }

            // still exitting
            else if (obs_col - OBSTACLE_CENTER_TO_EDGE < 0)
            {
                num_visible_chars = obs_col + OBSTACLE_CENTER_TO_EDGE;
                for (int i = 0; i < OBSTACLE_LENGTH; i++)
                {
                    for (int j = 0; j < num_visible_chars; j++)
                    {
                        map[ground_row - i][j] = obstacle_char_map[i][OBSTACLE_WIDTH - num_visible_chars + j];
                    }
                }
            }

            else
            {
                for (int i = 0; i < OBSTACLE_LENGTH; i++)
                {
                    for (int j = 0; j < OBSTACLE_WIDTH; j++)
                    {
                        map[ground_row - i][obs_col - OBSTACLE_CENTER_TO_EDGE + j] = obstacle_char_map[i][j];
                    }
                }
            }

            // If obstacle still in frame, update its position
            if (obs_col + OBSTACLE_CENTER_TO_EDGE > 0)
            {
                obs_col--;
            }
            // Else, reset its parameters
            else
            {
                obs_timer = random_int_between(obs_min_gen_gap, obs_max_gen_gap - difficulty);
                obs_col = win_col + OBSTACLE_CENTER_TO_EDGE;
            }
        }
        else
        {
            obs_timer--;
        }


        /*
        * If player jumps, maintain player_row (y position) according to JUMP_HEIGHT and JUMP_AIRTIME.
        */
        if (player_jumped)
        {
            // ascending
            if (player_jump_timer < JUMP_HEIGHT)
            {
                player_row--;
            }
            // descending
            else if (player_jump_timer > JUMP_HEIGHT + JUMP_AIRTIME)
            {
                player_row++;
            }

            player_jump_timer++; // airtime

            // end of jump
            if (player_jump_timer == JUMP_WIDTH)
            {
                player_jumped = 0;
                player_jump_timer = 0;
                player_row = ground_row;
            }
        }


        /*
        * Draw player.
        */
        // Overwriting obstacle chars while drawing player signals game over
        if (
            map[player_row][player_col] == '#' ||
            map[player_row - 1][player_col] == '#' ||
            map[player_row - 1][player_col - 1] == '#' ||
            map[player_row - 1][player_col + 1] == '#' ||
            map[player_row - 2][player_col] == '#'
            )
        {
            gameover = 1;
        }
        // draw player piece by piece
        else
        {
            map[player_row][player_col] = (rand() % 2 == 0) ? 'W' : 'M'; // legs
            map[player_row - 1][player_col] = 'O'; // body?
            if (player_jumped) // raise arms when jumping
            {
                map[player_row - 2][player_col - 1] = '\\';
                map[player_row - 2][player_col + 1] = '/';
            }
            else
            {
                map[player_row - 1][player_col - 1] = '/';
                map[player_row - 1][player_col + 1] = '\\';
            }
            map[player_row - 2][player_col] = '@'; // head
        }

        /*
         * Sleep before next frame.
         */
        if (!gameover)
        {
            usleep(FRAME_DELAY);
        }
    }

    /*
    * Free map from memory.
    */
    for (int i = 0; i < win_row; i++)
    {
        free(map[i]);
    }
    free(map);


    /*
    * Reset terminal after ncurses.
    */
    endwin();

    clear_screen();
    fprintf(stdout, "Game over :(\nFinal Score: %d\n", player_score);
    return 0;
}
