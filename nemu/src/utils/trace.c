#include <common.h>
#include <string.h>
#include <trace.h>

#define IRINGBUF_SIZE 16
#define IRINGBUF_MAX_LEN 128

static char iringbuf[IRINGBUF_SIZE][IRINGBUF_MAX_LEN];
static int iring_cursor = 0;
static bool iring_full = false;

void iringbuf_write(const char *log) {
    strncpy(iringbuf[iring_cursor], log, IRINGBUF_MAX_LEN -1);
    iringbuf[iring_cursor][IRINGBUF_MAX_LEN - 1] = '\0';

    iring_cursor = (iring_cursor + 1) % IRINGBUF_SIZE;

    if (iring_cursor == 0) {
        iring_full = true;
    }
}

void iringbuf_print() {
    printf("========== Instruction Ring Buffer ==========\n");
    if (!iring_full && iring_cursor == 0) {
        printf("Buffer is empty.\n");
        return;
    }
    int start = iring_full ? iring_cursor : 0;
    int count = iring_full ? IRINGBUF_SIZE : iring_cursor;
    
    for (int i = 0; i < count; i++) {
        int index = (start + i) % IRINGBUF_SIZE;

        if (i == count - 1) {
            printf("    -->%s\n", iringbuf[index]);
        } else {
            printf("       %s\n", iringbuf[index]);
        }
    }

    printf("=============================================\n");
}