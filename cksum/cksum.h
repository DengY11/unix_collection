#ifndef CKSUM_H
#define CKSUM_H
#include <stdint.h>
#include <stdio.h>
void init_crc32_table();
uint32_t update_crc(uint32_t crc,unsigned char *buf, int len);
int cksum_stream(int fd, const char *name);
#endif