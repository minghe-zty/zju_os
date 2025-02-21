#include "string.h"
#include "stdint.h"

void *memset(void *dest, int c, uint64_t n) {
    char *s = (char *)dest;
    for (uint64_t i = 0; i < n; ++i) {
        s[i] = c;
    }
    return dest;
}

void *memcpy(char *dest, char *src , uint64_t n){
    for(uint64_t i = 0 ; i < n ; ++i){
        dest[i] = src[i];
    }
    return ;
}