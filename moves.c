#include "moves.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "game.h"


// ----------- Append Buffer -------------- //
struct abuf {
    char *buf;
    int len;
};

#define ABUF_INIT {NULL, 0};

void abAppend(struct abuf *ab, const char *s, int len)
{
    char *new = realloc(ab->buf, ab->len + len);
    if (new == NULL)
        return;

    memcpy(&new[ab->len], s, len);
    ab->buf = new;
    ab->len += len;
}

void abFree(struct abuf *ab)
{
    free(ab->buf);
}

// -------------------------------------------- //

// --------------- save moves to file ---------------- //
int move_record[N_GRIDS];
int count = 0;

void move_to_file(int *arr)
{
    // move steps become a string
    struct abuf ab = ABUF_INIT;
    abAppend(&ab, "Moves: ", 7);
    for (int i = 0; i < count; i++) {
        char row = 'A' + GET_ROW(move_record[i]);
        char col = '1' + GET_COL(move_record[i]);
        abAppend(&ab, &row, sizeof(row));
        abAppend(&ab, &col, sizeof(col));

        if (i != (count - 1))
            abAppend(&ab, " -> ", 4);
    }
    abAppend(&ab, "\n", 2);

    // save to file
    FILE *fptr = fopen("file.txt", "a");
    fprintf(fptr, "%s", ab.buf);
    fclose(fptr);

    abFree(&ab);
}

void record_move(char move, char win)
{
    if ((count == 0) || ((move != move_record[count - 1]) && (win == ' '))) {
        move_record[count++] = (int) move;
    } else if (win == 'X' || win == 'O') {
        move_to_file(move_record);
        count = 0;
    }
}
// ------------------------------------------------- //