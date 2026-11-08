#include <string.h>
#include <unistd.h>
static unsigned H[8];
static void mix(int idx, unsigned long v){ H[idx] ^= v; H[idx] *= 16777619u; }
static const char *strs[] = {
    "", "a", "ab", "%abc", "a%b", "abc%", "hello world", "x%y%z",
    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa%target%tail", "%lead", "trail%"
};
#define NS (sizeof(strs)/sizeof(strs[0]))
int main(void) {
    for (unsigned i = 0; i < NS; i++) {
        const char *s = strs[i];
        size_t l = strlen(s);              mix(0, l);
        const char *p = strchrnul(s, '%'); mix(1, p - s); mix(1, *p);
        const char *q = strchr(s, '%');    mix(2, q ? q - s : 0xF000);
        const char *m = memchr(s, '%', l); mix(3, m ? m - s : 0xF000);
        char *d = strrchr(s, '%');         mix(4, d ? d - s : 0xF000);
        size_t c = strcspn(s, "%");        mix(5, c);
        char *r = strstr(s, "%");          mix(6, r ? r - s : 0xF000);
        size_t n = strnlen(s, l + 1);      mix(7, n);
    }
    (void)!write(1, H, 32);
    return 0;
}
