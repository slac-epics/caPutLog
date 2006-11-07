#include "module.h"

void unchecked_testv(int x, int y) {
    printf("testv(%d,%d) called\n", x, y);
}

int unchecked_testi(int x, int y) {
    printf("testi(%d,%d) called\n", x, y);
    return x+y;
}
