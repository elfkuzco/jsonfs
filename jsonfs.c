#include "jsonfs.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <jansson.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

void jsonfs_usage() {
        fprintf(
            stderr,
            "usage:  jsonfs [FUSE and mount options] jsonFile mountPoint\n");
}

// convert an integer to string
static char* integer_to_string(size_t value) {
        int size = snprintf(NULL, 0, "%lu", value) + 1;
        char* buf = malloc(size);
        snprintf(buf, size, "%zu", value);
        return buf;
}

// convert a double to string
static char* double_to_string(double value) {
        // allocating additional 3 bytes - one for dot (.), one for (zero) and
        // one for null byte as the format specifier does not always include
        // a dot if the value ends with .0
        int size = snprintf(NULL, 0, "%.17g", value) + 3;
        fprintf(stderr, "size is %d\n", size);
        char* buf = malloc(size);
        snprintf(buf, size, "%.17g", value);
        // check if ther's a decimal point or exponent
        bool has_dot = false;
        for (char* p = buf; *p; ++p) {
                if (*p == '.' || *p == 'e' || *p == 'E') {
                        has_dot = true;
                        break;
                }
        }

        if (!has_dot) {
                size_t len = strlen(buf);
                buf[len] = '.';
                buf[len + 1] = '0';
                buf[len + 2] = '\0';
        }
        return buf;
}

// find the object/array at the specified path from root. It is more
// like a function that finds the base direcotry of a regular file where
// a regular refers to the scalar json objects (null, true, integer,
// real).
json_t* jsonfs_find_element(const char* path, const json_t* root) {
        if (strcmp(path, "/") == 0) return root;

        char* path_copy = strdup(path);
        char* p = path_copy;
        if (path_copy == NULL) {
                perror("unable to duplicate path:");
                return NULL;
        }
        // advance past the first slash and split the path into tokens
        p++;
        char* token = strtok(p, "/");

        json_t* curr = root;
        while (token != NULL && curr) {
                // Determine the type of the node so that we can attempt
                // to retrive the correct element.
                switch (json_typeof(curr)) {
                        case JSON_OBJECT:
                                curr = json_object_get(curr, token);
                                break;
                        case JSON_ARRAY:
                                // we expect the token to be an integer
                                // at this point from our filesystem
                                errno = 0;
                                unsigned long index = strtoul(token, NULL, 0);
                                if (errno != 0) {
                                        perror(
                                            "unable to convert "
                                            "location to "
                                            "integer index:");
                                        free(path_copy);
                                        return NULL;
                                }
                                curr = json_array_get(curr, index);
                                break;
                        default:
                                // we never should get to this point as
                                // scalar objects are hidden behind
                                // key-names for objects or indices for
                                // arrays.
                                errno = ENOENT;
                                perror("unable to find element:");
                                free(path_copy);
                                return NULL;
                }
                token = strtok(NULL, "/");
        }

        free(path_copy);
        return curr;
}

// get attributes from an open file
int jsonfs_getattr(const char* path, struct stat* stbuf) {
        json_t* node = jsonfs_find_element(path, JSONFS_DATA->root);
        if (node == NULL) return -ENOENT;

        // set flags for directories
        memset(stbuf, 0, sizeof(struct stat));

        if (json_is_object(node) || json_is_array(node)) {
                stbuf->st_mode = S_IFDIR | 0555;
                stbuf->st_nlink = 2;
        } else {
                stbuf->st_mode = S_IFREG | 0444;
                stbuf->st_nlink = 1;
                // determine the size in bytes of the "regular file"
                switch (json_typeof(node)) {
                        case JSON_STRING:
                                // for strings, the size is the length
                                stbuf->st_size = json_string_length(node);
                                break;
                        case JSON_REAL: {
                                char* value =
                                    double_to_string(json_real_value(node));
                                stbuf->st_size = strlen(value);
                                free(value);
                                break;
                        }
                        case JSON_INTEGER: {
                                char* value =
                                    integer_to_string(json_integer_value(node));
                                stbuf->st_size = strlen(value);
                                free(value);
                                break;
                        }
                        case JSON_NULL:
                        case JSON_TRUE:
                                // for null and true, we set it to 4
                                // correspoding to it's length if it
                                // were a string
                                stbuf->st_size = 4;
                                break;
                        case JSON_FALSE:
                                // for false, we set it to 5
                                // corresponding to its length if it
                                // were a string
                                stbuf->st_size = 5;
                                break;
                        case JSON_ARRAY:
                        case JSON_OBJECT:
                                // we should never have to deal with
                                // arrays or objects since these are
                                // directories.
                                return -EINVAL;
                }
        }
        return 0;
}

// Initialize filesystem
//
// The return value will passed in the private_data field of
// fuse_context to all file operations and as a parameter to the
// destroy() method.
void* jsonfs_init(struct fuse_conn_info* UNUSED(conn)) { return JSONFS_DATA; }

// Clean up filesystem
void jsonfs_destroy(void* private_data) {
        struct jsonfs_state* state = private_data;

        if (state->root) json_decref(state->root);
        free(state);
}

// open a directory
int jsonfs_opendir(const char* path, struct fuse_file_info* fi) {
        json_t* node = jsonfs_find_element(path, JSONFS_DATA->root);
        if (node == NULL) return -ENOENT;
        // set the fi->fh to the current node (used by jsonfs_readdir)
        fi->fh = (intptr_t)node;
        return 0;
}

