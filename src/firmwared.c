#include <stdlib.h>

#include "manager.h"

int main(int argc, char **argv) {
        _cleanup_(manager_freep) Manager *manager = NULL;
        int r;

        r = manager_new(&manager);
        if (r < 0)
                return EXIT_FAILURE;

        r = manager_run(manager);
        if (r < 0)
                return EXIT_FAILURE;

        return EXIT_SUCCESS;
}
