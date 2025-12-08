#ifndef CAT_H
#define CAT_H
#include <stddef.h>

typedef struct _cat_options {
    int show_line_num;
    int number_nonblank_lines;
    int squeeze_blank_lines;
} cat_options;

typedef struct _cat_ret{
    char* content;
    size_t content_size;
} cat_ret;

cat_ret cat(int fd, const cat_options* options);


#endif