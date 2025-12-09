#include "ls.h"
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static inline void list_init(ls_entry_list_t *list) {
    list->entries = NULL;
    list->last_entry_idx = 0;
    list->capacity = 0;
}

static int list_ensure_capacity(ls_entry_list_t *list, size_t min_cap) {
    if (list->capacity >= min_cap) return 0;
    size_t new_cap = list->capacity ? list->capacity * 2 : 16;
    if (new_cap < min_cap) new_cap = min_cap;
    ls_entry_t *new_entries = (ls_entry_t *)realloc(list->entries, new_cap * sizeof(ls_entry_t));
    if (!new_entries) return -1;
    list->entries = new_entries;
    list->capacity = new_cap;
    return 0;
}

static inline int is_dot_or_dotdot(const char *name) {
    return (name[0] == '.' && (name[1] == '\0' || (name[1] == '.' && name[2] == '\0')));
}

static char *join_path(const char *base, const char *name) {
    size_t bl = strlen(base);
    size_t nl = strlen(name);
    int need_slash = (bl > 0 && base[bl - 1] != '/');
    size_t len = bl + (need_slash ? 1 : 0) + nl + 1;
    char *buf = (char *)malloc(len);
    if (!buf) return NULL;
    memcpy(buf, base, bl);
    if (need_slash) buf[bl] = '/';
    memcpy(buf + bl + (need_slash ? 1 : 0), name, nl);
    buf[len - 1] = '\0';
    return buf;
}

static int append_entry(ls_entry_list_t *list, const char *name, const char *full_path, const struct stat *st) {
    if (list_ensure_capacity(list, list->last_entry_idx + 1) != 0) return -1;
    ls_entry_t *e = &list->entries[list->last_entry_idx];
    e->name = strdup(name);
    if (!e->name) return -1;
    e->name_size = strlen(name);
    e->full_path = strdup(full_path);
    if (!e->full_path) {
        free(e->name);
        return -1;
    }
    e->full_path_size = strlen(full_path);
    e->st = *st;
    list->last_entry_idx += 1;
    return 0;
}

static int should_include(const ls_options_t *opt, const char *name, const struct stat *st) {
    if (!opt->show_all_files && name[0] == '.') return 0;
    if (opt->directory_only && !S_ISDIR(st->st_mode)) return 0;
    return 1;
}

static int read_dir_raw(const char *path, ls_entry_list_t *out) {
    DIR *dir = opendir(path);
    if (!dir) return -1;
    struct dirent *de;
    errno = 0;
    while ((de = readdir(dir)) != NULL) {
        char *full = join_path(path, de->d_name);
        if (!full) { errno = ENOMEM; break; }
        struct stat st;
        if (lstat(full, &st) != 0) {
            free(full);
            continue;
        }
        if (append_entry(out, de->d_name, full, &st) != 0) {
            free(full);
            errno = ENOMEM;
            break;
        }
        free(full);
    }
    int saved = errno;
    closedir(dir);
    errno = saved;
    if (de == NULL && saved != 0) return -1;
    return 0;
}

static int cmp_name(const void *a, const void *b) {
    const ls_entry_t *ea = (const ls_entry_t *)a;
    const ls_entry_t *eb = (const ls_entry_t *)b;
    return strcoll(ea->name, eb->name);
}

static int cmp_time(const void *a, const void *b) {
    const ls_entry_t *ea = (const ls_entry_t *)a;
    const ls_entry_t *eb = (const ls_entry_t *)b;
    if (ea->st.st_mtime == eb->st.st_mtime) return strcoll(ea->name, eb->name);
    return (ea->st.st_mtime < eb->st.st_mtime) ? 1 : -1;
}

static int cmp_size(const void *a, const void *b) {
    const ls_entry_t *ea = (const ls_entry_t *)a;
    const ls_entry_t *eb = (const ls_entry_t *)b;
    if (ea->st.st_size == eb->st.st_size) return strcoll(ea->name, eb->name);
    return (ea->st.st_size < eb->st.st_size) ? 1 : -1;
}

