/****************************************************************************
 * sudoku.c
 *
 * CS50 AP
 * Problem 4-0
 *
 * Implements the game of Sudoku.
 ***************************************************************************/

#include "sudoku.h"

#define _XOPEN_SOURCE 500

#include <ctype.h>
#include <ncurses.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>


// macro for processing control characters
#define CTRL(x) ((x) & ~0140)

// size of each int (in bytes) in *.bin files
#define INTSIZE 4


// wrapper for our game's globals
struct
{
    // the current level
    char *level;

    // the game's board
    int board[9][9];
    
    // the game's starting numbers
    bool startBoard[9][9];

    // the board's number
    int number;

    // the board's top-left coordinates
    int top, left;

    // the cursor's current location between (0,0) and (8,8)
    int y, x;
    
    int lastChange[3];
} g;

bool boolWon = false;

// prototypes
void draw_grid(void);
void draw_borders(void);
void draw_logo(void);
void draw_numbers(void);
void hide_banner(void);
bool load_board(void);
void handle_signal(int signum);
void log_move(int ch);
void redraw_all(void);
bool restart_game(void);
void show_banner(char *b);
void show_cursor(void);
void shutdown(void);
bool startup(void);
void determine_start_board(void);
bool game_won(void);
void error_check(void);
void show_error_banner(char *b);
void hide_error_banner(void);


/*
 * Main driver for the game.
 */

