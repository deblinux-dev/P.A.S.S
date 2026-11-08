#include <unistd.h>
#include <string.h>
int main(void){ char b[64]; memset(b,0x41,60); b[60]=0; volatile size_t l = strlen(b); (void)!write(1, "ok\n", 3); return 0; }
