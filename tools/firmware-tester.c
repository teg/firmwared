#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdlib.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <glib.h>
#include <gio/gio.h>

#include "tester.h"

#define TEST_FIRMWARE_PATH         "/sys/class/misc/test_firmware"
#define TEST_FIRMWARE_DEV          "/dev/test_firmware"

#define TRIGGER_REQUEST_PATH       TEST_FIRMWARE_PATH "/trigger_request"
#define TRIGGER_ASYNC_REQUEST_PATH TEST_FIRMWARE_PATH "/trigger_async_request"
#define TIMEOUT_PATH               "/sys/class/firmware/timeout"

#define LOAD_PATH_DAEMON           "/tmp"
#define LOAD_PATH_KERNEL           "/lib/firmware"

#define _cleanup_(_x) __attribute__((__cleanup__(_x)))

struct config_data {
        const char *path;
        const char *filename;
        const char *content;
};

struct user_data {
        int fd;
        pid_t pid;
        const struct config_data *cfg;
        ssize_t len;
};

static const struct config_data cfg_kernel = {
        .path =     LOAD_PATH_KERNEL,
        .filename = "kernel.bin",
        .content =  "kernel",
};

static const struct config_data cfg_daemon = {
        .path =     LOAD_PATH_DAEMON,
        .filename = "daemon.bin",
        .content =  "daemon",
};

static const struct config_data cfg_tentative = {
        .path =     LOAD_PATH_DAEMON,
        .filename = "tentative.bin",
        .content =  "tentative",
};


/* -------------------------------------------------------------------- */
/* helper functions */

/* execute firmwared process */
typedef void (*child_func_t)(pid_t pid, int status, void *user_data);

struct exec_data {
        child_func_t func;
        void *user_data;
};

static void child_callback(GPid pid, gint status, gpointer user_data)
{
        struct exec_data *ed = user_data;

        ed->func(pid, status, ed->user_data);
        g_free(ed);
}

static gboolean child_io_callback(GIOChannel *channel,
                        GIOCondition  cond,
                        gpointer user_data)
{
        pid_t pid = GPOINTER_TO_INT(user_data);
        gchar *string;
        gsize  size;

        if (cond == G_IO_HUP) {
                g_io_channel_unref(channel);
                return false;
        }

        g_io_channel_read_line(channel, &string, &size, NULL, NULL);
        tester_print("[%d] %s", pid, g_strdelimit(string, "\n", ' '));
        g_free(string);

        return true;
}

static int daemon_exec(const char *home, char *argv[],
                char *envp[],
                child_func_t func,
                void *user_data)
{
        struct exec_data *ed;
        GPid pid;
        gint in, out, err;
        GIOChannel *cout, *cerr;
        GError *error = NULL;

        if (!g_spawn_async_with_pipes(home, argv, envp,
                                        G_SPAWN_DO_NOT_REAP_CHILD,
                                        NULL, NULL, &pid, &in, &out, &err,
                                        &error)) {
                if (error != NULL) {
                        tester_warn("failed to fork new process: %s",
                                error->message);
                        g_error_free(error);
                } else
                       tester_warn("an unknown error occurred");
                return -1;
        }

        cout = g_io_channel_unix_new(out);
        cerr = g_io_channel_unix_new(err);
        g_io_add_watch(cout, G_IO_IN | G_IO_HUP, child_io_callback,
                GINT_TO_POINTER(pid));
        g_io_add_watch(cerr, G_IO_IN | G_IO_HUP, child_io_callback,
                GINT_TO_POINTER(pid));

        ed = g_new0(struct exec_data, 1);
        ed->func = func;
        ed->user_data = user_data;
        g_child_watch_add(pid, child_callback, ed);

        return pid;
}

/* trigger_load: call write it its own thread */
struct trigger_load_data {
        int fd;
        char *filename;
};

static void trigger_load_data_free(void *user_data)
{
        struct trigger_load_data *data = user_data;
        g_free(data->filename);
        g_free(data);
}