int main(int argc, char *argv[])
{
    // ensure that number of arguments is as expected
    if (argc != 2 && argc != 3)
    {
        fprintf(stderr, "Usage: sudoku n00b|l33t [#]\n");
        return 1;
    }

    // ensure that level is valid
    if (strcmp(argv[1], "debug") == 0)
        g.level = "debug";
    else if (strcmp(argv[1], "n00b") == 0)
        g.level = "n00b";
    else if (strcmp(argv[1], "l33t") == 0)
        g.level = "l33t";
    else
    {
        fprintf(stderr, "Usage: sudoku n00b|l33t [#]\n");
        return 2;
    }

    // n00b and l33t levels have 1024 boards; debug level has 9
    int max = (strcmp(g.level, "debug") == 0) ? 9 : 1024;

    // ensure that #, if provided, is in [1, max]
    if (argc == 3)
    {
        // ensure n is integral
        char c;
        if (sscanf(argv[2], " %d %c", &g.number, &c) != 1)
        {
            fprintf(stderr, "Usage: sudoku n00b|l33t [#]\n");
            return 3;
        }

        // ensure n is in [1, max]
        if (g.number < 1 || g.number > max)
        {
            fprintf(stderr, "That board # does not exist!\n");
            return 4;
        }

        // seed PRNG with # so that we get same sequence of boards
        srand(g.number);
    }
    else
    {
        // seed PRNG with current time so that we get any sequence of boards
        srand(time(NULL));

        // choose a random n in [1, max]
        g.number = rand() % max + 1;
    }

    // start up ncurses
    if (!startup())
    {
        fprintf(stderr, "Error starting up ncurses!\n");
        return 5;
    }

    // register handler for SIGWINCH (SIGnal WINdow CHanged)
    signal(SIGWINCH, (void (*)(int)) handle_signal);

    // start the first game
    if (!restart_game())
    {
        shutdown();
        fprintf(stderr, "Could not load board from disk!\n");
        return 6;
    }
    redraw_all();

    // let the user play!
    int ch;
    do
    {
        // refresh the screen
        refresh();

        // get user's input
        ch = getch();

        // capitalize input to simplify cases
        ch = toupper(ch);
        
        bool redrawNums;
        
        // process user's input
        switch (ch)
        {
            // start a new game
            case 'N': 
                g.number = rand() % max + 1;
                if (!restart_game())
                {
                    shutdown();
                    fprintf(stderr, "Could not load board from disk!\n");
                    return 6;
                }
                break;

            // restart current game
            case 'R': 
                if (!restart_game())
                {
                    shutdown();
                    fprintf(stderr, "Could not load board from disk!\n");
                    return 6;
                }
                break;
            
            // undo the last modification
            case 'U':
            case CTRL('z'):
                if (g.lastChange[0] != -1) 
                {
                    g.y = g.lastChange[1];
                    g.x = g.lastChange[2];
                    g.board[g.lastChange[1]][g.lastChange[2]] = g.lastChange[0];
                    redrawNums = true;
                }
                break;

            // let user manually redraw screen with ctrl-L
            case CTRL('l'):
                redraw_all();
                break;
            
            
            // cases for board movement
            case KEY_UP:
                if (g.y > 0)
                    g.y--;
                else
                    g.y = 8;
                break;
            
            case KEY_DOWN:
                if (g.y < 8)
                    g.y++;
                else
                    g.y = 0;
                break;
            
            case KEY_LEFT:
                if (g.x > 0)
                    g.x--;
                else
                    g.x = 8;
                break;
            
            case KEY_RIGHT:
                if (g.x < 8)
                    g.x++;
                else
                    g.x = 0;
                break;
            
            // case for clearing space
            case KEY_BACKSPACE:
            case KEY_DC:
            case '.':
            case '0':
                g.lastChange[1] = g.y;
                g.lastChange[2] = g.x;
                g.lastChange[0] = g.board[g.y][g.x];
                g.board[g.y][g.x] = 0;
                redrawNums = true;
                break;
            
            
            // default case for checking if a number
            default:
                if (isdigit(ch) && !g.startBoard[g.y][g.x]) 
                {
                    g.lastChange[1] = g.y;
                    g.lastChange[2] = g.x;
                    g.lastChange[0] = g.board[g.y][g.x];
                    g.board[g.y][g.x] = ch - '0';
                    redrawNums = true;
                }
        }
        
        // log input (and board's state) if any was received this iteration
        if (ch != ERR) 
        {
            log_move(ch);
            hide_error_banner();
            error_check();
            if (redrawNums) 
            {
                draw_numbers();
            }
            show_cursor();
            
            if (game_won()) 
            {
                boolWon = true;
                // draw nums in green
                draw_numbers();
                show_cursor();
                usleep(20000);
                break;
            }
        }
    }
    while (ch != 'Q');

    // shut down ncurses
    shutdown();

    // tidy up the screen (using ANSI escape sequences)
    printf("\033[2J");
    printf("\033[%d;%dH", 0, 0);

    // that's all folks
    if (boolWon) 
    {
        printf("\nYOU WIN!\n\n");
        return 1;
    } 
    else 
    {
        printf("\nkthxbai!\n\n");
        return 0;
    }
}

/*
* Checks if the user entered in an incorrect number
*/

void error_check()
{
    // checks to see if the current tile is 0
    if (g.board[g.y][g.x] == 0) 
    {
        hide_error_banner();
        return;
    }
    
    // checks to see if the number has a duplicate in the row
    for (int i = 0; i < 9; i++)
    {
        if (g.board[g.y][i] == g.board[g.y][g.x] && i != g.x)
        {
            show_error_banner("This number already exists in this row");
            return;
        }
    }
    
    // checks to see if the number has a duplicate in the column
    for (int i = 0; i < 9; i++)
    {
        if (g.board[i][g.x] == g.board[g.y][g.x] && i != g.y) 
        {
            show_error_banner("This number already exists in this column");
            return;
        }
    }
    
    // calculates the board section on x and y from 1 to 3
    int boardSectionX = (int) ((float) g.x / 3 + 1);
    int boardSectionY = (int) ((float) g.y / 3 + 1);
    
    // calculates the center location of the board section
    int centerBoardX = boardSectionX * 3 - 2;
    int centerBoardY = boardSectionY * 3 - 2;
    
    // check if the number has duplicates within the box
    for (int i = centerBoardY - 1; i < centerBoardY + 2; i++) 
    {
        for (int j = centerBoardX - 1; j < centerBoardX + 2; j++) 
        {
            if (g.board[i][j] == g.board[g.y][g.x] && !(i == g.y && j == g.x)) 
            {
                show_error_banner("This number already exists in this box");
                return;
            }
        }
    }
}

