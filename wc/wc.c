#include "wc.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <ctype.h>
#include <stdbool.h>
#include <string.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

typedef struct chunk_result {
    size_t bytes;
    size_t lines;
    size_t words;
    int head_word_char; 
    int tail_word_char; 
} chunk_result_t;

typedef struct chunk_args {
    const unsigned char *data;
    size_t begin;
    size_t end; // exclusive
    chunk_result_t *out;
} chunk_args_t;

static inline int is_space_uc(unsigned char c) {
    return isspace(c);
}

static void *count_chunk(void *argp) {
    chunk_args_t *arg = (chunk_args_t *)argp;
    const unsigned char *p = arg->data + arg->begin;
    const unsigned char *q = arg->data + arg->end;
    chunk_result_t *res = arg->out;
    memset(res, 0, sizeof(*res));
    if (p >= q) {
        return NULL;
    }
    res->bytes = (size_t)(q - p);
    res->head_word_char = !is_space_uc(*p);
    res->tail_word_char = !is_space_uc(*(q - 1));
    bool in_word = false;
    for (const unsigned char *it = p; it < q; ++it) {
        unsigned char ch = *it;
        if (ch == '\n') res->lines++;
        if (is_space_uc(ch)) {
            if (in_word) in_word = false;
        } else {
            if (!in_word) { res->words++; in_word = true; }
        }
    }
    return NULL;
}

static wc_result_t *do_wc_mmap_mt(int fd) {
    struct stat st;
    if (fstat(fd, &st) != 0) {
        perror("wc: fstat");
        return NULL;
    }
    if (!S_ISREG(st.st_mode)) {
        return NULL; 
    }
    size_t size = (size_t)st.st_size;
    if (size == 0) {
        wc_result_t *r = (wc_result_t *)malloc(sizeof(wc_result_t));
        if (!r) { perror("wc: malloc"); return NULL; }
        r->byte_cnt = 0; r->word_cnt = 0; r->line_cnt = 0;
        return r;
    }
    void *map = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (map == MAP_FAILED) {
        perror("wc: mmap");
        return NULL;
    }
    close(fd);

    long cpus = sysconf(_SC_NPROCESSORS_ONLN);
    if (cpus < 1) cpus = 1;
    const size_t MIN_CHUNK = 256 * 1024; 
    size_t max_threads_by_size = (size + MIN_CHUNK - 1) / MIN_CHUNK;
    size_t nthreads = (size_t)cpus;
    if (nthreads > max_threads_by_size) nthreads = max_threads_by_size;
    if (nthreads < 1) nthreads = 1;

    pthread_t *ths = (pthread_t *)malloc(nthreads * sizeof(pthread_t));
    chunk_args_t *args = (chunk_args_t *)malloc(nthreads * sizeof(chunk_args_t));
    chunk_result_t *res = (chunk_result_t *)malloc(nthreads * sizeof(chunk_result_t));
    if (!ths || !args || !res) {
        perror("wc: malloc threads");
        munmap(map, size);
        free(ths); free(args); free(res);
        return NULL;
    }

    size_t base = 0;
    size_t chunk = size / nthreads;
    size_t rem = size % nthreads;
    for (size_t i = 0; i < nthreads; ++i) {
        size_t begin = base;
        size_t len = chunk + (i < rem ? 1 : 0);
        size_t end = begin + len;
        args[i].data = (const unsigned char *)map;
        args[i].begin = begin;
        args[i].end = end;
        args[i].out = &res[i];
        base = end;
        if (pthread_create(&ths[i], NULL, count_chunk, &args[i]) != 0) {
            perror("wc: pthread_create");
            for (size_t j = 0; j < i; ++j) pthread_join(ths[j], NULL);
            chunk_args_t one = { .data = (const unsigned char *)map, .begin = 0, .end = size, .out = &res[0] };
            count_chunk(&one);
            nthreads = 1;
            break;
        }
    }
    for (size_t i = 0; i < nthreads; ++i) pthread_join(ths[i], NULL);

    size_t total_bytes = 0, total_lines = 0, total_words = 0;
    for (size_t i = 0; i < nthreads; ++i) {
        total_bytes += res[i].bytes;
        total_lines += res[i].lines;
        total_words += res[i].words;
    }
    for (size_t i = 0; i + 1 < nthreads; ++i) {
        if (res[i].tail_word_char && res[i + 1].head_word_char) {
            if (total_words > 0) total_words -= 1;
        }
    }

    wc_result_t *r = (wc_result_t *)malloc(sizeof(wc_result_t));
    if (!r) {
        perror("wc: malloc result");
        munmap(map, size);
        free(ths); free(args); free(res);
        return NULL;
    }
    r->byte_cnt = total_bytes;
    r->line_cnt = total_lines;
    r->word_cnt = total_words;

    munmap(map, size);
    free(ths); free(args); free(res);
    return r;
}

static wc_result_t *do_wc_streaming(int fd) {
    size_t byte_cnt = 0; size_t word_cnt = 0; size_t line_cnt = 0;
    char buf[8192]; bool in_word = false;
    for (;;) {
        ssize_t n = read(fd, buf, sizeof(buf));
        if (n < 0) { perror("wc: read file"); close(fd); return NULL; }
        if (n == 0) break;
        byte_cnt += (size_t)n;
        for (ssize_t i = 0; i < n; ++i) {
            unsigned char ch = (unsigned char)buf[i];
            if (ch == '\n') line_cnt++;
            if (is_space_uc(ch)) { if (in_word) in_word = false; }
            else { if (!in_word) { word_cnt++; in_word = true; } }
        }
    }
    close(fd);
    wc_result_t *r = (wc_result_t *)malloc(sizeof(wc_result_t));
    if (!r) { perror("wc: malloc"); return NULL; }
    r->byte_cnt = byte_cnt; r->word_cnt = word_cnt; r->line_cnt = line_cnt;
    return r;
}

wc_result_t* do_wc(const int fd) {
    struct stat st;
    if (fstat(fd, &st) == 0 && S_ISREG(st.st_mode)) {
        wc_result_t *r = do_wc_mmap_mt(fd);
        if (r) return r;
        return do_wc_streaming(fd);
    }
    return do_wc_streaming(fd);
}
