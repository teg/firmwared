#include <errno.h>
#include <fcntl.h>
#include <libudev.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/signalfd.h>
#include <sys/utsname.h>
#include <unistd.h>

#include "firmware.h"
#include "manager.h"
#include "log-util.h"

#define ELEMENTSOF(x) (sizeof(x)/sizeof(x[0]))

const char* firmware_dirs[] = {
        FIRMWARE_PATH,
        NULL
};

struct Manager {
        struct udev *udev;
        struct udev_monitor *udev_monitor;
        int *firmwaredirfds;
        int devicesfd;
        int signalfd;
        int epollfd;
        bool tentative;
};

int manager_new(Manager **managerp, bool tentative, const char *path) {
        _cleanup_(manager_freep) Manager *m = NULL;
        struct utsname kernel;
        struct epoll_event ep_udev = { .events = EPOLLIN };
        struct epoll_event ep_signal = { .events = EPOLLIN };
        sigset_t mask;
        int r;

        /* the last entry is reserverd for runtime configuration */
        firmware_dirs[ELEMENTSOF(firmware_dirs) - 1] = path;

        m = calloc(sizeof(*m) + 2 * sizeof(int) * ELEMENTSOF(firmware_dirs), 1);
        if (!m)
                return -ENOMEM;

        m->tentative = tentative;
        m->devicesfd = -1;
        m->signalfd = -1;
        m->epollfd = -1;
        m->firmwaredirfds = (int*)(m + 1);

        r = uname(&kernel);
        if (r < 0)
                return -errno;

        for (unsigned int i = 0; i < ELEMENTSOF(firmware_dirs); i ++) {
                if (firmware_dirs[i]) {
                        m->firmwaredirfds[2 * i] = openat(AT_FDCWD, firmware_dirs[i], O_RDONLY|O_NONBLOCK|O_DIRECTORY|O_CLOEXEC|O_PATH);
                        m->firmwaredirfds[2 * i + 1] = openat(m->firmwaredirfds[2 * i], kernel.release, O_RDONLY|O_NONBLOCK|O_DIRECTORY|O_CLOEXEC|O_PATH);
                } else {
                        m->firmwaredirfds[2 * i] = -1;
                        m->firmwaredirfds[2 * i + 1] = -1;
                }
        }

        m->devicesfd = openat(AT_FDCWD, "/sys/devices", O_RDONLY|O_NONBLOCK|O_DIRECTORY|O_CLOEXEC|O_PATH);
        if (m->devicesfd < 0)
                return -errno;

        m->udev = udev_new();
        if (!m->udev)
                return -errno;

        m->udev_monitor = udev_monitor_new_from_netlink(m->udev, "kernel");
        if (!m->udev_monitor)
                return -errno;

        r = udev_monitor_filter_add_match_subsystem_devtype(m->udev_monitor, "firmware", NULL);
        if (r < 0)
                return r;

        sigemptyset(&mask);
        sigaddset(&mask, SIGTERM);
        sigaddset(&mask, SIGINT);
        sigprocmask(SIG_BLOCK, &mask, NULL);

        m->signalfd = signalfd(-1, &mask, SFD_NONBLOCK|SFD_CLOEXEC);
        if (m->signalfd < 0)
                return -errno;

        m->epollfd = epoll_create1(EPOLL_CLOEXEC);
        if (m->epollfd < 0)
                return -errno;

        ep_udev.data.fd = udev_monitor_get_fd(m->udev_monitor);
        ep_signal.data.fd = m->signalfd;

        if (epoll_ctl(m->epollfd, EPOLL_CTL_ADD, udev_monitor_get_fd(m->udev_monitor), &ep_udev) < 0 ||
            epoll_ctl(m->epollfd, EPOLL_CTL_ADD, m->signalfd, &ep_signal))
                return -errno;

        *managerp = m;
        m = NULL;

        return 0;
}

void manager_free(Manager *m) {
        if (m->epollfd >= 0)
                close(m->epollfd);
        if (m->signalfd >= 0)
                close(m->signalfd);
        udev_monitor_unref(m->udev_monitor);
        udev_unref(m->udev);
        if (m->devicesfd >= 0)
                close(m->devicesfd);
        for (unsigned int i = 0; i < 2 * ELEMENTSOF(firmware_dirs); i ++)
                if (m->firmwaredirfds[i] >= 0)
                        close(m->firmwaredirfds[i]);
        free(m);
}