void show_error_banner(char *b)
{
    // enable color if possible
    if (has_colors())
        attron(COLOR_PAIR(PAIR_BANNER));

    // determine where top-left corner of board belongs 
    mvaddstr(g.top + 5, 0, b);

    // disable color if possible
    if (has_colors())
        attroff(COLOR_PAIR(PAIR_BANNER));
}

void hide_error_banner(void)
{
    // get window's dimensions
    int maxy, maxx;
    getmaxyx(stdscr, maxy, maxx);

    // overwrite banner with spaces
    for (int i = 0; i < 41; i++)
        mvaddch(g.top + 5, i, ' ');
}

/*
 * Checks if the game was won
 */

bool game_won()
{
    for (int i = 0; i < 9; i++)
    {
        bool checkBool[9];
        for (int j = 0; j < 9; j++)
        {
            // sets the checkbool element of the board to true;
            if (checkBool[g.board[j][i]] == true || g.board[j][i] == 0)
            {
                return false;
            }
            else
            {
                checkBool[g.board[j][i]] = true;
            }
        }
    }
    return true;
}

/*
 * Determines the starting grid
 */

void determine_start_board(void)
{
    for (int i = 0; i < 9; i++)
    {
        for (int j = 0; j < 9; j++)
        {
            if (g.board[i][j] != 0)
            {
                g.startBoard[i][j] = true;
            }
        }
    }    
}

/*
 * Draw's the game's board.
 */

void draw_grid(void)
{
    // get window's dimensions
    int maxy, maxx;
    getmaxyx(stdscr, maxy, maxx);

    // determine where top-left corner of board belongs 
    g.top = maxy / 2 - 7;
    g.left = maxx / 2 - 30;

    // enable color if possible
    if (has_colors())
        attron(COLOR_PAIR(PAIR_GRID));

    // print grid
    for (int i = 0 ; i < 3 ; i++)
    {
        mvaddstr(g.top + 0 + 4 * i, g.left, "+-------+-------+-------+");
        mvaddstr(g.top + 1 + 4 * i, g.left, "|       |       |       |");
        mvaddstr(g.top + 2 + 4 * i, g.left, "|       |       |       |");
        mvaddstr(g.top + 3 + 4 * i, g.left, "|       |       |       |");
    }
    mvaddstr(g.top + 4 * 3, g.left, "+-------+-------+-------+" );

    // remind user of level and #
    char reminder[maxx + 1];
    sprintf(reminder, "   playing %s #%d", g.level, g.number);
    mvaddstr(g.top + 14, g.left + 25 - strlen(reminder), reminder);

    // disable color if possible
    if (has_colors())
        attroff(COLOR_PAIR(PAIR_GRID));
}

/*
 * Draws game's borders.
 */

void draw_borders(void)
{
    // get window's dimensions
    int maxy, maxx;
    getmaxyx(stdscr, maxy, maxx);

    // enable color if possible (else b&w highlighting)
    if (has_colors())
    {
        attron(A_PROTECT);
        attron(COLOR_PAIR(PAIR_BORDER));
    }
    else
        attron(A_REVERSE);

    // draw borders
    for (int i = 0; i < maxx; i++)
    {
        mvaddch(0, i, ' ');
        mvaddch(maxy - 1, i, ' ');
    }

    // draw header
    char header[maxx + 1];
    sprintf(header, "%s by %s", TITLE, AUTHOR);
    mvaddstr(0, (maxx - strlen(header)) / 2, header);

    // draw footer
    mvaddstr(maxy - 1, 1, "[N]ew Game   [R]estart Game");
    mvaddstr(maxy - 1, maxx - 13, "[Q]uit Game");

    // disable color if possible (else b&w highlighting)
    if (has_colors())
        attroff(COLOR_PAIR(PAIR_BORDER));
    else
        attroff(A_REVERSE);
}