static void sort_entries(ls_entry_list_t *list, const ls_options_t *opt) {
    if (list->last_entry_idx <= 1) return;
    int (*cmp)(const void *, const void *) = cmp_name;
    if (opt->sort_key == LS_SORT_TIME) cmp = cmp_time;
    else if (opt->sort_key == LS_SORT_SIZE) cmp = cmp_size;
    qsort(list->entries, list->last_entry_idx, sizeof(ls_entry_t), cmp);
    if (opt->reverse) {
        size_t n = list->last_entry_idx;
        for (size_t i = 0; i < n / 2; ++i) {
            ls_entry_t tmp = list->entries[i];
            list->entries[i] = list->entries[n - 1 - i];
            list->entries[n - 1 - i] = tmp;
        }
    }
}

static int collect_filtered(const char *path, const ls_options_t *opt, ls_entry_list_t *out) {
    ls_entry_list_t tmp;
    list_init(&tmp);
    if (read_dir_raw(path, &tmp) != 0) {
        if (tmp.entries) {
            for (size_t i = 0; i < tmp.last_entry_idx; ++i) {
                free(tmp.entries[i].name);
                free(tmp.entries[i].full_path);
            }
            free(tmp.entries);
        }
        return -1;
    }
    for (size_t i = 0; i < tmp.last_entry_idx; ++i) {
        ls_entry_t *e = &tmp.entries[i];
        if (!should_include(opt, e->name, &e->st)) {
            continue;
        }
        if (append_entry(out, e->name, e->full_path, &e->st) != 0) {
            for (size_t j = 0; j < tmp.last_entry_idx; ++j) {
                free(tmp.entries[j].name);
                free(tmp.entries[j].full_path);
            }
            free(tmp.entries);
            return -1;
        }
    }
    for (size_t i = 0; i < tmp.last_entry_idx; ++i) {
        free(tmp.entries[i].name);
        free(tmp.entries[i].full_path);
    }
    free(tmp.entries);
    return 0;
}

static int collect_recursive(const char *path, const ls_options_t *opt, ls_entry_list_t *out) {
    ls_entry_list_t cur;
    list_init(&cur);
    if (read_dir_raw(path, &cur) != 0) {
        if (cur.entries) {
            for (size_t i = 0; i < cur.last_entry_idx; ++i) {
                free(cur.entries[i].name);
                free(cur.entries[i].full_path);
            }
            free(cur.entries);
        }
        return -1;
    }
    for (size_t i = 0; i < cur.last_entry_idx; ++i) {
        ls_entry_t *e = &cur.entries[i];
        if (should_include(opt, e->name, &e->st)) {
            if (append_entry(out, e->name, e->full_path, &e->st) != 0) {
                for (size_t j = 0; j < cur.last_entry_idx; ++j) {
                    free(cur.entries[j].name);
                    free(cur.entries[j].full_path);
                }
                free(cur.entries);
                return -1;
            }
        }
        if (S_ISDIR(e->st.st_mode)) {
            if (is_dot_or_dotdot(e->name)) continue;
            collect_recursive(e->full_path, opt, out);
        }
    }
    for (size_t i = 0; i < cur.last_entry_idx; ++i) {
        free(cur.entries[i].name);
        free(cur.entries[i].full_path);
    }
    free(cur.entries);
    return 0;
}

ls_ret_t ls(const char *path, const ls_options_t *options) {
    ls_ret_t ret;
    ret.error = 0;
    list_init(&ret.entry_list);

    struct stat st;
    if (lstat(path, &st) != 0) {
        ret.error = 1;
        return ret;
    }

    if (S_ISDIR(st.st_mode)) {
        if (options && options->recursive_listing) {
            if (collect_recursive(path, options, &ret.entry_list) != 0) {
                ret.error = 1;
                return ret;
            }
        } else {
            if (collect_filtered(path, options, &ret.entry_list) != 0) {
                ret.error = 1;
                return ret;
            }
        }
    } else {
        if (should_include(options, path, &st)) {
            if (append_entry(&ret.entry_list, path, path, &st) != 0) {
                ret.error = 1;
                return ret;
            }
        }
    }

    sort_entries(&ret.entry_list, options);
    return ret;
}

void ls_free_ret(ls_ret_t *ret) {
    if (!ret) return;
    if (ret->entry_list.entries) {
        for (size_t i = 0; i < ret->entry_list.last_entry_idx; ++i) {
            free(ret->entry_list.entries[i].name);
            free(ret->entry_list.entries[i].full_path);
        }
        free(ret->entry_list.entries);
        ret->entry_list.entries = NULL;
    }
    ret->entry_list.last_entry_idx = 0;
    ret->entry_list.capacity = 0;
}
