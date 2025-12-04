#include "wc.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <ctype.h>
#include <stdbool.h>

wc_result_t* do_wc(const int fd) {
    size_t byte_cnt = 0;
    size_t word_cnt = 0;
    size_t line_cnt = 0;
    char buf[8192];
    bool in_word = false;
    for (;;) {
        ssize_t n = read(fd, buf, sizeof(buf));
        if (n < 0) {
            perror("wc: read file");
            close(fd);
            return NULL;
        }
        if (n == 0) {
            break; // EOF
        }
        byte_cnt += (size_t)n;
        for (ssize_t i = 0; i < n; i++) {
            unsigned char ch = (unsigned char)buf[i];
            if (ch == '\n') {
                line_cnt++;
            }
            if (isspace(ch)) {
                if (in_word) {
                    word_cnt++;
                    in_word = false;
                }
            } else {
                in_word = true;
            }
        }
    }
    if (in_word) {
        word_cnt++;
    }
    close(fd);
    wc_result_t *result = malloc(sizeof(wc_result_t));
    if (!result) {
        perror("wc: malloc");
        return NULL;
    }
    result->byte_cnt = byte_cnt;
    result->word_cnt = word_cnt;
    result->line_cnt = line_cnt;
    return result;
}