/*
 * Draws game's logo.  Must be called after draw_grid has been
 * called at least once.
 */

void draw_logo(void)
{
    // determine top-left coordinates of logo
    int top = g.top + 2;
    int left = g.left + 30;

    // enable color if possible
    if (has_colors())
        attron(COLOR_PAIR(PAIR_LOGO));

    // draw logo
    mvaddstr(top + 0, left, "               _       _          ");
    mvaddstr(top + 1, left, "              | |     | |         ");
    mvaddstr(top + 2, left, " ___ _   _  __| | ___ | | ___   _ ");
    mvaddstr(top + 3, left, "/ __| | | |/ _` |/ _ \\| |/ / | | |");
    mvaddstr(top + 4, left, "\\__ \\ |_| | (_| | (_) |   <| |_| |");
    mvaddstr(top + 5, left, "|___/\\__,_|\\__,_|\\___/|_|\\_\\\\__,_|"); // sepookoo

    // sign logo
    char signature[3 + strlen(AUTHOR) + 1];
    sprintf(signature, "by %s", AUTHOR);
    mvaddstr(top + 7, left + 35 - strlen(signature) - 1, signature);

    // disable color if possible
    if (has_colors())
        attroff(COLOR_PAIR(PAIR_LOGO));
}


/*
 * Draw's game's numbers.  Must be called after draw_grid has been
 * called at least once.
 */

void draw_numbers(void)
{
    // enable color if possible
    if (has_colors())
        attron(COLOR_PAIR(PAIR_DIGITS));
        
    // iterate over board's numbers
    for (int i = 0; i < 9; i++)
    {
        for (int j = 0; j < 9; j++)
        {
            if (g.startBoard[i][j]) 
            {
                if (has_colors())
                    attron(COLOR_PAIR(START_DIGITS));
            } 
            else if (boolWon) 
            {
                if (has_colors())
                    attron(COLOR_PAIR(WON_DIGITS));
            }
            else 
            {
                if (has_colors())
                    attron(COLOR_PAIR(PAIR_DIGITS));
            }
            // determine char
            char c = (g.board[i][j] == 0) ? '.' : g.board[i][j] + '0';
            mvaddch(g.top + i + 1 + i / 3, g.left + 2 + 2 * (j + j / 3), c);
            refresh();
        }
    }
    
    // disable color if possible
    if (has_colors())
        attroff(COLOR_PAIR(PAIR_BANNER));
}


/*
 * Designed to handles signals (e.g., SIGWINCH).
 */

void handle_signal(int signum)
{
    // handle a change in the window (i.e., a resizing)
    if (signum == SIGWINCH)
        redraw_all();

    // re-register myself so this signal gets handled in future too
    signal(signum, (void (*)(int)) handle_signal);
}


/*
 * Hides banner.
 */

void hide_banner(void)
{
    // get window's dimensions
    int maxy, maxx;
    getmaxyx(stdscr, maxy, maxx);

    // overwrite banner with spaces
    for (int i = 0; i < maxx; i++)
        mvaddch(g.top + 16, i, ' ');
}


/*
 * Loads current board from disk, returning true iff successful.
 */

bool load_board(void)
{
    // open file with boards of specified level
    char filename[strlen(g.level) + 5];
    sprintf(filename, "%s.bin", g.level);
    FILE *fp = fopen(filename, "rb");
    if (fp == NULL)
        return false;

    // determine file's size
    fseek(fp, 0, SEEK_END);
    int size = ftell(fp);

    // ensure file is of expected size
    if (size % (81 * INTSIZE) != 0)
    {
        fclose(fp);
        return false;
    }

    // compute offset of specified board
    int offset = ((g.number - 1) * 81 * INTSIZE);

    // seek to specified board
    fseek(fp, offset, SEEK_SET);

    // read board into memory
    if (fread(g.board, 81 * INTSIZE, 1, fp) != 1)
    {
        fclose(fp);
        return false;
    }

    // fills a 2D boolean array with true for computer filled slots
    determine_start_board();
    g.lastChange[0] = -1;

    // w00t
    fclose(fp);
    return true;
}


