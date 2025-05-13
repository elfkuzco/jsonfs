#include "jsonfs.h"

#include <errno.h>
#include <jansson.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

void jsonfs_usage() {
        fprintf(stderr,
                "usage:  jsonfs [FUSE and mount options] rootDir mountPoint\n");
}

// find the object/array at the specified path from root. It is more like a
// function that finds the base direcotry of a regular file.
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
                // if the token is same as the identifier for
                // native objects, we simply return the object
                if (strcmp(token, JSONFS_NATIVE_IDENTIFIER) == 0) {
                        free(path_copy);
                        return curr;
                }
                // Determine the type of the node so that we can attempt to
                // retrive the correct element.
                switch (json_typeof(curr)) {
                        case JSON_OBJECT:
                                curr = json_object_get(curr, token);
                                break;
                        case JSON_ARRAY:
                                // we expect the token to be an integer at this
                                // point from our filesystem
                                errno = 0;
                                unsigned long index = strtoul(token, NULL, 0);
                                if (errno != 0) {
                                        perror(
                                            "unable to convert location to "
                                            "integer index:");
                                        free(path_copy);
                                        return NULL;
                                }
                                curr = json_array_get(curr, index);
                                break;
                        default:
                                // we never should get to this point as we
                                // always specify native json objects as
                                errno = 2;
                                perror("unable to find element:");
                                free(path_copy);
                                return NULL;
                }
                token = strtok(NULL, "/");
        }

        free(path_copy);
        return curr;
}

//
int jsonfs_getattr(const char* path, struct stat* statbuf) { return 0; }

int main(int argc, char* argv[]) {
        // jsonfs doesn't do any access checking on its own (the comment
        // blocks in fuse.h mention some of the functions that need
        // accesses checked -- but note there are other functions, like
        // chown(), that also need checking!).  Since running jsonfs as root
        // will therefore open Metrodome-sized holes in the system
        // security, we'll check if root is trying to mount the filesystem
        // and refuse if it is.  The somewhat smaller hole of an ordinary
        // user doing it with the allow_other flag is still there because
        // I don't want to parse the options string.
        if ((getuid() == 0) || (geteuid() == 0)) {
                fprintf(stderr,
                        "Running jsonfs as root opens unnacceptable security "
                        "holes\n");
                return 1;
        }

        // Perform some sanity checking on the command line:  make sure
        // there are enough arguments, and that neither of the last two
        // start with a hyphen (this will break if you actually have a
        // rootpoint or mountpoint whose name starts with a hyphen, but so
        // will a zillion other programs)
        if ((argc < 3) || (argv[argc - 2][0] == '-') ||
            (argv[argc - 1][0] == '-')) {
                jsonfs_usage();
                return 1;
        }

        json_t* json;
        json_error_t error;

        json = json_load_file(argv[1], 0, &error);
        if (!json) {
                fprintf(stderr, "error while reading file '%s': %s\n", argv[1],
                        error.text);
        }
        json_decref(json);
        return 0;
}
