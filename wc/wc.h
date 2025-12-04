#ifndef WC_H
#define WC_H

#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

struct wc_result {
    size_t byte_cnt;
    size_t word_cnt;
    size_t line_cnt;
};

typedef struct wc_result wc_result_t;

wc_result_t* do_wc(const int fd);


#endif