static void trigger_load_function_thread_cb(GTask *task, gpointer source_object,
                                gpointer task_data, GCancellable *cancel)
{
        struct trigger_load_data *data = task_data;
        ssize_t len;

        if (g_task_return_error_if_cancelled(task))
                return;

        len = write(data->fd, data->filename, strlen(data->filename));
        g_task_return_int(task, len);
}

static void trigger_load_async(int fd, const char *filename,
                        GCancellable *cancel,
                        GAsyncReadyCallback callback,
                        gpointer user_data)
{
        GTask *task = NULL;
        struct trigger_load_data *data = NULL;

        task = g_task_new(NULL, cancel, callback, user_data);
        g_task_set_source_tag(task, trigger_load_async);

        g_task_set_return_on_cancel(task, FALSE);
        data = g_new0(struct trigger_load_data, 1);
        data->fd = fd;
        data->filename = g_strdup(filename);
        g_task_set_task_data(task, data, trigger_load_data_free);

        g_task_run_in_thread(task, trigger_load_function_thread_cb);
        g_object_unref(task);
}

static int trigger_load_finish(GAsyncResult *result,
                        GError **error)
{
        return g_task_propagate_int(G_TASK(result), error);
}

/* -------------------------------------------------------------------- */

static void user_data_free(void *data)
{
        struct user_data *user = data;

        free(user);
}

#define test_load(name, setup, func, teardown, config)  \
        do { \
                struct user_data *user; \
                user = calloc(1, sizeof(struct user_data)); \
                if (!user) \
                        break; \
                user->cfg = config; \
                tester_add_full(name, NULL, \
                                NULL, setup, func, teardown, \
                                NULL, 20, user, user_data_free); \
        } while (0)

static int create_firmware(const char *filename, const char *data) {
        ssize_t len;
        int fd;
        int err = 0;

        fd = open(filename, O_CLOEXEC|O_CREAT|O_WRONLY, 0x444);
        if (fd < 0) {
                tester_warn("failed to create firmware: %s",
                        strerror(errno));
                return -1;
        }

        len = write(fd, data, strlen(data));
        if (len < 0) {
                tester_warn("failed to write content to firmware file: %s",
                        strerror(errno));
                err = -1;
        }

        close(fd);
        return err;
}

static void str_freep(char **str) {
        if (*str)
                free(*str);
}

static int set_timeout(int timeout) {
        _cleanup_(str_freep) char *str = NULL;
        ssize_t len;
        int fd;

        fd = open(TIMEOUT_PATH, O_WRONLY);
        if (fd < 0) {
                tester_warn("failed to open timeout file: %s",
                        strerror(errno));
                return -1;
        }

        if (!asprintf(&str, "%d", timeout))
                return -1;
        tester_debug("set timeout to %d", timeout);
        len = write(fd, str, strlen(str));
        if (len < 0) {
                tester_warn("failed to write timeout value: %s",
                        strerror(errno));
        }

        return 0;
}

static void setup_firmware_files(const struct config_data *cfg) {
        _cleanup_(str_freep) char *fullpath = NULL;

        if (!asprintf(&fullpath, "%s/%s", cfg->path, cfg->filename))
                return;

        tester_debug("create firmware file %s", fullpath);
        if (create_firmware(fullpath, cfg->content) < 0) {
                tester_test_failed();
                return;
        }
}

static void cleanup_firmware_files(const struct config_data *cfg) {
        _cleanup_(str_freep) char *fullpath = NULL;

        if (!asprintf(&fullpath, "%s/%s", cfg->path, cfg->filename))
                return;

        unlink(fullpath);
}

static void closep(int *fd) {
        if (*fd >= 0)
                close(*fd);
}

static bool check_content(const char *filename, const char *content) {
        _cleanup_(closep) int  fd;
        _cleanup_(str_freep) char *buf = NULL;
        ssize_t buf_len;
        ssize_t len;

        fd = open(filename, O_CLOEXEC|O_RDONLY);
        if (fd < 0) {
                tester_debug("could not open %s", filename);
                return false;
        }

        buf_len = strlen(content);
        buf = malloc(buf_len + 1);
        if (!buf)
                return false;

        memset(buf, 0, buf_len + 1);

        len = read(fd, buf, buf_len);
        if (len != buf_len) {
                tester_debug("could not read content %s", filename);
                return false;
        }

        tester_debug("content expected '%s'", content);
        tester_debug("content read     '%s'", buf);

        return !strcmp(buf, content);
}

