#ifndef LS_H
#define LS_H
#include <stddef.h>
#include <sys/stat.h>

typedef enum {
    LS_SORT_NAME = 0,
    LS_SORT_TIME,
    LS_SORT_SIZE
} ls_sort_key_t;

typedef struct _ls_options {
    int show_all_files;     // -a
    int recursive_listing;  // -R
    int show_file_type;     // -F
    int long_format;        // -l
    int human_readable;     // -h
    int reverse;            // -r
    ls_sort_key_t sort_key; //  LS_SORT_NAME
    int directory_only;     // -d
} ls_options_t;

typedef struct _ls_entry {
    char *name;
    size_t name_size;       
    char *full_path;
    size_t full_path_size;  
    struct stat st;         
} ls_entry_t;

typedef struct _ls_entry_list {
    ls_entry_t *entries;
    size_t last_entry_idx;
    size_t capacity;
} ls_entry_list_t;

typedef struct _ls_ret {
    ls_entry_list_t entry_list;
    int error;
} ls_ret_t;

ls_ret_t ls(const char *path, const ls_options_t *options);
void ls_free_ret(ls_ret_t *ret);

#endif