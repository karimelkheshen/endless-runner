#include <stdlib.h>
#include <time.h>
#include <string.h> /* Only for strlen() */

#include "util.h"


#define MIN_WIN_ROW 30
#define MIN_WIN_COL 70

#ifdef _WIN32
    #define FRAME_DELAY 10
#else
    #define FRAME_DELAY 14
#endif

#define JUMP_HEIGHT 10 // number of frames player needs to reach max jump height
#define JUMP_AIRTIME 8 // number of frames player hovers for when JUMP_HEIGHT is reached
#define JUMP_WIDTH 2 * JUMP_HEIGHT + JUMP_AIRTIME // total number of frames for player jump

#define OBSTACLE_LENGTH 6
#define OBSTACLE_WIDTH 11
#define OBSTACLE_CENTER_TO_EDGE 5 // number of columns half of obstacle takes up

#define GAME_LENGTH 4 // see declaration of obs_max_gen_gap
#define GAME_MAX_DIFF_SCORE 4000 // see updating game difficulty in game loop


int random_int_between(int upper, int lower)
{
    return rand() % (upper + 1 - lower) + lower;
}



int main (void)
{
    /*
     * Get and check window dimensions.
     */
    int win_row, win_col;
    get_terminal_window_dimensions(&win_row, &win_col);
    
    if (win_row < MIN_WIN_ROW || win_col < MIN_WIN_COL)
    {
        fprintf(stdout, "Terminal window size too small to render on.\n");
        fprintf(stdout, "Must be larger than %dx%d.\n", MIN_WIN_ROW, MIN_WIN_COL);
        exit(EXIT_SUCCESS);
    }


    /*
    * Buffer output for smoother drawing.
    */
    int BUFFER_SIZE = win_row * win_col;
    char new_buffer[BUFFER_SIZE];
    for (int i = 0; i < BUFFER_SIZE; i++)
    {
        new_buffer[i] = ' ';
    }
    setvbuf(stdout, new_buffer, _IOFBF, BUFFER_SIZE);
    

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
    // environment parameters
    int num_rows_for_sky = (int) ((win_row / 2) - (win_row / 9));
    int num_rows_for_ground = (int) (win_row / 6);
    int ground_row = win_row - num_rows_for_ground - 1;
    int difficulty = 0;
    int gameover = 0;

    // player parameters
    int player_row = ground_row;
    int player_col = (int) (win_col / 5);
    int player_score = 0;
    int player_is_jumping = 0;
    int player_jump_timer = 0;

    // obstacle parameters
    int obs_min_gen_gap = JUMP_WIDTH;
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
     * If called from ncurses-compatible terminal, configure the terminal to make keystroke detection during game loop non-blocking.
     * https://stackoverflow.com/questions/4025891/create-a-function-to-check-for-key-press-in-unix-using-ncurses
     */
    #ifndef _WIN32
        initscr(); // Will cause terminal to automatically clear the screen
        cbreak();
        noecho();
        nodelay(stdscr, TRUE);
        scrollok(stdscr, TRUE);
    #endif


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


        if (space_key_pressed())
        {
            player_is_jumping = 1;
        }


        /*
         * Update and display player score.
         */
        player_score += 2;

        char score_message[win_col];
        sprintf(score_message, " SCORE: %d", player_score);
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
        // obstacle is currently in frame
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

            // midway
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

            // If obstacle hasn't exitted, update its position
            if (obs_col + OBSTACLE_CENTER_TO_EDGE > 0)
            {
                obs_col--;
            }
            // Else, reset its parameters.
            else
            {
                obs_timer = random_int_between(obs_min_gen_gap, obs_max_gen_gap - difficulty);
                obs_col = win_col + OBSTACLE_CENTER_TO_EDGE;
            }
        }
        // obstacle hasn't been generated yet, update timer
        else
        {
            obs_timer--;
        }


        /*
         * If player jumps, maintain player_row (y position) according to JUMP_HEIGHT and JUMP_AIRTIME.
         */
        if (player_is_jumping)
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
                player_is_jumping = 0;
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
            if (player_is_jumping) // raise arms when jumping
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
            fflush(stdout);
            sleep_for_millis(FRAME_DELAY);
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
     * If Unix terminal, reset previous config after ncurses.
     */
    #ifndef _WIN32
        endwin();
    #endif

    
    /*
     * Clear screen.
     */
    cursor_to(0, 0);
    for (int i = 0; i < win_row; i++)
    {
        for (int j = 0; j < win_col; j++)
        {
            fprintf(stdout, " ");
        }
    }
    cursor_to(0, 0);
    
    fprintf(stdout, "Game over :(\nFinal Score: %d\n", player_score);

    return 0;
}