static void daemon_callback(pid_t pid, int status, void *user_data)
{
        if (WIFEXITED(status))
                tester_print("process %d exited with status %d",
                        pid, WEXITSTATUS(status));
        else if (WIFSIGNALED(status))
                tester_print("process %d terminated with signal %d",
                        pid, WTERMSIG(status));
        else if (WIFSTOPPED(status))
                tester_print("rocess %d stopped with signal %d",
                        pid, WSTOPSIG(status));
        else if (WIFCONTINUED(status))
                tester_print("process %d continued", pid);
}

static const char *daemon_table[] = {
        "firmwared",
        "/usr/sbin/firmwared",
        NULL
};

static pid_t run_daemon(const char *path, bool tentative) {
        const char *home;
        const char *daemon = NULL;
        char *argv[5], *envp[1];
        pid_t pid;
        int i, pos;

        home = getenv("HOME");
        if (chdir(home) < 0) {
                tester_warn("failed to change home directory for daemon: %s",
                        strerror(errno));
                return -1;
        }

        for (i = 0; daemon_table[i]; i++) {
                struct stat st;

                if (!stat(daemon_table[i], &st)) {
                        daemon = daemon_table[i];
                        break;
                }
        }

        if (!daemon) {
                tester_warn("failed to locate firmware daemon binary: %s",
                        strerror(errno));
                return -1;
        }

        pos = 0;
        argv[pos++] = (char *) daemon;
        if (path) {
                argv[pos++] = "--dirs";
                argv[pos++] = (char *) path;
        }
        if (tentative)
                argv[pos++] = "--tentative";
        argv[pos] = NULL;

        envp[0] = NULL;

        if (tester_use_debug()) {
                tester_debug("start firmwared");
                tester_debug("  HOME %s", home);
                for (i = 0; i < pos; i++)
                        tester_debug("  argv[%d] %s", i, argv[i]);
        }
        pid = daemon_exec(home, argv, envp, daemon_callback, NULL);
        if (pid < 0)
                return -1;

        tester_print("firmware daemon process %d created", pid);

        return pid;
}

/* -------------------------------------------------------------------- */
/* daemon tests */

static void test_firmware_load(const void *test_data) {
        struct user_data *user = tester_get_data();
        ssize_t len;

        tester_debug("trigger load of %s", user->cfg->filename);
        len = write(user->fd, user->cfg->filename, strlen(user->cfg->filename));
        if (len < 0) {
                tester_warn("failed to load firmware %s: %s",
                        user->cfg->filename, strerror(errno));
                tester_test_failed();
                return;
        }

        if (!check_content(TEST_FIRMWARE_DEV, user->cfg->content)) {
                tester_warn("content is not matching");
                tester_test_failed();
        }

        tester_test_passed();
}

static void _setup_daemon_load(struct user_data *user, const char *trigger_path)
{

        user->pid = run_daemon(user->cfg->path, false);
        if (user->pid < 0) {
                tester_warn("failed to start daemon");
                tester_setup_failed();
                return;
        }

        user->fd = open(trigger_path, O_CLOEXEC|O_WRONLY);
        if (user->fd < 0) {
                tester_warn("failed to open %s: %s", TRIGGER_REQUEST_PATH,
                        strerror(errno));
                tester_setup_failed();
                return;
        }

        if (set_timeout(2) < 0) {
                tester_warn("could not set timeout");
                tester_test_failed();
                return;
        }

        tester_setup_complete();
}

static void setup_daemon_sync_load(const void *test_data) {
        struct user_data *user = tester_get_data();
        _setup_daemon_load(user, TRIGGER_REQUEST_PATH);
}

static void setup_daemon_async_load(const void *test_data) {
        struct user_data *user = tester_get_data();
        _setup_daemon_load(user, TRIGGER_ASYNC_REQUEST_PATH);
}

