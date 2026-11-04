/* Temporary ALSA Sequencer source for Akai CV Program / Mod Wheel CV.
 * Uses the kernel event ABI and resolves alsa-lib functions at runtime. */
#define _POSIX_C_SOURCE 200809L
#include <dlfcn.h>
#include <poll.h>
#include <signal.h>
#include <sound/asequencer.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

typedef struct _snd_seq Seq;
static volatile sig_atomic_t stop;
static void on_signal(int signal_number) { (void)signal_number; stop = 1; }

static int send_level(Seq *seq, int port,
                      int (*output)(Seq *, struct snd_seq_event *), unsigned level) {
    struct snd_seq_event ev = {0};
    ev.type = SNDRV_SEQ_EVENT_CONTROLLER;
    ev.queue = SNDRV_SEQ_QUEUE_DIRECT;
    ev.source.port = (unsigned char)port;
    ev.dest.client = SNDRV_SEQ_ADDRESS_SUBSCRIBERS;
    ev.dest.port = SNDRV_SEQ_ADDRESS_UNKNOWN;
    ev.data.control.param = 1;
    ev.data.control.value = (int)((level * 127 + 2) / 5);
    int result = output(seq, &ev);
    if (result < 0)
        fprintf(stderr, "ALSA rejected CC1 value %d (%d)\n", ev.data.control.value, result);
    else
        printf("CC1 value %d sent to subscribers; nominal %u V\n", ev.data.control.value, level);
    return result;
}

int main(void) {
    void *lib = dlopen("libasound.so.2", RTLD_NOW | RTLD_LOCAL);
    if (!lib) { fprintf(stderr, "%s\n", dlerror()); return 1; }
    int (*open_seq)(Seq **, const char *, int, int) = dlsym(lib, "snd_seq_open");
    int (*close_seq)(Seq *) = dlsym(lib, "snd_seq_close");
    int (*name)(Seq *, const char *) = dlsym(lib, "snd_seq_set_client_name");
    int (*create)(Seq *, const char *, unsigned, unsigned) = dlsym(lib, "snd_seq_create_simple_port");
    int (*output)(Seq *, struct snd_seq_event *) = dlsym(lib, "snd_seq_event_output_direct");
    Seq *seq = NULL;
    if (!open_seq || !close_seq || !name || !create || !output || open_seq(&seq, "hw", 1, 1) < 0) {
        fprintf(stderr, "ALSA Sequencer unavailable\n");
        dlclose(lib);
        return 1;
    }
    int port = name(seq, "MPC-CV LAB Probe") < 0 ? -1 : create(seq, "CV1 diagnostic",
        SNDRV_SEQ_PORT_CAP_READ | SNDRV_SEQ_PORT_CAP_SUBS_READ,
        SNDRV_SEQ_PORT_TYPE_MIDI_GENERIC | SNDRV_SEQ_PORT_TYPE_APPLICATION);
    int status = 1;
    if (port >= 0) {
        signal(SIGINT, on_signal);
        signal(SIGTERM, on_signal);
        setvbuf(stdout, NULL, _IOLBF, 0);
        puts("READY: route only to the test CV Program. Keys 0–3 set level, q quits.");
        int requested = 0;
        int pending_zero = 0;
        status = 0;
        while (!stop) {
            struct pollfd input = {STDIN_FILENO, POLLIN, 0};
            int result = poll(&input, 1, pending_zero ? 30000 : -1);
            if (result <= 0) break;
            char command;
            if (read(STDIN_FILENO, &command, 1) != 1 || command == 'q') break;
            if (command < '0' || command > '3') continue;
            requested = 1;
            unsigned level = (unsigned)(command - '0');
            if (send_level(seq, port, output, level) < 0) { status = 1; break; }
            pending_zero = level != 0;
        }
        if (requested) {
            int result = -1;
            for (int attempt = 0; attempt < 3 && result < 0; ++attempt) {
                if (attempt) nanosleep(&(struct timespec){0, 20000000}, NULL);
                result = send_level(seq, port, output, 0);
            }
            if (result < 0) status = 1;
        }
    } else {
        fprintf(stderr, "Could not create virtual MIDI port\n");
    }
    close_seq(seq);
    dlclose(lib);
    return status;
}
