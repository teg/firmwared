#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#include "manager.h"
#include "log-util.h"

static void usage(void) {
	printf("firmwared - Linux Firmware Loader Daemon\n"
		"Usage:\n");
	printf("\tfirmwared [options] [--] <command> [args]\n");
	printf("Options:\n"
		"\t-t, --tentative        Defer loading of non existing firmwares\n"
		"\t-d, --dir [path]       Firmware loading path\n"
		"\t-h, --help             Show help options\n");
}

static const struct option main_options[] = {
	{ "tentative",     no_argument,       NULL, 't' },
	{ "dir",           required_argument, NULL, 'd' },
	{ "help",          no_argument,       NULL, 'h' },
	{ }
};

int main(int argc, char **argv) {
        _cleanup_(manager_freep) Manager *manager = NULL;
        bool tentative = false;
        const char *firmware_path = NULL;
        int r;

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
                        firmware_path = optarg;
                        break;
                case 'h':
                        usage();
                        return EXIT_SUCCESS;
                default:
                        return EXIT_FAILURE;
                }
        }

        setbuf(stdout, NULL);

        r = manager_new(&manager, tentative, firmware_path);
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
