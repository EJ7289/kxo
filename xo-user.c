#include <fcntl.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include "game.h"
#include "moves.h"

#define XO_STATUS_FILE "/sys/module/kxo/initstate"
#define XO_DEVICE_FILE "/dev/kxo"
#define XO_DEVICE_ATTR_FILE "/sys/class/kxo/kxo/kxo_state"

static bool status_check(void)
{
    FILE *fp = fopen(XO_STATUS_FILE, "r");
    if (!fp) {
        printf("kxo status : not loaded\n");
        return false;
    }

    char read_buf[20];
    fgets(read_buf, 20, fp);
    read_buf[strcspn(read_buf, "\n")] = 0;
    if (strcmp("live", read_buf)) {
        printf("kxo status : %s\n", read_buf);
        fclose(fp);
        return false;
    }
    fclose(fp);
    return true;
}

static void attr_fd_orig(void)
{
    int attr_fd = open(XO_DEVICE_ATTR_FILE, O_RDWR);
    if (attr_fd < 0)
        perror("Open attribute file error");
    else {
        char *buf = "1 1 0\n";
        write(attr_fd, buf, 6);
        // printf("final attr_fd = %s", buf);
    }
}

static struct termios orig_termios;

static void raw_mode_disable(void)
{
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
    attr_fd_orig();
}

static void raw_mode_enable(void)
{
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(raw_mode_disable);
    struct termios raw = orig_termios;
    raw.c_iflag &= ~(IXON);
    raw.c_lflag &= ~(ECHO | ICANON);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

static bool read_attr, end_attr;

static void listen_keyboard_handler(void)
{
    int attr_fd = open(XO_DEVICE_ATTR_FILE, O_RDWR);
    if (attr_fd < 0) {
        perror("Error");
        printf("Can't open device attribute file: %s, error:%d\n",
               XO_DEVICE_ATTR_FILE, attr_fd);
    }
    char input;

    if (read(STDIN_FILENO, &input, 1) == 1) {
        char buf[20];
        switch (input) {
        case 16: /* Ctrl-P */
            read(attr_fd, buf, 6);
            buf[0] = (buf[0] - '0') ? '0' : '1';
            read_attr ^= 1;
            write(attr_fd, buf, 6);
            if (!read_attr)
                printf("Stopping to display the chess board...\n");
            break;
        case 17: /* Ctrl-Q */
            read(attr_fd, buf, 6);
            buf[4] = '1';
            read_attr = false;
            end_attr = true;
            write(attr_fd, buf, 6);
            printf("Stopping the kernel space tic-tac-toe game...\n\n");

            display_moves();
            break;
        }
    }
    close(attr_fd);
}

/* Draw the board into draw_buffer */
int draw_board(char *table, char *draw_buffer)
{
    int i = 0, k = 0;
    draw_buffer[i++] = '\n';
    draw_buffer[i++] = '\n';

    while (i < DRAWBUFFER_SIZE) {
        for (int j = 0; j < (BOARD_SIZE << 1) - 1 && k < N_GRIDS; j++) {
            draw_buffer[i++] = j & 1 ? '|' : table[k++];
        }
        draw_buffer[i++] = '\n';
        for (int j = 0; j < (BOARD_SIZE << 1) - 1; j++) {
            draw_buffer[i++] = '-';
        }
        draw_buffer[i++] = '\n';
    }
    return 0;
}

// display the current time

void display_time(void)
{
    time_t current_time;
    const struct tm *time_info;
    char time_string[50];

    // Get the current time in seconds since the Epoch
    time(&current_time);

    // Convert the time to local time
    time_info = localtime(&current_time);

    // Format the time into a string
    strftime(time_string, sizeof(time_string), "%Y-%m-%d %H:%M:%S", time_info);

    // Print the formatted time
    printf("Current time: %s\n", time_string);
}



int main(int argc, char *argv[])
{
    if (!status_check())
        exit(1);

    raw_mode_enable();
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);

    char table_move[N_GRIDS + 2];
    char table_buf[N_GRIDS];
    char display_buf[DRAWBUFFER_SIZE + 1];

    char char_move;
    char game_win;

    fd_set readset;
    int device_fd = open(XO_DEVICE_FILE, O_RDONLY);
    if (device_fd < 0) {
        perror("Error");
        printf("Can't open device file: %s, error:%d\n", XO_DEVICE_FILE,
               device_fd);
        exit(1);
    }

    int max_fd = device_fd > STDIN_FILENO ? device_fd : STDIN_FILENO;
    read_attr = true;
    end_attr = false;

    while (!end_attr) {
        FD_ZERO(&readset);
        FD_SET(STDIN_FILENO, &readset);
        FD_SET(device_fd, &readset);

        int result = select(max_fd + 1, &readset, NULL, NULL, NULL);
        if (result < 0) {
            printf("Error with select system call\n");
            exit(1);
        }

        if (FD_ISSET(STDIN_FILENO, &readset)) {
            FD_CLR(STDIN_FILENO, &readset);
            listen_keyboard_handler();
        } else if (read_attr && FD_ISSET(device_fd, &readset)) {
            FD_CLR(device_fd, &readset);

            printf("\033[H\033[J"); /* ASCII escape code to clear the screen */
            read(device_fd, table_move, N_GRIDS + 2);

            memcpy(table_buf, table_move, N_GRIDS);
            memcpy(&char_move, table_move + N_GRIDS, 1);
            memcpy(&game_win, table_move + N_GRIDS + 1, 1);

            draw_board(table_buf, display_buf);
            display_buf[DRAWBUFFER_SIZE] = '\0';
            printf("%s", display_buf);

            display_time();
            record_move(char_move, game_win);
        }
    }

    raw_mode_disable();
    fcntl(STDIN_FILENO, F_SETFL, flags);

    close(device_fd);

    return 0;
}
