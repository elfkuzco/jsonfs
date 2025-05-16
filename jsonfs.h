#ifndef _JSONFS_H_
#define _JSONFS_H_
#define _POSIX_C_SOURCE 200809L
#define FUSE_USE_VERSION 26

#include <fuse/fuse.h>

#include "jansson.h"

// Macro to make compilers not warn about unused parameters
#ifdef UNUSED
#elif defined(__GNUC__)
#define UNUSED(x) UNUSED_##x __attribute__((unused))
#elif defined(__LCLINT__)
#define UNUSED(x) /*@unused@*/ x
#else
#define UNUSED(x) x
#endif

int jsonfs_getattr(const char* path, struct stat* statbuf);

void* jsonfs_init(struct fuse_conn_info* conn);

void jsonfs_destroy(void* private_data);

int jsonfs_open(const char* path, struct fuse_file_info* fi);

int jsonfs_opendir(const char* path, struct fuse_file_info* fi);

int jsonfs_readdir(const char* path, void* buf, fuse_fill_dir_t filler,
                   off_t offset, struct fuse_file_info* fi);

int jsonfs_read(const char* path, char* buf, size_t size, off_t offset,
                struct fuse_file_info* fi);

struct jsonfs_state {
        json_t* root;
};

struct fuse_operations jsonfs_ops = {
    .getattr = jsonfs_getattr,
    .init = jsonfs_init,
    .destroy = jsonfs_destroy,
    .open = jsonfs_open,
    .read = jsonfs_read,
    .opendir = jsonfs_opendir,
    .readdir = jsonfs_readdir,
};

#define JSONFS_DATA ((struct jsonfs_state*)fuse_get_context()->private_data)
#endif