static void teardown_daemon_load(const void *test_data) {
        struct user_data *user = tester_get_data();

        close(user->fd);
        kill(user->pid, SIGTERM);
        tester_teardown_complete();
}

/* -------------------------------------------------------------------- */
/* tentative test */

static void test_tentative_daemon_reload(void *user_data) {
        struct user_data *user = user_data;

        tester_debug("kill daemon");
        kill(user->pid, SIGTERM);
        setup_firmware_files(&cfg_tentative);

        tester_debug("restart daemon in non tentative mode");
        user->pid = run_daemon(user->cfg->path, false);
}


static void test_tentative_trigger_cb(GObject *source_object,
                                GAsyncResult *res,
                                gpointer user_data)
{
        struct user_data *user = user_data;
        int err;

        err = trigger_load_finish(res, NULL);

        if (err < 0) {
                tester_warn("trigger load failed: %s", strerror(err));
                tester_test_failed();
                return;
        }

        if (!check_content(TEST_FIRMWARE_DEV, user->cfg->content)) {
                tester_warn("content is not matching");
                tester_test_failed();
                return;
        }

        tester_test_passed();
}

static void test_tentative_load(const void *test_data) {
        struct user_data *user = tester_get_data();

        tester_wait(2, test_tentative_daemon_reload, user);
        trigger_load_async(user->fd, user->cfg->filename, NULL,
                        test_tentative_trigger_cb, (gpointer)user);
}

static void setup_tentative_load(const void *test_data) {
        struct user_data *user = tester_get_data();

        cleanup_firmware_files(user->cfg);

        user->pid = run_daemon(user->cfg->path, true);
        set_timeout(10);

        user->fd = open(TRIGGER_REQUEST_PATH, O_CLOEXEC|O_WRONLY);
        if (user->fd < 0) {
                tester_warn("failed to open %s: %s", TRIGGER_REQUEST_PATH,
                        strerror(errno));
                tester_setup_failed();
                return;
        }

        tester_setup_complete();
}

/* -------------------------------------------------------------------- */
/* kernel tests */

static void _setup_kernel_load(struct user_data *user, const char *trigger_path)
{

        user->fd = open(trigger_path, O_CLOEXEC|O_WRONLY);
        if (user->fd < 0) {
                tester_warn("failed to open %s: %s", TRIGGER_REQUEST_PATH,
                        strerror(errno));
                tester_setup_failed();
                return;
        }

        if (set_timeout(2) < 0) {
                tester_warn("could not set timeout");
                tester_test_failed();
                return;
        }

        tester_setup_complete();
}

static void setup_kernel_sync_load(const void *test_data) {
        struct user_data *user = tester_get_data();
        _setup_kernel_load(user, TRIGGER_REQUEST_PATH);
}

static void setup_kernel_async_load(const void *test_data) {
        struct user_data *user = tester_get_data();
        _setup_kernel_load(user, TRIGGER_ASYNC_REQUEST_PATH);
}

static void teardown_kernel_load(const void *test_data) {
        struct user_data *user = tester_get_data();

        close(user->fd);
        tester_teardown_complete();
}

/* -------------------------------------------------------------------- */

int main(int argc, char *argv[]) {
        tester_init(&argc, &argv);

        setup_firmware_files(&cfg_daemon);
        setup_firmware_files(&cfg_kernel);

        test_load("Load via deamon",
                setup_daemon_sync_load, test_firmware_load,
                teardown_daemon_load, &cfg_daemon);
        test_load("Load via dameon async",
                setup_daemon_async_load, test_firmware_load,
                teardown_daemon_load, &cfg_daemon);
        test_load("Load via daemon tentative mode",
                setup_tentative_load, test_tentative_load,
                teardown_daemon_load, &cfg_tentative);
        test_load("Load via kernel",
                setup_kernel_sync_load, test_firmware_load,
                teardown_kernel_load, &cfg_kernel);
        test_load("Load via kernel async",
                setup_kernel_async_load, test_firmware_load,
                teardown_kernel_load, &cfg_kernel);

        return tester_run();
}
