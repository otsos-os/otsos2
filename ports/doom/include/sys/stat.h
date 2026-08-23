#ifndef _SYS_STAT_H
#define _SYS_STAT_H

#include <sys/types.h>

struct stat {
    off_t st_size;
};

static inline int stat(const char *path, struct stat *buf) {
    (void)path;
    (void)buf;
    return -1;
}

static inline int mkdir(const char *pathname, unsigned int mode) {
    (void)pathname;
    (void)mode;
    return 0;
}

#endif
