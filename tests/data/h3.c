#include <unistd.h>
#include <string.h>
int main(void){ char b[64]; memset(b,0x41,60); (void)!write(1, "ok\n", 3); return 0; }
