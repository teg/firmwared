#include <stdlib.h>
#include <string.h>

#include "manager.h"
#include "log-util.h"

int main(int argc, char **argv) {
        _cleanup_(manager_freep) Manager *manager = NULL;
        bool tentative = false;
        int r;

        setbuf(stdout, NULL);

        for (int i = 0; i < argc; i++) {
                if (strcmp("--tentative", argv[i]) == 0)
                        tentative = true;
        }

        r = manager_new(&manager, tentative);
        if (r < 0) {
                log_error("firmwared %s", strerror(-r));
                return EXIT_FAILURE;
        }

        r = manager_run(manager);
        if (r < 0) {
                log_error("firmwared %s", strerror(-r));
                return EXIT_FAILURE;
        }

        return EXIT_SUCCESS;
}
