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

#include "ccnumr.h"
#include "ccnumrd.h"
#include "ccnumr_listener.h"
#include "ccnumr_net_listener.h"

#include "ccnumr_test.h"

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
	printf("ccnumrd v0.0.0.1a\n");
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
            ccnumr_listener_close();
            ccnumr_net_listener_close();
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
    snprintf(log_name, 256, "ccnumr_%u", nodeId);
    if (!log_file_set)
        snprintf(log_file, 256, "/tmp/ccnumr_%u.log", nodeId);
    log_name[255] = '\0';
    log_file[255] = '\0';

    if (log_init(log_name, log_file, g_log, LOG_OVERWRITE) < 0) {
        syslog(LOG_ERR, "ccnumr log: %s failed to initalize, exiting!", log_file);
        exit(EXIT_FAILURE);
    }

    /* change the current working directory (after log is inited) */
    if ((chdir("/")) < 0) {
        log_print(g_log, "chdir: %s.", strerror(errno));
        exit(EXIT_FAILURE);
    }

    /* stuff needs to be initialized in this order */
    if (ccnumr_init(nodeId, levels, width, height, startX, startY) < 0) {
        log_print(g_log, "ccnumr_init: error initializing.");
        exit(EXIT_FAILURE);
    }

    if (stategy_init() < 0) {
        log_print(g_log, "stategy_init: error initializing.");
        exit(EXIT_FAILURE);
    }

    if (ccnumr_listener_init() < 0) {
        log_print(g_log, "ccnumr_listener_init: error initializing.");
        exit(EXIT_FAILURE);
    }

    if (ccnumr_net_listener_init() < 0) {
        log_print(g_log, "ccnumr_net_listener_init: error initializing.");
        exit(EXIT_FAILURE);
    }

    log_print(g_log, "Initialized successfully, pid = %d.", getpid());

    pthread_mutex_t ccnumrd_mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t  ccnumrd_var   = PTHREAD_COND_INITIALIZER;

    /* initialize the ipc listener */
    struct listener_args ipc_args;
    ipc_args.lock = &ccnumrd_mutex;
    ipc_args.cond = &ccnumrd_var;
    ipc_args.queue = synch_queue_init();
    pthread_create(&ipc_listener, NULL, ccnumr_listener_service, &ipc_args);

    /* initialize the net listener */
    struct listener_args net_args;
    net_args.lock = &ccnumrd_mutex;
    net_args.cond = &ccnumrd_var;
    net_args.queue = synch_queue_init();
    pthread_create(&net_listener, NULL, ccnumr_net_listener_service, &net_args);

    /* initialize the strategy serivce */
    pthread_create(&strategy_thread, NULL, strategy_service, NULL);

    while (1) {
        /* for efficiency we sleep until someone wakes us up */
        pthread_mutex_lock(&ccnumrd_mutex);
            pthread_cond_wait(&ccnumrd_var, &ccnumrd_mutex);
        pthread_mutex_unlock(&ccnumrd_mutex);

        /* we don't know who woke us up, it could be the ipc listener or net
         * listener
         */

        ccnumr_handle_net(&net_args);
        ccnumr_handle_ipc(&ipc_args);
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
