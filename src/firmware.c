#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <unistd.h>

#include "firmware.h"

#define LOADING_START   (1)
#define LOADING_CANCEL  (-1)
#define LOADING_FINISH  (0)

static int firmware_set_loading(int loadingfd, int state) {
        int r;

        r = dprintf(loadingfd, "%d\n", state);
        if (r < 0)
                return -errno;

        return 0;
}

int firmware_load(int devicefd, int firmwarefd, bool tentative) {
        int loadingfd = -1, datafd = -1;
        struct stat statbuf;
        bool started = false;
        int r;

        loadingfd = openat(devicefd, "loading", O_CLOEXEC|O_WRONLY);
        if (loadingfd < 0) {
                r = -errno;
                goto finish;
        }

        datafd = openat(devicefd, "data", O_CLOEXEC|O_WRONLY);
        if (datafd < 0) {
                r = -errno;
                goto finish;
        }

        if (fstat(firmwarefd, &statbuf) < 0) {
                r = -errno;
                goto finish;
        }

        if (statbuf.st_size == 0) {
                r = -EIO;
                goto finish;
        }

        r = firmware_set_loading(loadingfd, LOADING_START);
        if (r < 0)
                goto finish;

        started = true;

        while (statbuf.st_size) {
                ssize_t size;
                off_t offset = 0;

                size = sendfile(datafd, firmwarefd, &offset, statbuf.st_size);
                if (size < 0) {
                        r = -errno;
                        goto finish;
                }

                statbuf.st_size -= size;
        }

        firmware_set_loading(loadingfd, LOADING_FINISH);

finish:
        if (loadingfd >= 0)
                close(loadingfd);
        if (datafd >= 0)
                close(datafd);
        if (r < 0 && r != -ENOENT && (!tentative || started)) {
                firmware_set_loading(loadingfd, LOADING_CANCEL);
                return r;
        } else
                return 0;
}

int firmware_cancel_load(int devicefd) {
        int loadingfd = -1;
        int r;

        loadingfd = openat(devicefd, "loading", O_CLOEXEC|O_WRONLY);
        if (loadingfd < 0) {
                r = -errno;
                goto finish;
        }

        r = firmware_set_loading(loadingfd, LOADING_CANCEL);
        if (r < 0)
                goto finish;

finish:
        if (loadingfd >= 0)
                close(loadingfd);
        if (r < 0 && r != -ENOENT)
                return r;
        else
                return 0;
}
