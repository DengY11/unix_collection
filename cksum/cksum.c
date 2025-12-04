#include "cksum.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
static uint32_t crc32_table[256];
void init_crc32_table() {
    uint32_t poly = 0x04C11DB7u; 
    for (uint32_t i = 0; i < 256; ++i) {
        uint32_t crc = i << 24;
        for (int j = 0; j < 8; ++j) {
            if (crc & 0x80000000u) {
                crc = (crc << 1) ^ poly;
            } else {
                crc <<= 1;
            }
        }
        crc32_table[i] = crc;
    }
}
uint32_t update_crc(uint32_t crc, unsigned char *buf, int len) {
    for (int i = 0; i < len; ++i) {
        uint32_t idx = ((crc >> 24) ^ buf[i]) & 0xFFu;
        crc = (crc << 8) ^ crc32_table[idx];
    }
    return crc;
}
int cksum_stream(int fd, const char *name) {
    unsigned char buf[8192];
    uint32_t crc = 0u; 
    unsigned long long total = 0ull;
    for (;;) {
        ssize_t n = read(fd, buf, sizeof(buf));
        if (n < 0) {
            perror("cksum: read error");
            return -1;
        }
        if (n == 0) {
            break; // EOF
        }
        crc = update_crc(crc, buf, (int)n);
        total += (unsigned long long)n;
    }
    unsigned long long len_bytes = total;
    while (len_bytes != 0ull) {
        unsigned char b = (unsigned char)(len_bytes & 0xFFu);
        crc = update_crc(crc, &b, 1);
        len_bytes >>= 8;
    }
    crc ^= 0xFFFFFFFFu;
    if (name)  printf("%u %llu %s\n", crc, total, name);
    else printf("%u %llu\n", crc, total);
    return 0;
}