// read a directory
int jsonfs_readdir(const char* UNUSED(path), void* buf, fuse_fill_dir_t filler,
                   off_t UNUSED(offset), struct fuse_file_info* fi) {
        // populate the directory entries with current and previous
        // directory
        filler(buf, ".", NULL, 0);
        filler(buf, "..", NULL, 0);

        // no need for finding the node from root since we already have
        // the file handle from jsonfs_opendir
        json_t* node = (json_t*)(intptr_t)fi->fh;
        switch (json_typeof(node)) {
                case JSON_OBJECT: {
                        // for objects, we populate the directory
                        // entries with the key names
                        const char* key;
                        void* iter = json_object_iter(node);
                        while (iter) {
                                key = json_object_iter_key(iter);
                                if (filler(buf, key, NULL, 0) != 0)
                                        return -ENOMEM;
                                iter = json_object_iter_next(node, iter);
                        }
                        break;
                }
                case JSON_ARRAY: {
                        // for arrays, we populate the directory entries
                        // with the index as names.
                        size_t array_size = json_array_size(node);
                        for (size_t i = 0; i < array_size; i++) {
                                char* key = integer_to_string(i);
                                if (filler(buf, key, NULL, 0) != 0) {
                                        free(key);
                                        return -ENOMEM;
                                }
                                free(key);
                        }
                        break;
                }
                default:
                        // only objects and arrays are modeled as
                        // directories
                        return -EINVAL;
        }
        return 0;
}

// open a file within the filesystem
int jsonfs_open(const char* path, struct fuse_file_info* fi) {
        json_t* node = jsonfs_find_element(path, JSONFS_DATA->root);
        if (node == NULL) return -ENOENT;
        if ((fi->flags & 3) != O_RDONLY) return -EACCES;
        // set the fi->fh to the current node
        fi->fh = (intptr_t)node;
        return 0;
}

//  Read data from an open file
int jsonfs_read(const char* UNUSED(path), char* buf, size_t size, off_t offset,
                struct fuse_file_info* fi) {
        // variables to determine the contents of the node and it's
        // length
        char* contents = NULL;
        ;
        size_t len = 0;
        // no need for finding the node from root since we already have
        // the file handle from jsonfs_open
        json_t* node = (json_t*)(intptr_t)fi->fh;
        // boolean for determining if memory was allocated for the
        // contents
        bool mem_allocated = false;
        switch (json_typeof(node)) {
                case JSON_STRING:
                        len = json_string_length(node);
                        contents = json_string_value(node);
                        break;
                case JSON_TRUE:
                        len = 4;
                        contents = "true";
                        break;
                case JSON_FALSE:
                        len = 5;
                        contents = "false";
                        break;
                case JSON_NULL:
                        len = 4;
                        contents = "null";
                        break;
                case JSON_REAL:
                        contents = double_to_string(json_real_value(node));
                        len = strlen(contents);
                        mem_allocated = true;
                        break;
                case JSON_INTEGER:
                        contents = integer_to_string(json_integer_value(node));
                        len = strlen(contents);
                        mem_allocated = true;
                        break;
                case JSON_OBJECT:
                case JSON_ARRAY:
                        return -EINVAL;
        }
        if (offset < len) {
                if (offset + size > len) size = len - offset;
                memcpy(buf, contents + offset, size);
        } else
                size = 0;

        if (mem_allocated) free(contents);
        return size;
}

int main(int argc, char* argv[]) {
        // jsonfs doesn't do any access checking on its own (the comment
        // blocks in fuse.h mention some of the functions that need
        // accesses checked -- but note there are other functions, like
        // chown(), that also need checking!).  Since running jsonfs as
        // root will therefore open Metrodome-sized holes in the system
        // security, we'll check if root is trying to mount the
        // filesystem and refuse if it is.  The somewhat smaller hole of
        // an ordinary user doing it with the allow_other flag is still
        // there because I don't want to parse the options string.
        if ((getuid() == 0) || (geteuid() == 0)) {
                fprintf(stderr,
                        "Running jsonfs as root opens unnacceptable "
                        "security "
                        "holes\n");
                return 1;
        }

        // Perform some sanity checking on the command line:  make sure
        // there are enough arguments, and that neither of the last two
        // start with a hyphen (this will break if you actually have a
        // rootpoint or mountpoint whose name starts with a hyphen, but
        // so will a zillion other programs)
        if ((argc < 3) || (argv[argc - 2][0] == '-') ||
            (argv[argc - 1][0] == '-')) {
                jsonfs_usage();
                return 1;
        }

        json_t* root;
        json_error_t error;

        // Pull the jsonfile out of the argument list and save it in my
        // internal data
        root = json_load_file(argv[argc - 2], 0, &error);
        if (!root) {
                fprintf(stderr, "error while reading file '%s': %s\n", argv[1],
                        error.text);
        }

        struct jsonfs_state* jsonfs_data = malloc(sizeof(struct jsonfs_state));
        if (jsonfs_data == NULL) {
                perror("main calloc");
                return -1;
        }

        jsonfs_data->root = root;

        // remove the jsonfile name from the argument list
        argv[argc - 2] = argv[argc - 1];
        argv[argc - 1] = NULL;
        argc--;

        // turn over control to fuse
        int fuse_stat = fuse_main(argc, argv, &jsonfs_ops, jsonfs_data);

        return fuse_stat;
}
