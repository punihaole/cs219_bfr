#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include "bfr.h"
#include "bfrd.h"
#include "bfr_listener.h"
#include "bfr_net_listener.h"

#include "bfr_test.h"

#include "log.h"
#include "net_lib.h"

#include "hash.h"
#include "strategy.h"
#include "synch_queue.h"

pthread_t ipc_listener;
pthread_t net_listener;
pthread_t strategy_thread;

static void print_usage(char * argv_0)
{
	printf("bfrd v0.0.0.1a\n");
    printf("Usage: %s -h\n", argv_0);
    printf("      -h\tShow this help screen.\n");
    printf("      -t\tRun test suite.\n");
    printf("      -n\tSet the node Id to use.\n");
    printf("      -l\tSet the number of hierarchy levels.\n");
    printf("      -g\tSet the grid dimensions, format:\n");
    printf("        \t\t%%dx%%d (ex. -g 100x100)\n");
    printf("      -s\tSet the starting position of the node, format:\n");
    printf("        \t\t%%d,%%d (ex. -s 10,10)\n");
    printf("\n");
}

static void signal_handler(int signal)
{
    switch(signal) {
        case SIGHUP:
            log_print(g_log, "Received SIGHUP signal.");
            break;
        case SIGTERM:
            log_print(g_log, "Received SIGTERM signal.");
            bfr_listener_close();
            bfr_net_listener_close();
            strategy_close();
            log_close(g_log);

            exit(EXIT_SUCCESS);
            break;
        case SIGUSR1:
            log_print(g_log, "Received SIGUSR1 signal.");
            break;
        case SIGUSR2:
            log_print(g_log, "Received SIGUSR2 signal.");
            break;
        default:
            log_print(g_log, "Unhandled signal (%d) %s", signal, strsignal(signal));
            break;
    }
}

static int read_config(int * nodeId, int * levels,
                unsigned int * width, unsigned int * height,
                unsigned int * startX, unsigned int * startY);

int main(int argc, char ** argv)
{
    char log_file[256];
    int log_file_set = 0;

    int nodeId = DEFAULT_NODE_ID, levels = DEFAULT_NUM_LEVELS;
    unsigned int width = DEFAULT_GRID_WIDTH, height = DEFAULT_GRID_HEIGHT,
    startX = DEFAULT_GRID_STARTX, startY = DEFAULT_GRID_STARTY;

    read_config(&nodeId, &levels, &width, &height, &startX, &startY);

    pid_t pid, sid;

    signal(SIGHUP, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);
    signal(SIGQUIT, signal_handler);
    signal(SIGUSR1, signal_handler);
    signal(SIGUSR2, signal_handler);

    int c;
    while ((c = getopt(argc, argv, "-h?tn:l:g:s:")) != -1) {
        switch (c) {
            case 'h':
            case '?':
                print_usage(argv[0]);
                exit(EXIT_SUCCESS);
            case 'n':
                nodeId = atoi(optarg);
                fprintf(stderr, "set node ID = %u.\n", nodeId);
                break;
            case 'l':
                levels = atoi(optarg);
                fprintf(stderr, "set levels = %d.\n", levels);
                break;
            case 'f':
                strncpy(optarg, log_file, 256);
                break;
            case 'g':
                sscanf(optarg, "%dx%d", &height, &width);
                fprintf(stderr, "set grid dim: %d X %d.\n", height, width);
                break;
            case 's':
                sscanf(optarg, "%d,%d", &startX, &startY);
                fprintf(stderr, "set start pos = (%d, %d).\n", startX, startY);
                break;
            case 't':
                if (test_suite())
                    exit(EXIT_SUCCESS);
                else
                    exit(EXIT_FAILURE);
            default:
                break;

        }
    }

    fprintf(stderr, "Starting daemon...");

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

    /* close out the std io file handles */
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    g_log = (struct log * ) malloc(sizeof(struct log));
    char log_name[256];
    snprintf(log_name, 256, "bfr_%u", nodeId);
    if (!log_file_set)
        snprintf(log_file, 256, "/tmp/bfr_%u.log", nodeId);
    log_name[255] = '\0';
    log_file[255] = '\0';

    if (log_init(log_name, log_file, g_log, LOG_OVERWRITE) < 0) {
        syslog(LOG_ERR, "bfr log: %s failed to initalize, exiting!", log_file);
        exit(EXIT_FAILURE);
    }

    /* change the current working directory (after log is inited) */
    if ((chdir("/")) < 0) {
        log_print(g_log, "chdir: %s.", strerror(errno));
        exit(EXIT_FAILURE);
    }

    /* stuff needs to be initialized in this order */
    if (bfr_init(nodeId, levels, width, height, startX, startY) < 0) {
        log_print(g_log, "bfr_init: error initializing.");
        exit(EXIT_FAILURE);
    }

    if (stategy_init() < 0) {
        log_print(g_log, "stategy_init: error initializing.");
        exit(EXIT_FAILURE);
    }

    if (bfr_listener_init() < 0) {
        log_print(g_log, "bfr_listener_init: error initializing.");
        exit(EXIT_FAILURE);
    }

    if (bfr_net_listener_init() < 0) {
        log_print(g_log, "bfr_net_listener_init: error initializing.");
        exit(EXIT_FAILURE);
    }

    log_print(g_log, "Initialized successfully, pid = %d.", getpid());

    pthread_mutex_t bfrd_mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t  bfrd_var   = PTHREAD_COND_INITIALIZER;

    /* initialize the ipc listener */
    struct listener_args ipc_args;
    ipc_args.lock = &bfrd_mutex;
    ipc_args.cond = &bfrd_var;
    ipc_args.queue = synch_queue_init();
    pthread_create(&ipc_listener, NULL, bfr_listener_service, &ipc_args);

    /* initialize the net listener */
    struct listener_args net_args;
    net_args.lock = &bfrd_mutex;
    net_args.cond = &bfrd_var;
    net_args.queue = synch_queue_init();
    pthread_create(&net_listener, NULL, bfr_net_listener_service, &net_args);

    /* initialize the strategy serivce */
    pthread_create(&strategy_thread, NULL, strategy_service, NULL);

    while (1) {
        /* for efficiency we sleep until someone wakes us up */
        pthread_mutex_lock(&bfrd_mutex);
            pthread_cond_wait(&bfrd_var, &bfrd_mutex);
        pthread_mutex_unlock(&bfrd_mutex);

        /* we don't know who woke us up, it could be the ipc listener or net
         * listener
         */
        bfr_handle_net(&net_args);
        bfr_handle_ipc(&ipc_args);
    }

    exit(EXIT_SUCCESS);
}

static int read_config(int * nodeId, int * levels,
                unsigned int * width, unsigned int * height,
                unsigned int * startX, unsigned int * startY)
{
    /** TODO **/
    return -1;
}
