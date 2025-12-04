#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include "wc.h"

static void print_usage(const char *prog) {
    fprintf(stderr, "Usage: %s [-lwc] [file ...]\\n", prog);
    fprintf(stderr, "  -l  print lines only\\n");
    fprintf(stderr, "  -w  print words only\\n");
    fprintf(stderr, "  -c  print bytes only\\n");
    fprintf(stderr, "  no options: print lines/words/bytes\\n");
    fprintf(stderr, "  file '-' reads from standard input\\n");
}

static void print_selected(const wc_result_t *r, int show_l, int show_w, int show_c, const char *name) {
    int printed_any = 0;
    if (show_l) { printf("%zu", r->line_cnt); printed_any = 1; }
    if (show_w) { printf(printed_any ? " %zu" : "%zu", r->word_cnt); printed_any = 1; }
    if (show_c) { printf(printed_any ? " %zu" : "%zu", r->byte_cnt); printed_any = 1; }
    if (name) printf(" %s", name);
    putchar('\n');
}

int main(int argc,char **argv){
    int show_l = 0, show_w = 0, show_c = 0;
    int opt;
    while ((opt = getopt(argc, argv, "lwc")) != -1) {
        switch (opt) {
            case 'l': show_l = 1; break;
            case 'w': show_w = 1; break;
            case 'c': show_c = 1; break;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }
    if (!show_l && !show_w && !show_c) {
        show_l = show_w = show_c = 1;
    }

    size_t total_lines = 0, total_words = 0, total_bytes = 0;
    int files_count = 0;
    int exit_code = 0;

    if (optind >= argc) {
        int fd = dup(STDIN_FILENO);
        if (fd < 0) {
            perror("wc: dup stdin");
            return 1;
        }
        wc_result_t *r = do_wc(fd);
        if (!r) return 1;
        print_selected(r, show_l, show_w, show_c, NULL);
        free(r);
        return 0;
    }

    for (int i = optind; i < argc; ++i) {
        const char *path = argv[i];
        int fd;
        if (path[0] == '-' && path[1] == '\0') {
            fd = dup(STDIN_FILENO);
            if (fd < 0) {
                perror("wc: dup stdin");
                exit_code = 1;
                continue;
            }
        } else {
            fd = open(path, O_RDONLY);
            if (fd < 0) {
                perror("wc: open file");
                exit_code = 1;
                continue;
            }
        }
        wc_result_t *r = do_wc(fd);
        if (!r) { exit_code = 1; continue; }
        print_selected(r, show_l, show_w, show_c, path);
        total_lines += r->line_cnt;
        total_words += r->word_cnt;
        total_bytes += r->byte_cnt;
        files_count++;
        free(r);
    }

    if (files_count > 1) {
        wc_result_t total = { .byte_cnt = total_bytes, .word_cnt = total_words, .line_cnt = total_lines };
        print_selected(&total, show_l, show_w, show_c, "total");
    }
    return exit_code;
}
