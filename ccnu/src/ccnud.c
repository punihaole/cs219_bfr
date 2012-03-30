#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>

#include "ccnud_listener.h"
#include "ccnud_net_broadcaster.h"
#include "ccnud_net_listener.h"
#include "ccnud_cs.h"
#include "ccnud_pit.h"
#include "ccnud_stats.h"
#include "ccnud.h"
#include "ccnu.h"
#include "net_lib.h"
#include "log.h"

#include "bfr.h"

#include "synch_queue.h"

extern int ccnu_net_test();

struct log * g_log;
uint32_t g_nodeId;
int g_timeout_ms;
int g_interest_attempts;
pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;

static pthread_t idp_listener;
static pthread_t net_listener;
static pthread_mutex_t ccnud_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  ccnud_var   = PTHREAD_COND_INITIALIZER;
static struct listener_args ipc_args;
static struct listener_args net_args;

void print_usage(char * argv_0)
{
    printf("ccnud v0.0.0.1a\n");
    printf("Usage: %s -h\n", argv_0);
    printf("      -h\tShow this help screen.\n");
    printf("      -t\tRun networking test (ccnud must be running).\n");
    printf("      -l\tSpecify a log file.\n");
    printf("      -n\tSpecify a node Id.\n");
    printf("      -p\tSpecify the target false positive rate for the Bloom filter size.\n");
    printf("      -i\tSpecify the interest pipeline size.\n");
    printf("      -s\tSpecify a stat log file\n");
    printf("The ccnud drops all output capability, so test results must be\n");
    printf("grepped from the log file. Running ccnud without the -t flag \n");
    printf("starts the daemon.\n");
}

void signal_handler(int signal)
{
    switch(signal) {
        case SIGHUP:
            log_print(g_log, "Received SIGHUP signal.");
            break;
        case SIGTERM:
            log_print(g_log, "Received SIGTERM signal.");
            /* do cleanup */
            exit(EXIT_SUCCESS);
            break;
        default:
            log_print(g_log, "Unhandled signal (%d) %s", signal, strsignal(signal));
            break;
    }
}

