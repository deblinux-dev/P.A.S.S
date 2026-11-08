#include <stdio.h>
#include <string.h>
volatile int dummy = 0;
int main(void) {
    char buf[128];
    printf("A[%d]", 42);
    printf("B[%s]", "str");
    printf("C[%5d|%-5d]", 7, 8);
    printf("D[%x|%X|%u]", 255, 255, 3000000000u);
    printf("E[%c|%c]", 'x', 'y');
    sprintf(buf, "F[%ld|%zu]", 123456789L, (size_t)999);
    fputs(buf, stdout);
    printf("G[%g|%e]", 1.5, 2.25);
    printf("H[%p]", (void*)0x1234);
    snprintf(buf, sizeof buf, "I[%*d|%.*s]", 6, 5, 3, "abcdef");
    fputs(buf, stdout);
    /* ifunc-тяжёлые: */
    char a[64]; memset(a, 'q', 60); a[60] = 0;
    printf("J[%zu|%d]", strlen(a), memcmp(a, "qqqq", 4));
    printf("K[%s]", strchr("hello", 'l'));
    dummy ^= (int)strlen(buf);
    printf("\nEND\n");
    return 0;
}
