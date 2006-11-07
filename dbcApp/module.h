#ifndef INCmoduleh
#define INCmoduleh

#include "DbC.h"

extern void unchecked_testv(int x, int y);
extern int unchecked_testi(int x, int y);

#define testv(x,y)\
    assertPre((x) == 0 || (y) == 0,\
        testv(x,y)\
    )

#define testi(x,y)\
    assertPre((x) != 0 && (y) != 0,\
        testi(x,y)\
    )

#endif
