#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>

#include "manager.h"
#include "log-util.h"

#define ELEMENTSOF(x) (sizeof(x)/sizeof(x[0]))

static const char* const firmware_builtin_dirs[] = {
	FIRMWARE_PATH
};

char **firmware_dirs = NULL;
size_t firmware_dirs_size;

static int setup_firmware_dirs(char *dirs) {
        char *token;
        char **p;
        size_t i = 0, j;

        /* First, the runtime defined lookup paths */
        token = strtok(dirs, ":");
        while (token) {
                p = realloc(firmware_dirs, (i + 1) * sizeof(char*));
                if (!p)
                        return -ENOMEM;
                firmware_dirs = p;

                firmware_dirs[i] = strdup(token);
                if (!firmware_dirs[i])
                        return -ENOMEM;

                token = strtok(NULL, ":");
                i++;
        }

        /* Second the default builtin lookup paths */
        p = realloc(firmware_dirs, (i + ELEMENTSOF(firmware_builtin_dirs)) * sizeof(char *));
        if (!p)
                return -ENOMEM;
        firmware_dirs = p;
        memset(&firmware_dirs[i], 0, ELEMENTSOF(firmware_builtin_dirs));
        firmware_dirs_size = i + ELEMENTSOF(firmware_builtin_dirs);

        for (j = i; j < firmware_dirs_size; j++)
                firmware_dirs[j] = strdup(firmware_builtin_dirs[j - i]);

        return 0;
}

static void cleanup_firmware_dirs(void) {
        size_t i;

        for (i = 0; i < firmware_dirs_size; i++)
                if (firmware_dirs[i])
                        free(firmware_dirs[i]);

        if (firmware_dirs)
                free(firmware_dirs);
}

static void usage(void) {
	printf("firmwared - Linux Firmware Loader Daemon\n"
		"Usage:\n");
	printf("\tfirmwared [options]\n");
	printf("Options:\n"
		"\t-t, --tentative        Defer loading of non existing firmwares\n"
		"\t-d, --dirs [paths]     Firmware loading paths\n"
		"\t-h, --help             Show help options\n");
}

static const struct option main_options[] = {
	{ "tentative",     no_argument,       NULL, 't' },
	{ "dirs",          required_argument, NULL, 'd' },
	{ "help",          no_argument,       NULL, 'h' },
	{ }
};

int main(int argc, char **argv) {
        _cleanup_(manager_freep) Manager *manager = NULL;
        bool tentative = false;
        char *dirs = NULL;
        int r;

        setbuf(stdout, NULL);

        for (;;) {
                int opt;

                opt = getopt_long(argc, argv, "td:h", main_options, NULL);
                if (opt < 0)
                        break;

                switch (opt) {
                case 't':
                        tentative = true;
                        break;
                case 'd':
                        dirs = optarg;
                        break;
                case 'h':
                        usage();
                        return EXIT_SUCCESS;
                default:
                        return EXIT_FAILURE;
                }
        }

        r = setup_firmware_dirs(dirs);
        if (r < 0)
                goto out;

        r = manager_new(&manager, tentative);
        if (r < 0) {
                log_error("firmwared %s", strerror(-r));
                goto out;
        }

        r = manager_run(manager);
        if (r < 0) {
                log_error("firmwared %s", strerror(-r));
                goto out;
        }

out:
        cleanup_firmware_dirs();
        return r < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
