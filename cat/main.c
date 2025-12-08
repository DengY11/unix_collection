#include "cat.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

int main(int argc, char **argv){
    cat_options options = {0, 0, 0};
    int opt;
    while ((opt = getopt(argc, argv, "nbs")) != -1) {
        switch (opt) {
            case 'n':
                options.show_line_num = 1;
                break;
            case 'b':
                options.number_nonblank_lines = 1;
                options.show_line_num = 0;
                break;
            case 's':
                options.squeeze_blank_lines = 1;
                break;
            default:
                return 1;
        }
    }

    if (optind >= argc) {
        cat_ret ret = cat(STDIN_FILENO, &options);
        if (!ret.content) return 1;
        size_t written = fwrite(ret.content, 1, ret.content_size, stdout);
        if (written != ret.content_size) {
            perror("cat: write");
            free(ret.content);
            return 1;
        }
        free(ret.content);
        return 0;
    }

    int exit_code = 0;
    for (int i = optind; i < argc; ++i) {
        int fd = open(argv[i], O_RDONLY);
        if (fd < 0) {
            perror("cat: open");
            exit_code = 1;
            continue;
        }
        cat_ret ret = cat(fd, &options);
        if (!ret.content) {
            exit_code = 1;
            continue;
        }
        size_t written = fwrite(ret.content, 1, ret.content_size, stdout);
        if (written != ret.content_size) {
            perror("cat: write");
            free(ret.content);
            exit_code = 1;
            continue;
        }
        free(ret.content);
    }
    return exit_code;
}
