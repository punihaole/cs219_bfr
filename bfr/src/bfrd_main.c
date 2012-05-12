#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/prctl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "bfr.h"
#include "bfr_stats.h"
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
    printf("      -c\tSet the starting position of the node, format:\n");
    printf("        \t\t%%d,%%d (ex. -s 10,10)\n");
    printf("      -s\tSet the stat file.\n");
    printf("      -b\tSet the bloom sharing interval in ms.\n");
    printf("        \t\t%%d (ex. -b 20000)");
    printf("      -j\tSet the cluster join interval in ms.\n");
    printf("        \t\t%%d (ex. -j 10000)");
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
    char stat_file[256];
    int stat_file_set = 0;

    int nodeId = DEFAULT_NODE_ID, levels = DEFAULT_NUM_LEVELS;
    unsigned int width = DEFAULT_GRID_WIDTH, height = DEFAULT_GRID_HEIGHT,
    startX = DEFAULT_GRID_STARTX, startY = DEFAULT_GRID_STARTY;
    int bloom_interval_ms = DEFAULT_BLOOM_INTERVAL_SEC * 1000;
    int cluster_interval_ms = DEFAULT_CLUSTER_INTERVAL_SEC * 1000;

    read_config(&nodeId, &levels, &width, &height, &startX, &startY);

    pid_t pid, sid;

    signal(SIGHUP, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);
    signal(SIGQUIT, signal_handler);
    signal(SIGUSR1, signal_handler);
    signal(SIGUSR2, signal_handler);

    int logging_level = LOG_NORMAL;

    int c;
    while ((c = getopt(argc, argv, "-h?tn:l:g:s:c:b:j:qv")) != -1) {
        switch (c) {
            case 'h':
            case '?':
                print_usage(argv[0]);
                exit(EXIT_SUCCESS);
            case 'b':
            	bloom_interval_ms = atoi(optarg);
            	if (bloom_interval_ms <= 0) {
        		    bloom_interval_ms = DEFAULT_BLOOM_INTERVAL_SEC * 1000;
            		printf("error parsing bloom sharing interval! defaulting to %d.\n", bloom_interval_ms);
            	} else {
	            	printf("set bloom sharing interval to %d ms.\n", bloom_interval_ms);
	            }
            	break;
            case 'c':
                sscanf(optarg, "%d,%d", &startX, &startY);
                printf("set start pos = (%d, %d).\n", startX, startY);
                break;
            case 'f':
                strncpy(optarg, log_file, 256);
                printf("set log file = %s.\n", log_file);
                log_file_set = 1;
                break;
            case 'g':
                sscanf(optarg, "%dx%d", &height, &width);
                printf("set grid dim: %d X %d.\n", height, width);
                break;
            case 'j':
            	cluster_interval_ms = atoi(optarg);
            	if (cluster_interval_ms <= 0) {
        		    cluster_interval_ms = DEFAULT_CLUSTER_INTERVAL_SEC * 1000;
            		printf("error parsing cluster join interval default to %d.\n", cluster_interval_ms);
            	} else {
	            	printf("set cluster join interval to %d ms.\n", cluster_interval_ms);
	            }
            	break;
           	case 'l':
                levels = atoi(optarg);
                printf("set levels = %d.\n", levels);
                break;
            case 'n':
                nodeId = atoi(optarg);
                printf("set node ID = %u.\n", nodeId);
                break;
            case 's':
                strncpy(optarg, stat_file, 256);
                printf("set stat file = %s.\n", stat_file);
                stat_file_set = 1;
                break;
            case 't':
                if (test_suite())
                    exit(EXIT_SUCCESS);
                else
                    exit(EXIT_FAILURE);
            case 'q':
                logging_level = LOG_ERROR | LOG_CRITICAL;
                break;
            case 'v':
                logging_level = LOG_DEVEL;
                break;
            default:
                break;

        }
    }

    g_bfr.nodeId = nodeId;

    char proc[256];
    snprintf(proc, 256, "bfrd%u", g_bfr.nodeId);
    prctl(PR_SET_NAME, proc, 0, 0, 0);

    char * home_env = getenv("HOME");
    char home[256];
    if (!home_env) {
        fprintf(stderr, "bfrd: could not parse HOME environment, exiting!");
        exit(EXIT_FAILURE);
    }
    strncpy(home, home_env, 256);

    g_log = (struct log * ) malloc(sizeof(struct log));
    char log_name[256];
    snprintf(log_name, 256, "bfr_%u", nodeId);
    if (!log_file_set)
        snprintf(log_file, 256, "%s/log/bfr_%u.log", home, nodeId);
    log_name[255] = '\0';
    log_file[255] = '\0';

    if (log_init(log_name, log_file, g_log, LOG_OVERWRITE | logging_level) < 0) {
        fprintf(stderr, "bfr log: %s failed to initalize, exiting!", log_file);
        exit(EXIT_FAILURE);
    }

    if (!stat_file_set)
        snprintf(stat_file, 256, "%s/stat/bfr_%u.stat", home, nodeId);
    stat_file[255] = '\0';
    if (bfrstat_init(stat_file) < 0) {
        fprintf(stderr, "bfrd stat: %s failed to initalize!", stat_file);
        exit(EXIT_FAILURE);
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

    if (strategy_init(levels, bloom_interval_ms, cluster_interval_ms) < 0) {
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
