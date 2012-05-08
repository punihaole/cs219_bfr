#include <sys/prctl.h>
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

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/time.h>
#include <sys/ioctl.h>

//#include <netinet/in.h>
#include <ifaddrs.h>
#include <net/if.h>

//#include <linux/if_packet.h>
//#include <linux/if_ether.h>
//#include <linux/if_arp.h>

#include "ccnf.h"
#include "ccnfd_listener.h"
#include "ccnfd_net_broadcaster.h"
#include "ccnfd_net_listener.h"
#include "ccnfd_cs.h"
#include "ccnfd_pit.h"
#include "ccnfd_stats.h"
#include "ccnfd.h"
#include "net_lib.h"
#include "log.h"

#include "synch_queue.h"

extern int ccnf_net_test();

/**
 * Only place where globals may be defined! (Declare in header)
 * Prefixed with g_
 **/
struct log * g_log;
uint32_t g_nodeId;
int g_timeout_ms;
int g_interest_attempts;
pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;

int g_sockfd;
struct sockaddr_ll g_eth_addr[MAX_INTERFACES];
char g_face_name[IFNAMSIZ][MAX_INTERFACES];
int g_faces;

static pthread_t idp_listener;
static pthread_t net_listener;
static pthread_mutex_t ccnfd_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  ccnfd_var   = PTHREAD_COND_INITIALIZER;

void print_usage(char * argv_0)
{
    printf("ccnfd v0.0.0.1a\n");
    printf("Usage: %s -h\n", argv_0);
    printf("      -h\tShow this help screen.\n");
    printf("      -t\tRun networking test (ccnfd must be running).\n");
    printf("      -l\tSpecify a log file.\n");
    printf("      -n\tSpecify a node Id.\n");
    printf("      -p\tSpecify the target false positive rate for the Bloom filter size.\n");
    printf("      -i\tSpecify the interest pipeline size.\n");
    printf("The ccnfd drops all output capability, so test results must be\n");
    printf("grepped from the log file. Running ccnfd without the -t flag \n");
    printf("starts the daemon.\n");
}

void signal_handler(int signal)
{
    switch(signal) {
        case SIGHUP:
            log_important(g_log, "Received SIGHUP signal.");
            break;
        case SIGTERM:
            log_important(g_log, "Received SIGTERM signal.");
            /* do cleanup */
            exit(EXIT_SUCCESS);
            break;
        default:
            log_warn(g_log, "Unhandled signal (%d) %s", signal, strsignal(signal));
            break;
    }
}

static int net_init()
{
    memset(g_face_name, 0, sizeof(g_face_name));

    struct ifaddrs * ifa, * p;

    if (getifaddrs(&ifa) != 0) {
        log_critical(g_log, "net_init: getifaddrs: %s", strerror(errno));
        return -1;
    }

    g_faces = 0;
    int family;
    for (p = ifa; p != NULL; p = p->ifa_next) {
        family = p->ifa_addr->sa_family;
        if ((p == NULL) || (p->ifa_addr == NULL)) continue;
        log_print(g_log, "net_init: found face: %s, address family: %d%s",
                  p->ifa_name, family,
                  (family == AF_PACKET) ? " (AF_PACKET)" :
                  (family == AF_INET) ?   " (AF_INET)" :
                  (family == AF_INET6) ?  " (AF_INET6)" : "");

        if (family != AF_PACKET) continue;

        if (strcmp(p->ifa_name, "lo") == 0) {
            log_debug(g_log, "net_init: skipping lo");
            continue;
        }

        /*
        int i;
        int found = 0;
        for (i = 0; i < g_faces; i++) {
            // weird bug where we discover the same faces multiple times
            if (strcmp(p->ifa_name, g_face_name[i]) == 0) {
                found = 1;
                break;
            }
        }

        if (found) continue;
        */

        //char * opt = p->ifa_name;
        //strcpy(g_face_name[g_faces], opt);

        /*
        g_sockfd[g_faces] = socket(AF_PACKET, SOCK_RAW, htons(CCNF_ETHER_PROTO));
        if (g_sockfd[g_faces] < 0) {
            log_print(g_log, "net_init: socket: %s", strerror(errno));
            return -1;
        }

        struct ifreq if_mac;
        // read the current ethernet card flags
        strncpy(if_mac.ifr_name, g_face_name[g_faces], IFNAMSIZ);
        if (ioctl(g_sockfd[g_faces], SIOCGIFFLAGS, &if_mac) < 0) {
            log_print(g_log, "net_init: ioctl: %s", strerror(errno));
            return -1;
        }
        // add the promiscuous flag and set it in the device
        if_mac.ifr_flags |= IFF_PROMISC;
        if (ioctl(g_sockfd[g_faces], SIOCSIFFLAGS, &if_mac) < 0) {
            log_print(g_log, "net_init: ioctl: %s", strerror(errno));
            return -1;
        }
        */

        g_sockfd = socket(AF_PACKET, SOCK_RAW, htons(CCNF_ETHER_PROTO));
        if (g_sockfd < 0) {
            log_critical(g_log, "net_init: socket: %s", strerror(errno));
            return -1;
        }
    }

    //log_print(g_log, "net_init: found %d faces", g_faces);

    freeifaddrs(ifa);

    return 0;
}

