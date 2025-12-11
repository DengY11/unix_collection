#include "cp.h"
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>

#if defined(__linux__)
#include <sys/sendfile.h>
#endif

static int copy_read_write(int src_fd, int dst_fd) {
    char buf[1 << 20]; 
    for (;;) {
        ssize_t n = read(src_fd, buf, sizeof(buf));
        if (n == 0) break; 
        if (n < 0) {
            if (errno == EINTR) continue;
            perror("cp: read");
            return -1;
        }
        size_t off = 0;
        while (off < (size_t)n) {
            ssize_t w = write(dst_fd, buf + off, (size_t)n - off);
            if (w < 0) {
                if (errno == EINTR) continue;
                perror("cp: write");
                return -1;
            }
            off += (size_t)w;
        }
    }
    return 0;
}

#if defined(__linux__)
static int copy_sendfile(int src_fd, int dst_fd) {
    struct stat st;
    if (fstat(src_fd, &st) != 0) {
        perror("cp: fstat");
        return -1;
    }
    if (!S_ISREG(st.st_mode)) {
        errno = EINVAL;
        return -1;
    }

    off_t offset = 0;
    while (offset < st.st_size) {
        size_t to_copy = (size_t)(st.st_size - offset);
        if (to_copy > (1U << 30)) to_copy = (1U << 30);
        ssize_t sent = sendfile(dst_fd, src_fd, &offset, to_copy);
        if (sent < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (sent == 0) {
            break;
        }
    }
    if (offset == st.st_size) return 0;
    return -1;
}
#endif

int do_cp(int src_fd, int dst_fd) {
#if defined(__linux__)
    int ret = copy_sendfile(src_fd, dst_fd);
    if (ret == 0) {
        if (fsync(dst_fd) != 0) {
            perror("cp: fsync dst");
            return -1;
        }
        return 0;
    }
    if (lseek(src_fd, 0, SEEK_SET) < 0) {
        perror("cp: lseek src");
        return -1;
    }
    if (lseek(dst_fd, 0, SEEK_SET) < 0) {
        perror("cp: lseek dst");
        return -1;
    }
    if (ftruncate(dst_fd, 0) != 0) {
        perror("cp: ftruncate dst");
        return -1;
    }
    ret = copy_read_write(src_fd, dst_fd);
    if (ret != 0) return ret;
    if (fsync(dst_fd) != 0) {
        perror("cp: fsync dst");
        return -1;
    }
    return 0;
#else
    int ret = copy_read_write(src_fd, dst_fd);
    if (ret != 0) return ret;
    if (fsync(dst_fd) != 0) {
        perror("cp: fsync dst");
        return -1;
    }
    return 0;
#endif
}
