#include "module.h"

int main () {
    int x = 0, y = 0;
    int z;
#ifdef TESTV
    unchecked_testv(x+0,y+0);
    unchecked_testv(x+0,y+1);
    unchecked_testv(x+1,y+0);
    unchecked_testv(x+1,y+1);
    testv(x+0,y+0);
    testv(x+0,y+1);
    testv(x+1,y+0);
    testv(x+1,y+1);          /* precondition violated here */
    printf("this should not appear\n");
#endif
#ifdef TESTI
    testi(x+1,y+1);
    printf("result of test2 is %d\n", z);
    z = testi(x+0,y+1);     /* precondition violated here */
    printf("this should not appear\n");
    printf("result of test2 is %d\n", z);
#endif
}
