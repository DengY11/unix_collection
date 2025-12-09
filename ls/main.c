#include "ls.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <sys/stat.h>
#include <limits.h>

static void format_mode(mode_t m, char out[11]) {
    out[0] = S_ISDIR(m) ? 'd' : S_ISLNK(m) ? 'l' : S_ISCHR(m) ? 'c' : S_ISBLK(m) ? 'b' : S_ISFIFO(m) ? 'p' : S_ISSOCK(m) ? 's' : '-';
    out[1] = (m & S_IRUSR) ? 'r' : '-';
    out[2] = (m & S_IWUSR) ? 'w' : '-';
    out[3] = (m & S_IXUSR) ? ((m & S_ISUID) ? 's' : 'x') : ((m & S_ISUID) ? 'S' : '-');
    out[4] = (m & S_IRGRP) ? 'r' : '-';
    out[5] = (m & S_IWGRP) ? 'w' : '-';
    out[6] = (m & S_IXGRP) ? ((m & S_ISGID) ? 's' : 'x') : ((m & S_ISGID) ? 'S' : '-');
    out[7] = (m & S_IROTH) ? 'r' : '-';
    out[8] = (m & S_IWOTH) ? 'w' : '-';
    out[9] = (m & S_IXOTH) ? ((m & S_ISVTX) ? 't' : 'x') : ((m & S_ISVTX) ? 'T' : '-');
    out[10] = '\0';
}

static void human_size(off_t s, char *buf, size_t n) {
    const char *units[] = {"B","K","M","G","T","P"};
    double v = (double)s;
    int idx = 0;
    while (v >= 1024.0 && idx < 5) { v /= 1024.0; idx++; }
    if (idx == 0) snprintf(buf, n, "%lld", (long long)s);
    else snprintf(buf, n, "%.1f%s", v, units[idx]);
}

static char type_suffix(const struct stat *st) {
    if (S_ISDIR(st->st_mode)) return '/';
    if (S_ISLNK(st->st_mode)) return '@';
    if ((st->st_mode & (S_IXUSR|S_IXGRP|S_IXOTH)) != 0) return '*';
    return '\0';
}

static void print_entry(const ls_entry_t *e, const ls_options_t *opt) {
    char suffix = '\0';
    if (opt->show_file_type) suffix = type_suffix(&e->st);

    if (opt->long_format) {
        char modebuf[11];
        format_mode(e->st.st_mode, modebuf);

        struct passwd *pw = getpwuid(e->st.st_uid);
        struct group  *gr = getgrgid(e->st.st_gid);
        const char *user = pw ? pw->pw_name : "?";
        const char *group = gr ? gr->gr_name : "?";

        char sizebuf[32];
        if (opt->human_readable) human_size(e->st.st_size, sizebuf, sizeof(sizebuf));
        else snprintf(sizebuf, sizeof(sizebuf), "%lld", (long long)e->st.st_size);

        char timebuf[64];
        struct tm lt;
        localtime_r(&e->st.st_mtime, &lt);
        strftime(timebuf, sizeof(timebuf), "%b %e %H:%M", &lt);

        if (S_ISLNK(e->st.st_mode)) {
            char linkbuf[PATH_MAX];
            ssize_t n = readlink(e->full_path, linkbuf, sizeof(linkbuf)-1);
            if (n >= 0) linkbuf[n] = '\0'; else linkbuf[0] = '\0';
            printf("%s %3lld %-8s %-8s %8s %s %s%s -> %s\n",
                modebuf,
                (long long)e->st.st_nlink,
                user,
                group,
                sizebuf,
                timebuf,
                e->name,
                suffix ? (char[2]){suffix,'\0'} : "",
                linkbuf);
        } else {
            printf("%s %3lld %-8s %-8s %8s %s %s%s\n",
                modebuf,
                (long long)e->st.st_nlink,
                user,
                group,
                sizebuf,
                timebuf,
                e->name,
                suffix ? (char[2]){suffix,'\0'} : "");
        }
    } else {
        if (suffix) printf("%s%c\n", e->name, suffix);
        else printf("%s\n", e->name);
    }
}

static void print_usage(const char *prog) {
    fprintf(stderr, "Usage: %s [-alhRrFtSd] [path ...]\n", prog);
}

int main(int argc, char **argv) {
    ls_options_t opt = {0};
    opt.sort_key = LS_SORT_NAME;
    int c;
    while ((c = getopt(argc, argv, "alhRrFtSd")) != -1) {
        switch (c) {
            case 'a': opt.show_all_files = 1; break;
            case 'l': opt.long_format = 1; break;
            case 'h': opt.human_readable = 1; break;
            case 'R': opt.recursive_listing = 1; break;
            case 'r': opt.reverse = 1; break;
            case 'F': opt.show_file_type = 1; break;
            case 't': opt.sort_key = LS_SORT_TIME; break;
            case 'S': opt.sort_key = LS_SORT_SIZE; break;
            case 'd': opt.directory_only = 1; break;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }

    int npaths = argc - optind;
    if (npaths == 0) {
        const char *path = ".";
        ls_ret_t ret = ls(path, &opt);
        if (ret.error) {
            perror("ls");
            return 1;
        }
        for (size_t i = 0; i < ret.entry_list.last_entry_idx; ++i) {
            print_entry(&ret.entry_list.entries[i], &opt);
        }
        ls_free_ret(&ret);
        return 0;
    }

    int exit_code = 0;
    for (int i = optind; i < argc; ++i) {
        const char *path = argv[i];
        ls_ret_t ret = ls(path, &opt);
        if (ret.error) {
            fprintf(stderr, "ls: cannot access '%s'\n", path);
            exit_code = 1;
            continue;
        }
        if (npaths > 1 || opt.recursive_listing) {
            printf("%s:\n", path);
        }
        for (size_t j = 0; j < ret.entry_list.last_entry_idx; ++j) {
            print_entry(&ret.entry_list.entries[j], &opt);
        }
        if (npaths > 1 || opt.recursive_listing) putchar('\n');
        ls_free_ret(&ret);
    }
    return exit_code;
}

