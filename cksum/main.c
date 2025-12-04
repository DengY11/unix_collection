#include "cksum.h"
#include <fcntl.h>
#include <unistd.h>

int main(int argc, char **argv) {
    init_crc32_table();
    if (argc <= 1) {
        int ret = cksum_stream(STDIN_FILENO, NULL);
        return ret == 0 ? 0 : 1;    
    }
    int exit_code = 0;
    for (int i = 1; i < argc; ++i) {
        const char *path = argv[i];
        int fd = open(path, O_RDONLY);
        if (fd < 0) {
            perror("cksum: open file");
            exit_code = 1;
            continue;
        }
        if (cksum_stream(fd, path) != 0) {
            exit_code = 1;
        }
        close(fd);
    }
    return exit_code;
}
