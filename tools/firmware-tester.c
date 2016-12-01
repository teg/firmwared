#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "tester.h"

#define TRIGGER_REQUEST "/sys/class/misc/test_firmware/trigger_request"
#define TRIGGER_ASYNC_REQUEST "/sys/class/misc/test_firmware/trigger_async_request"

struct user_data {
        int fd;
};

static void user_data_free(void *data)
{
        struct user_data *user = data;

        free(user);
}

#define test_load(name, setup, func, teardown) \
        do { \
                struct user_data *user; \
                user = calloc(1, sizeof(struct user_data)); \
                if (!user) \
                        break; \
                tester_add_full(name, NULL, \
                                NULL, setup, func, teardown, \
                                NULL, 2, user, user_data_free); \
        } while (0)

static void test_firmware_load(const void *test_data) {
        struct user_data *user = tester_get_data();
        const char *filename = "foobar.bin";
        ssize_t len;

        len = write(user->fd, filename, strlen(filename));
        if (len < 0) {
                tester_warn("Failed to load firmware %s: %s",
                        filename, strerror(errno));
                tester_test_failed();
                return;
        }
        tester_test_passed();
}

static void setup_sync_load(const void *test_data) {
        struct user_data *user = tester_get_data();

        user->fd = open(TRIGGER_REQUEST, O_CLOEXEC|O_WRONLY);
        if (user->fd < 0) {
                tester_warn("Failed to open %s: %s", TRIGGER_REQUEST,
                        strerror(errno));
                tester_setup_failed();
                return;
        }

        tester_setup_complete();
}

static void setup_async_load(const void *test_data) {
        struct user_data *user = tester_get_data();

        user->fd = open(TRIGGER_ASYNC_REQUEST, O_CLOEXEC|O_WRONLY);
        if (user->fd < 0) {
                tester_warn("Failed to open %s: %s", TRIGGER_ASYNC_REQUEST,
                        strerror(errno));
                tester_setup_failed();
                return;
        }
        tester_setup_complete();
}

static void teardown_load(const void *test_data) {
        struct user_data *user = tester_get_data();

        close(user->fd);
        tester_teardown_complete();
}

static void create_firmware() {
        const char *data = "narf";
        ssize_t len;
        int fd;

        fd = open("/tmp/foobar.bin", O_CLOEXEC|O_CREAT|O_WRONLY, 0x444);
        if (fd < 0) {
                tester_warn("Failed to create firmware: %s",
                        strerror(errno));
                tester_test_failed();
                return;
        }

        len = write(fd, data, strlen(data));
        if (len < 0) {
                tester_warn("Failed to write content to firmware file: %s",
                        strerror(errno));
                tester_test_failed();
                close(fd);
                return;
        }
        close(fd);
        tester_test_passed();
}

int main(int argc, char *argv[]) {

        tester_init(&argc, &argv);

        test_load("Create firmware", NULL, create_firmware, NULL);

        test_load("Load firmware", setup_sync_load, test_firmware_load, teardown_load);
        test_load("Load firmware async", setup_async_load, test_firmware_load, teardown_load);

        return tester_run();
}
