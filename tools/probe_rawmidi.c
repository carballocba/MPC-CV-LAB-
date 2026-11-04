/* Linux diagnostic: inspect existing write descriptors without opening a MIDI endpoint.
 * pidfd_getfd takes a temporary reference to the existing open file description.
 * Do not use this program to read, write, drain or change descriptor flags. */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <sound/asound.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <unistd.h>

static int number(const char *arg, long minimum) {
    char *end;
    errno = 0;
    long n = strtol(arg, &end, 10);
    return errno || end == arg || *end || n < minimum || n > INT_MAX ? -1 : (int)n;
}

int main(int argc, char **argv) {
    int pid = argc > 2 ? number(argv[1], 1) : -1;
    if (pid < 0) {
        fprintf(stderr, "usage: %s PID FD [FD ...]\n", argv[0]);
        return 2;
    }
    int process = (int)syscall(SYS_pidfd_open, pid, 0);
    if (process < 0) {
        perror("pidfd_open");
        return 1;
    }
    int failed = 0;
    for (int i = 2; i < argc; ++i) {
        int original = number(argv[i], 0);
        if (original < 0) {
            fprintf(stderr, "invalid fd: %s\n", argv[i]);
            failed = 1;
            continue;
        }
        int fd = (int)syscall(SYS_pidfd_getfd, process, original, 0);
        if (fd < 0) {
            fprintf(stderr, "fd %d: pidfd_getfd: %s\n", original, strerror(errno));
            failed = 1;
            continue;
        }
        char path[64], target[256];
        snprintf(path, sizeof path, "/proc/self/fd/%d", fd);
        ssize_t length = readlink(path, target, sizeof target - 1);
        if (length >= 0) target[length] = 0;
        struct snd_rawmidi_info info = {.stream = SNDRV_RAWMIDI_STREAM_OUTPUT};
        if (length < 0 || strncmp(target, "/dev/snd/midiC", 14) != 0) {
            fprintf(stderr, "fd %d: no longer a raw MIDI descriptor\n", original);
            failed = 1;
        } else if (ioctl(fd, SNDRV_RAWMIDI_IOCTL_INFO, &info) < 0) {
            fprintf(stderr, "fd %d: INFO: %s\n", original, strerror(errno));
            failed = 1;
        } else {
            printf("fd %d: card=%d device=%u substream=%u name=%.*s\n",
                   original, info.card, info.device, info.subdevice,
                   (int)sizeof info.subname, (const char *)info.subname);
        }
        close(fd);
    }
    close(process);
    return failed;
}
