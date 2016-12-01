#pragma once

#include <stdbool.h>

#define _cleanup_(_x) __attribute__((__cleanup__(_x)))

typedef struct Manager Manager;

int manager_new(Manager **managerp, bool tentative, const char *path);
void manager_free(Manager *manager);

int manager_run(Manager *manager);

static inline void manager_freep(Manager **managerp) {
        if (*managerp)
                manager_free(*managerp);
}
