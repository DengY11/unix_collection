#include "cat.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

static char* read_file(int fd, size_t* size) {
    char* content = NULL;
    size_t content_size = 0;
    char buf[8192];
    for (;;) {
        ssize_t n = read(fd, buf, sizeof(buf));
        if (n < 0) {
            perror("cat: read file");
            close(fd);
            return NULL;
        }
        if (n == 0) {
            break; 
        }
        content_size += (size_t)n;
        char* new_content = realloc(content, content_size);
        if (!new_content) {
            perror("cat: realloc");
            close(fd);
            free(content);
            return NULL;
        }
        content = new_content;
        memcpy(content + content_size - (size_t)n, buf, (size_t)n);
    }
    close(fd);
    *size = content_size;
    return content;
}

static void squeeze_blank_lines(char **content_ptr, size_t *content_size){
    char* content = *content_ptr;
    char *new_content = malloc(*content_size);
    size_t new_content_size = 0;
    int last_nl = 0;
    for(size_t i = 0;i < *content_size; ++i){
        char c = content[i];
        if (c == '\n'){
            if (!last_nl){
                new_content[new_content_size++] = c;
                last_nl = 1;
            }
        } else {
            new_content[new_content_size++] = c;
            last_nl = 0;
        }
    }
    free(content);
    *content_ptr = new_content;
    *content_size = new_content_size;
}

static void show_line_num(char **content_ptr, size_t *content_size){
    char *content = *content_ptr;
    size_t size = *content_size;
    size_t lines = 0;
    if (size > 0) {
        for (size_t i = 0; i < size; ++i) {
            if (content[i] == '\n') lines++;
        }
        if (content[size - 1] != '\n') lines++;
    }
    if (lines == 0) return;
    size_t max_digits = 1;
    size_t tmp = lines;
    while (tmp >= 10) { max_digits++; tmp /= 10; }
    size_t cap = size + lines * (max_digits + 1);
    char *new_content = malloc(cap);
    if (!new_content) {
        perror("cat: malloc");
        return;
    }
    size_t new_size = 0;
    size_t ln = 1;
    int start = 1;
    for (size_t i = 0; i < size; ++i) {
        if (start) {
            char buf[32];
            int n = snprintf(buf, sizeof(buf), "%zu\t", ln++);
            memcpy(new_content + new_size, buf, (size_t)n);
            new_size += (size_t)n;
            start = 0;
        }
        new_content[new_size++] = content[i];
        if (content[i] == '\n') start = 1;
    }
    free(content);
    *content_ptr = new_content;
    *content_size = new_size;
}

static void show_nonblank_line_num(char **content_ptr, size_t *content_size){
    char *content = *content_ptr;
    size_t size = *content_size;
    size_t nonblank = 0;
    int start = 1;
    for (size_t i = 0; i < size; ++i) {
        if (start) {
            if (content[i] != '\n') nonblank++;
            start = 0;
        }
        if (content[i] == '\n') start = 1;
    }
    if (nonblank == 0) return;
    size_t max_digits = 1;
    size_t tmp = nonblank;
    while (tmp >= 10) { max_digits++; tmp /= 10; }
    size_t cap = size + nonblank * (max_digits + 1);
    char *new_content = malloc(cap);
    if (!new_content) {
        perror("cat: malloc");
        return;
    }
    size_t new_size = 0;
    size_t ln = 1;
    start = 1;
    for (size_t i = 0; i < size; ++i) {
        if (start) {
            if (content[i] != '\n') {
                char buf[32];
                int n = snprintf(buf, sizeof(buf), "%zu\t", ln++);
                memcpy(new_content + new_size, buf, (size_t)n);
                new_size += (size_t)n;
            }
            start = 0;
        }
        new_content[new_size++] = content[i];
        if (content[i] == '\n') start = 1;
    }
    free(content);
    *content_ptr = new_content;
    *content_size = new_size;
}

cat_ret cat(int fd, const cat_options* options){
    size_t content_size = 0;
    char* content = read_file(fd, &content_size);
    if (!content) {
        return (cat_ret){NULL, 0};
    }
    if(options->squeeze_blank_lines){
       squeeze_blank_lines(&content, &content_size);
    }
    if(options->number_nonblank_lines){
        show_nonblank_line_num(&content, &content_size);
    } else if(options->show_line_num){
        show_line_num(&content, &content_size);
    }
    return (cat_ret){content, content_size};
}
