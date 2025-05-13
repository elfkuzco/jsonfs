#ifndef _JSONFS_H_
#define _JSONFS_H_
#define _POSIX_C_SOURCE 200809L
#define FUSE_USE_VERSION 26

#include <fuse/fuse.h>

#include "jansson.h"

#define JSONFS_NATIVE_IDENTIFIER "#jsonfs_native"

int jsonfs_getattr(const char* path, struct stat* statbuf);

struct fuse_operations jsonfs_ops = {
    .getattr = jsonfs_getattr,
};

#endif
