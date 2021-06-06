#define  _GNU_SOURCE // Otherwise we get a warning about getline()
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <pthread.h>
#include <ncurses.h>
#include "../src/KVS-lib.h"
#include "../src/list.h"
#define MAX_KEY_SIZE 1024
#define MAX_VALUE_SIZE 65536

static pthread_mutex_t print_mutex;
WINDOW *txtwindow, *owindow;

void print_usage(char **argv) 
{
    printf("Usage: %s USERNAME GROUPID SECRET\n", argv[0]);
    exit(EXIT_FAILURE);
}

void stop(int code)
{
    close_connection();
    delwin(txtwindow);
    endwin();
    exit(code);
}

void update()
{
    int y, x, r, c;
    getyx(txtwindow, y, x);
    getmaxyx(txtwindow, r, c);

    if (y >= r)
        scroll(txtwindow);
}

void new_message(char *value)
{
    int y, x;
    
    pthread_mutex_lock(&print_mutex);
    getyx(owindow, y, x);
    waddstr(txtwindow, value);
    update();
    waddstr(txtwindow, "\n");
    update();
    wmove(owindow, y, x);
    wrefresh(txtwindow);
    pthread_mutex_unlock(&print_mutex);
}

int main(int argc, char **argv)
{
    if (argc != 4)
        print_usage(argv);

    char *username = argv[1], *groupid = argv[2], *secret = argv[3];
    int result;

    if (strlen(username) >= 64) {
        printf("Usernames must be 63 characters long at most\n");
        exit(EXIT_FAILURE);
    }

    initscr();
    txtwindow = newwin(LINES-1, 0, 0, 0);
    owindow = newwin(1, 0, LINES-1, 0);
    scrollok(txtwindow, 1);

    addstr("Connecting to chat server... ");
    refresh();
    result = establish_connection(groupid, secret);
    if (result == 1) {
        addstr("Done!\n");
        refresh();
    }
    else {
        printw("Error connecting (%d)\n", result);
        refresh();
        stop(EXIT_FAILURE);
    }

    register_callback("msg", new_message);

    char *line = NULL;
    int size = 0;
    char running = 1;
    int r, c;
    while (running) {
        getmaxyx(stdscr, r, c);

        pthread_mutex_lock(&print_mutex);
        wmove(owindow, 0, 0);
        wclrtoeol(owindow);
        wprintw(owindow, ">>> ");
        wrefresh(owindow);
        pthread_mutex_unlock(&print_mutex);

        if (size < c) {
            line = (char*) realloc(line, sizeof(char)*(c+1));
            line[c] = '\0';
            size = c;
        }
        int read = wgetnstr(owindow, line, c);
        if (strcmp(line, "exit") != 0 && strcmp(line, "") != 0) {
            pthread_mutex_lock(&print_mutex);
            wmove(owindow, 0, 0);
            wclrtoeol(owindow);
            refresh();
            pthread_mutex_unlock(&print_mutex);

            char *msg = (char*) malloc(sizeof(char)*(strlen(username) + strlen(line) + 1));
            sprintf(msg, "[%s] %s", username, line);
            put_value("msg", msg);
            free(msg);
        }
        else if (strcmp(line, "exit") == 0)
            break;
    }
    free(line);

    stop(EXIT_SUCCESS);
}