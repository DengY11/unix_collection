#include "cat.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

static int process_fd(int fd, const cat_options *options){
    cat_ret ret = cat(fd, &options[0]);
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

int main(int argc, char **argv){
    cat_options options = (cat_options){0,0,0};
    char **files = NULL;
    int files_count = 0;

    files = malloc(sizeof(char*) * (argc > 1 ? argc - 1 : 1));
    if (!files) {
        perror("cat: malloc");
        return 1;
    }

    for (int i = 1; i < argc; ++i) {
        const char *arg = argv[i];
        if (arg[0] == '-' && arg[1] != '\0') {
            for (int j = 1; arg[j] != '\0'; ++j) {
                switch (arg[j]) {
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
                        break;
                }
            }
        } else {
            files[files_count++] = argv[i];
        }
    }

    if (files_count == 0) {
        int rc = process_fd(STDIN_FILENO, &options);
        free(files);
        return rc;
    }

    int exit_code = 0;
    for (int i = 0; i < files_count; ++i) {
        if (files[i][0] == '-' && files[i][1] == '\0') {
            if (process_fd(STDIN_FILENO, &options) != 0) exit_code = 1;
            continue;
        }
        int fd = open(files[i], O_RDONLY);
        if (fd < 0) {
            perror("cat: open");
            exit_code = 1;
            continue;
        }
        if (process_fd(fd, &options) != 0) exit_code = 1;
    }
    free(files);
    return exit_code;
}