/*
 * Logs input and board's state to log.txt to facilitate automated tests.
 */

void log_move(int ch)
{
    // open log
    FILE *fp = fopen("log.txt", "a");
    if (fp == NULL)
        return;

    // log input
    fprintf(fp, "%d\n", ch);

    // log board
    for (int i = 0; i < 9; i++)
    {
        for (int j = 0; j < 9; j++)
            fprintf(fp, "%d", g.board[i][j]);
        fprintf(fp, "\n");
    }

    // that's it
    fclose(fp);
}


/*
 * (Re)draws everything on the screen.
 */

void redraw_all(void)
{
    // reset ncurses
    endwin();
    refresh();

    // clear screen
    clear();

    // re-draw everything
    draw_borders();
    draw_grid();
    draw_logo();
    draw_numbers();

    // show cursor
    show_cursor();
}


/*
 * (Re)starts current game, returning true iff succesful.
 */

bool restart_game(void)
{
    // reload current game
    if (!load_board())
        return false;

    // redraw board
    draw_grid();
    draw_numbers();

    // get window's dimensions
    int maxy, maxx;
    getmaxyx(stdscr, maxy, maxx);

    // move cursor to board's center
    g.y = g.x = 4;
    show_cursor();

    // remove log, if any
    remove("log.txt");

    // w00t
    return true;
}


/*
 * Shows cursor at (g.y, g.x).
 */

void show_cursor(void)
{
    // restore cursor's location
    move(g.top + g.y + 1 + g.y / 3, g.left + 2 + 2 * (g.x + g.x / 3));
}


/*
 * Shows a banner.  Must be called after show_grid has been
 * called at least once.
 */

void show_banner(char *b)
{
    // enable color if possible
    if (has_colors())
        attron(COLOR_PAIR(PAIR_BANNER));

    // determine where top-left corner of board belongs 
    mvaddstr(g.top + 16, g.left + 64 - strlen(b), b);

    // disable color if possible
    if (has_colors())
        attroff(COLOR_PAIR(PAIR_BANNER));
}


/*
 * Shuts down ncurses.
 */

void shutdown(void)
{
    endwin();
}


/*
 * Starts up ncurses.  Returns true iff successful.
 */

bool startup(void)
{
    // initialize ncurses
    if (initscr() == NULL)
        return false;

    // prepare for color if possible
    if (has_colors())
    {
        // enable color
        if (start_color() == ERR || attron(A_PROTECT) == ERR)
        {
            endwin();
            return false;
        }

        // initialize pairs of colors
        if (init_pair(PAIR_BANNER, FG_BANNER, BG_BANNER) == ERR || 
            init_pair(PAIR_GRID, FG_GRID, BG_GRID) == ERR || 
            init_pair(PAIR_BORDER, FG_BORDER, BG_BORDER) == ERR || 
            init_pair(PAIR_LOGO, FG_LOGO, BG_LOGO) == ERR || 
            init_pair(PAIR_DIGITS, FG_DIGITS, BG_DIGITS) == ERR || 
            init_pair(START_DIGITS, FG_DIGITS_START, BG_DIGITS_START) == ERR || 
            init_pair(WON_DIGITS, FG_DIGITS_WON, BG_DIGITS_WON) == ERR)
        {
            endwin();
            return false;
        }
    }

    // don't echo keyboard input
    if (noecho() == ERR)
    {
        endwin();
        return false;
    }

    // disable line buffering and certain signals
    if (raw() == ERR)
    {
        endwin();
        return false;
    }

    // enable arrow keys
    if (keypad(stdscr, true) == ERR)
    {
        endwin();
        return false;
    }

    // wait 1000 ms at a time for input
    timeout(1000);

    // w00t
    return true;
}
