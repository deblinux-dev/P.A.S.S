#include <string.h>
#include <unistd.h>
static const char *strs[] = {
    "", "a", "ab", "%abc", "a%b", "abc%", "hello world", "x%y%z",
    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa%target%tail", "%lead", "trail%"
};
#define NS (sizeof(strs)/sizeof(strs[0]))
int main(void) {
    unsigned char out[64]; int k = 0;
    for (unsigned i = 0; i < NS; i++) {
        const char *s = strs[i];
        const char *p = strchrnul(s, '%');
        out[k++] = i; out[k++] = (p - s) > 200 ? 200 : (p - s); out[k++] = (unsigned char)*p;
        const char *q = strchr(s, '%');
        out[k++] = q ? ((q - s) > 200 ? 200 : (q - s)) : 0xFE;
        char *d = strrchr(s, '%');
        out[k++] = d ? ((d - s) > 200 ? 200 : (d - s)) : 0xFE;
    }
    (void)!write(1, out, k);
    return 0;
}