static int manager_find_firmware(Manager *manager, const char *name) {
        int firmwarefd;

        for (unsigned int i = 0; i < 2 * ELEMENTSOF(firmware_dirs); i ++) {
                firmwarefd = openat(manager->firmwaredirfds[i], name, O_RDONLY|O_NONBLOCK|O_CLOEXEC);
                if (firmwarefd >= 0)
                        return firmwarefd;
        }

        log_info("firmware '%s' not found", name);
        return -ENOENT;
}

static void closep(int *fdp) {
        if (*fdp >= 0)
                close(*fdp);
}

static int manager_handle_device(Manager *manager, struct udev_device *device) {
        _cleanup_(closep) int devicefd = -1, firmwarefd = -1;
        const char *name;
        int r;

        devicefd = openat(manager->devicesfd, udev_device_get_syspath(device), O_RDONLY|O_NONBLOCK|O_DIRECTORY|O_CLOEXEC|O_PATH);
        if (devicefd < 0)
                return errno == ENOENT ? 0 : -errno;

        name = udev_device_get_property_value(device, "FIRMWARE");
        firmwarefd = manager_find_firmware(manager, name);
        if (firmwarefd >= 0) {
                log_info("load firmware %s", name);
                r = firmware_load(devicefd, firmwarefd, manager->tentative);
                if (r < 0)
                        return r;
        } else if (!manager->tentative) {
                log_info("cancel firmware load %s", name);
                r = firmware_cancel_load(devicefd);
                if (r < 0)
                        return r;
        }

        return 0;
}

static void udev_enumerate_unrefp(struct udev_enumerate **enumeratep) {
        if (*enumeratep)
                udev_enumerate_unref(*enumeratep);
}

static void udev_device_unrefp(struct udev_device **devicep) {
        if (*devicep)
                udev_device_unref(*devicep);
}

int manager_run(Manager *manager) {
        _cleanup_(udev_enumerate_unrefp) struct udev_enumerate *enumerate = NULL;
        struct udev_list_entry *device_list, *device_entry;
        int r;

        enumerate = udev_enumerate_new(manager->udev);
        if (!enumerate)
                return -errno;

        r = udev_enumerate_add_match_subsystem(enumerate, "firmware");
        if (r < 0)
                return r;

        r = udev_monitor_enable_receiving(manager->udev_monitor);
        if (r < 0)
                return r;

        r = udev_enumerate_scan_devices(enumerate);
        if (r < 0)
                return r;

        device_list = udev_enumerate_get_list_entry(enumerate);
        udev_list_entry_foreach(device_entry, device_list) {
                _cleanup_(udev_device_unrefp) struct udev_device *device = NULL;

                device = udev_device_new_from_syspath(manager->udev, udev_list_entry_get_name(device_entry));
                if (!device)
                        continue;

                r = manager_handle_device(manager, device);
                if (r < 0)
                        return r;
        }

        udev_enumerate_unref(enumerate);
        enumerate = NULL;

        for (;;) {
                struct epoll_event ev;
                int n;

                n = epoll_wait(manager->epollfd, &ev, 1, -1);
                if (n < 0) {
                        if (errno == EINTR)
                                continue;

                        return -errno;
                } else if (n == 0)
                        continue;

                if (ev.data.fd == manager->signalfd &&
                    ev.events & EPOLLIN) {
                        struct signalfd_siginfo fdsi;
                        ssize_t size;

                        size = read(manager->signalfd, &fdsi, sizeof(fdsi));
                        if (size != sizeof(fdsi))
                                continue;

                        if (fdsi.ssi_signo != SIGTERM && fdsi.ssi_signo != SIGINT)
                                continue;

                        return 0;
                }

                if (ev.data.fd == udev_monitor_get_fd(manager->udev_monitor) &&
                    ev.events & EPOLLIN) {
                        for (;;) {
                                _cleanup_(udev_device_unrefp) struct udev_device *device = NULL;

                                device = udev_monitor_receive_device(manager->udev_monitor);
                                if (!device)
                                        break;

                                if (strcmp(udev_device_get_action(device), "add") &&
                                    strcmp(udev_device_get_action(device), "move"))
                                        continue;

                                r = manager_handle_device(manager, device);
                                if (r < 0)
                                        return r;
                        }
                }
        }

        return 0;
}