int main(int argc, char *argv[])
{
    pid_t pid, sid;
    char log_file[256];
    char stat_file[256];
    int log_file_set = 0, stat_file_set = 0;
    double p = DEFAULT_P;
    int interest_pipeline = DEFAULT_INTEREST_PIPELINE;

    g_nodeId = DEFAULT_NODE_ID;
    g_timeout_ms = DEFAULT_INTEREST_TIMEOUT_MS;
    g_interest_attempts = DEFAULT_INTEREST_MAX_ATTEMPTS;

    signal(SIGHUP, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);
    signal(SIGQUIT, signal_handler);

    int c;
    while ((c = getopt(argc, argv, "-h?tl:n:p:i:s:")) != -1) {
        switch (c) {
            case 'h':
                print_usage(argv[0]);
                exit(EXIT_SUCCESS);
                break;
            case '?':
                print_usage(argv[0]);
                exit(EXIT_SUCCESS);
                break;
            case 'i':
                interest_pipeline = atoi(optarg);
                fprintf(stderr, "set interest pipeline size to %d.\n", interest_pipeline);
                break;
            case 'l':
                strncpy(log_file, optarg, 256);
                printf("set log file = %s.\n", log_file);
                log_file_set = 1;
                break;
            case 'n':
                g_nodeId = atoi(optarg);
                printf("set node ID = %u.\n", g_nodeId);
                break;
            case 'p':
                p = atof(optarg);
                if (p >= 1 || p == 0) {
                    fprintf(stderr, "p must be 0 < p < 1, defaulting to %1.4f\n", DEFAULT_P);
                    p = DEFAULT_P;
                } else {
                    printf("set p to %1.4f...\n", p);
                }
                break;
            case 's':
                strncpy(stat_file, optarg, 256);
                printf("set stat file = %s.\n", stat_file);
                stat_file_set = 1;
                break;
            case 't':
                printf("Running tests (will execute tests and terminate ");
                printf("without forking a daemon)...\n");
                if (ccnu_net_test() == 0) {
                    printf("Done. Check log for details.\n");
                    exit(EXIT_SUCCESS);
                } else {
                    fprintf(stderr, "Failed. Check log for details ");
                    fprintf(stderr, "-- make sure another ccnud is running.\n");
                    exit(EXIT_FAILURE);
                }
                break;
            default:
                print_usage(argv[0]);
                exit(EXIT_SUCCESS);
                break;
        }
    }

    fprintf(stderr, "Starting ccnud...");

    pid = fork();
    if (pid < 0) {
        exit(EXIT_FAILURE);
    }

    if (pid > 0) {
        /* parent exits */
        fprintf(stderr, "Done. pid = %d\n", pid);
        exit(EXIT_SUCCESS);
    }

    umask(0); /* os calls provide their own permissions */

    sid = setsid();
    if (sid < 0) {
        fprintf(stderr, "setsid: %s.", strerror(errno));
        exit(EXIT_FAILURE);
    }

    g_log = (struct log * ) malloc(sizeof(struct log));
    char log_name[256];
    snprintf(log_name, 256, "ccnud_%u", g_nodeId);
    if (!log_file_set)
        snprintf(log_file, 256, "~/log/ccnud_%u.log", g_nodeId);

    if (log_init(log_name, log_file, g_log, LOG_OVERWRITE) < 0) {
        fprintf(stderr, "ccnud log: %s failed to initalize!", log_file);
        exit(EXIT_FAILURE);
    }

    if (!stat_file_set)
        snprintf(stat_file, 256, "~/stat/ccnud_%u.stat", g_nodeId);
	stat_file[255] = '\0';
    if (ccnustat_init(stat_file) < 0) {
        fprintf(stderr, "ccnud stat: %s failed to initalize!", stat_file);
        exit(EXIT_FAILURE);
    }

    /* close out the std io file handles */
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
    /* change the current working directory (after log is inited) */
    if ((chdir("/")) < 0) {
        log_print(g_log, "chdir: %s.", strerror(errno));
        exit(EXIT_FAILURE);
    }

    /* initalize CS, PIT, FIB */
    if (CS_init(OLDEST, p) != 0) {
        log_print(g_log, "failed to create CS! - EXITING");
        exit(EXIT_FAILURE);
    }

    if (PIT_init() != 0) {
        log_print(g_log, "failed to create PIT! - EXITING");
        exit(EXIT_FAILURE);
    }

    if (ccnudl_init(interest_pipeline) < 0) {
        log_print(g_log, "ccnud listener failed to initalize.");
        exit(EXIT_FAILURE);
    }

    if (ccnudnl_init(interest_pipeline) < 0) {
        log_print(g_log, "ccnud net listener failed to initalize.");
        exit(EXIT_FAILURE);
    }

    if (ccnudnb_init() < 0) {
        log_print(g_log, "ccnud net broadcaster failed to initalize.");
        exit(EXIT_FAILURE);
    }

    /* initialize the ipc listener */
    ipc_args.lock = &ccnud_mutex;
    ipc_args.cond = &ccnud_var;
    ipc_args.queue = synch_queue_init();

    pthread_create(&idp_listener, NULL, ccnudl_service, &ipc_args);

    /* initialize the net listener */
    net_args.lock = &ccnud_mutex;
    net_args.cond = &ccnud_var;
    net_args.queue = synch_queue_init();

    pthread_create(&net_listener, NULL, ccnudnl_service, &net_args);

    while (1) {
        /* for efficiency we sleep until someone wakes us up */
        pthread_mutex_lock(&ccnud_mutex);
            pthread_cond_wait(&ccnud_var, &ccnud_mutex);
        pthread_mutex_unlock(&ccnud_mutex);

        while (synch_len(ipc_args.queue)) {
            struct ccnud_msg * msg = synch_dequeue(ipc_args.queue);
            switch (msg->type) {
                case MSG_IPC_TIMEOUT:
                    if (msg->payload_size != sizeof(uint32_t)) {
                        log_print(g_log, "ccnud: malformed MSG_IPC_TIMEOUT");
                    }
                    pthread_mutex_lock(&g_lock);
                    memcpy(&g_timeout_ms, msg->payload, sizeof(uint32_t));
                    pthread_mutex_unlock(&g_lock);
                    log_print(g_log, "ccnud: updating interest timeout = %d", g_timeout_ms);
                    break;
                case MSG_IPC_RETRIES:
                    if (msg->payload_size != sizeof(uint32_t)) {
                        log_print(g_log, "ccnud: malformed MSG_IPC_TIMEOUT");
                    }
                    pthread_mutex_lock(&g_lock);
                    memcpy(&g_interest_attempts, msg->payload, sizeof(uint32_t));
                    pthread_mutex_unlock(&g_lock);
                    log_print(g_log, "ccnud: updating interest retries = %d", g_interest_attempts);
                    break;
                default:
                    log_print(g_log, "ccnud: unknown IPC message: %d", msg->type);
                    break;
            }

            free(msg->payload);
            free(msg);
        }
    }

    exit(EXIT_SUCCESS);
}
