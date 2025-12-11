#include "cp.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>

static void print_usage(const char *prog) {
    fprintf(stderr, "Usage: %s <src> <dst>\n", prog);
}

int main(int argc, char **argv) {
    if (argc != 3) {
        print_usage(argv[0]);
        return 1;
    }
    const char *src_path = argv[1];
    const char *dst_path = argv[2];

    int src_fd = open(src_path, O_RDONLY);
    if (src_fd < 0) {
        perror("cp: open src");
        return 1;
    }

    struct stat st;
    if (fstat(src_fd, &st) != 0) {
        perror("cp: fstat src");
        close(src_fd);
        return 1;
    }

    mode_t mode = st.st_mode & 0777;
    int dst_fd = open(dst_path, O_WRONLY | O_CREAT | O_TRUNC, mode);
    if (dst_fd < 0) {
        perror("cp: open dst");
        close(src_fd);
        return 1;
    }

    int ret = do_cp(src_fd, dst_fd);
    if (ret != 0) {
        close(src_fd);
        close(dst_fd);
        return 1;
    }

    if (fchmod(dst_fd, st.st_mode & 07777) != 0) {
        perror("cp: fchmod dst");
    }

    close(src_fd);
    close(dst_fd);
    return 0;
}