int main(int argc, char *argv[])
{
    if (getuid() != 0) {
        fprintf(stderr, "uid must be 0");
        exit(EXIT_FAILURE);
    }

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

    int logging_level = LOG_NORMAL;

    int c;
    while ((c = getopt(argc, argv, "-h?tl:n:p:i:qv")) != -1) {
        switch (c) {
            case 'h':
                print_usage(argv[0]);
                exit(EXIT_SUCCESS);
                break;
            case '?':
                print_usage(argv[0]);
                exit(EXIT_SUCCESS);
                break;
            case 't':
                printf("Running tests (will execute tests and terminate ");
                printf("without forking a daemon)...\n");
                if (ccnf_net_test() == 0) {
                    printf("Done. Check log for details.\n");
                    exit(EXIT_SUCCESS);
                } else {
                    fprintf(stderr, "Failed. Check log for details ");
                    fprintf(stderr, "-- make sure another ccnfd is running.\n");
                    exit(EXIT_FAILURE);
                }
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
                printf("set log file = %s.\n", stat_file);
                stat_file_set = 1;
                break;
            case 'i':
                interest_pipeline = atoi(optarg);
                fprintf(stderr, "set interest pipeline size to %d.\n", interest_pipeline);
                break;
            case 'q':
                logging_level = LOG_ERROR | LOG_CRITICAL;
                break;
            case 'v':
                logging_level = LOG_DEVEL;
                break;
            default:
                print_usage(argv[0]);
                exit(EXIT_SUCCESS);
                break;
        }
    }

    char * home_env = getenv("HOME");
    char home[256];
    if (!home_env) {
        fprintf(stderr, "ccnfd: could not parse HOME environment, exiting!");
        exit(EXIT_FAILURE);
    }
    strncpy(home, home_env, 256);

    g_log = (struct log * ) malloc(sizeof(struct log));
    char log_name[256];
    snprintf(log_name, 256, "ccnfd_%u", g_nodeId);
    if (!log_file_set)
        snprintf(log_file, 256, "%s/log/ccnfd_%u.log", home, g_nodeId);

    char proc[256];
    snprintf(proc, 256, "ccnfd%u", g_nodeId);
    prctl(PR_SET_NAME, proc, 0, 0, 0);

    if (log_init(log_name, log_file, g_log, LOG_OVERWRITE | logging_level) < 0) {
        fprintf(stderr, "ccnfd log: %s failed to initalize!\n", log_file);
        exit(EXIT_FAILURE);
    }

	if (!stat_file_set)
        snprintf(stat_file, 256, "%s/stat/ccnfd_%u.stat", home, g_nodeId);
	stat_file[255] = '\0';
    if (ccnfstat_init(stat_file) < 0) {
        fprintf(stderr, "ccnfd stat: %s failed to initalize!\n", stat_file);
        exit(EXIT_FAILURE);
    }

    fprintf(stderr, "Starting ccnfd...\n");

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
        fprintf(stderr, "setsid: %s.\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

	/* close out the std io file handles */
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    /* change the current working directory (after log is inited) */
    if ((chdir("/")) < 0) {
        log_critical(g_log, "chdir: %s.\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    /* initalize CS, PIT, FIB */
    if (CS_init(OLDEST, p) != 0) {
        log_critical(g_log, "failed to create CS! - EXITING");
        exit(EXIT_FAILURE);
    }

    if (PIT_init() != 0) {
        log_critical(g_log, "failed to create PIT! - EXITING");
        exit(EXIT_FAILURE);
    }

    if (ccnfdl_init(interest_pipeline) < 0) {
        log_critical(g_log, "ccnfd listener failed to initalize.");
        exit(EXIT_FAILURE);
    }

    if (net_init() < 0) {
        log_critical(g_log, "ccnfd net init failed.");
        exit(EXIT_FAILURE);
    }

    if (ccnfdnl_init(interest_pipeline) < 0) {
        log_critical(g_log, "ccnfd net listener failed to initalize.");
        exit(EXIT_FAILURE);
    }

    if (ccnfdnb_init() < 0) {
        log_critical(g_log, "ccnfd net broadcaster failed to initalize.");
        exit(EXIT_FAILURE);
    }

    /* initialize the ipc listener */
    struct listener_args ipc_args;
    ipc_args.lock = &ccnfd_mutex;
    ipc_args.cond = &ccnfd_var;
    ipc_args.queue = synch_queue_init();

    pthread_create(&idp_listener, NULL, ccnfdl_service, &ipc_args);

    /* initialize the net listener */
    pthread_create(&net_listener, NULL, ccnfdnl_service, NULL);

    /*
    int i = 0;
    for (i = 0; i < g_faces; i++) {
        struct listener_args * net_args = malloc(sizeof(struct listener_args));
        net_args->lock = NULL;
        net_args->cond = NULL;
        net_args->queue = NULL;
        net_args->opt = i;
        pthread_create(&net_listener, NULL, ccnfdnl_service, net_args);
    }
    */

    while (1) {
        /* for efficiency we sleep until someone wakes us up */
        pthread_mutex_lock(&ccnfd_mutex);
            pthread_cond_wait(&ccnfd_var, &ccnfd_mutex);
        pthread_mutex_unlock(&ccnfd_mutex);

        while (synch_len(ipc_args.queue)) {
            struct ccnfd_msg * msg = synch_dequeue(ipc_args.queue);
            switch (msg->type) {
                case MSG_IPC_TIMEOUT:
                    if (msg->payload_size != sizeof(uint32_t)) {
                        log_error(g_log, "ccnfd: malformed MSG_IPC_TIMEOUT");
                    }
                    pthread_mutex_lock(&g_lock);
                    memcpy(&g_timeout_ms, msg->payload, sizeof(uint32_t));
                    pthread_mutex_unlock(&g_lock);
                    log_important(g_log, "ccnfd: updating interest timeout = %d", g_timeout_ms);
                    break;
                case MSG_IPC_RETRIES:
                    if (msg->payload_size != sizeof(uint32_t)) {
                        log_error(g_log, "ccnfd: malformed MSG_IPC_TIMEOUT");
                    }
                    pthread_mutex_lock(&g_lock);
                    memcpy(&g_interest_attempts, msg->payload, sizeof(uint32_t));
                    pthread_mutex_unlock(&g_lock);
                    log_important(g_log, "ccnfd: updating interest retries = %d", g_interest_attempts);
                    break;
                default:
                    log_error(g_log, "ccnud: unknown IPC message: %d", msg->type);
                    break;
            }

            free(msg->payload);
            free(msg);
        }
    }

    exit(EXIT_SUCCESS);
}
