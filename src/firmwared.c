#include <stdlib.h>
#include <string.h>

#include "manager.h"

int main(int argc, char **argv) {
        _cleanup_(manager_freep) Manager *manager = NULL;
        bool tentative = false;
        int r;

        for (int i = 0; i < argc; i++) {
                if (strcmp("--tentative", argv[i]) == 0)
                        tentative = true;
        }

        r = manager_new(&manager, tentative);
        if (r < 0)
                return EXIT_FAILURE;

        r = manager_run(manager);
        if (r < 0)
                return EXIT_FAILURE;

        return EXIT_SUCCESS;
}
