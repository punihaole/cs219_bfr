#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <signal.h>

#include "ccnf.h"
#include "ccnu.h"
#include "content_name.h"
#include "content.h"
#include "ts.h"

long jitter_ms;

typedef int (*publish_t)(struct content_obj *);
typedef int (*retrieve_t)(struct content_name * , struct content_obj ** );

void signal_handler(int signal)
{
	switch(signal) {
		case SIGHUP:
			exit(EXIT_SUCCESS);
		case SIGINT:
		case SIGTERM:
			printf("\naverage session jitter = %5.4fms\n", jitter_ms);
			exit(EXIT_SUCCESS);
		default:
			break;
	}
}

void print_usage(char * argv_0)
{
	printf("cstp\n");
	printf("CCNU Streaming file transfer application.\n");
    printf("Usage: %s -f[opt] -n content_name {-s source_file}[opt] {-d dest_file}[opt]\n", argv_0);
	printf("\tthe optional (-f) argument specifies to use the flooding ccn daemon\n");
	printf("\tyou must specify either a -s or -d option. -s indicates to serve the data\n");
	printf("\tunder the given file/device. -d means to write to it. In either case content\n");
	printf("\tis published/retrieved by the given content name.\n");
    printf("Example: %s -n /voice/orders -s /dev/microphone\n", argv_0);
	printf("\t serves data under the content name /voice/orders from a fictional device, microphone.\n");
	printf("Example: %s -n /voice/orders -d /dev/speakers\n", argv_0);
	printf("\t retrieves data under the content name /voice/orders and writes it to a fictional device,\n");
	printf("\t speakers.\n");
}

void mssleep(int ms)
{
	if (ms >= 1000) {
		sleep(ms / 1000);
		ms = ms % 1000;
	}
	usleep(ms * 1000);
}

int server(struct content_name * base_name, int interval_ms, FILE * source, int flood)
{
	publish_t publish_fun;
	if (flood) publish_fun = ccnf_publish;
	else publish_fun = ccnu_publish;

	int max_segment_size = ccnu_max_payload_size(base_name);
	struct timespec ts;
	ts_fromnow(&ts);
	struct content_obj publish;
	publish.name = content_name_create(base_name->full_name);
	publish.timestamp = ts.tv_sec;
	publish.size = sizeof(uint32_t);
	publish.data = malloc(publish.size);
	uint32_t dummydata = interval_ms;
	memcpy(publish.data, &dummydata, sizeof(uint32_t));
	if (publish_fun(&publish) < 0) {
		fprintf(stderr, "call to publish(%s) failed!\n", base_name->full_name);
		exit(EXIT_FAILURE);
	}
	
	char buffer[2048];
	int n;
	int chunk_id = 0;
	char str[MAX_NAME_LENGTH+1];
	str[MAX_NAME_LENGTH] = '\0';
	while (chunk_id < 100000) {
		mssleep(interval_ms);
		ts_fromnow(&ts);
		snprintf(str, MAX_NAME_LENGTH, "%s/%d", base_name->full_name, chunk_id);
		content_name_delete(publish.name);
		publish.name = content_name_create(str);
		max_segment_size = ccnu_max_payload_size(publish.name);
		publish.timestamp = ts.tv_sec;
		n = fread(buffer, 1, max_segment_size, source);
		publish.size = n;
		publish.data = realloc(publish.data, publish.size);
		memcpy(publish.data, buffer, n);
		printf("publishing %s\n", str);
		if (publish_fun(&publish) < 0) {
			fprintf(stderr, "call to publish(%s) failed!\n", publish.name->full_name);
			exit(EXIT_FAILURE);
		}
		chunk_id++;
	}
	
	return 0;
}

int client(struct content_name * base_name, int interval_ms, FILE * dest, int flood)
{
	signal(SIGHUP, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);
    signal(SIGQUIT, signal_handler);

	int lost = 0, lost_consec = 0;
	retrieve_t retrieve_fun;
	if (flood) retrieve_fun = ccnf_retrieve;
	else retrieve_fun = ccnu_retrieve;

	struct content_obj * obj;
	if (retrieve_fun(base_name, &obj) != 0) {
		fprintf(stderr, "Could not retrieve %s.\n", base_name->full_name);
		exit(EXIT_FAILURE);
	}

	struct content_name * next_name;
	int chunk_id = 0;
	char str[MAX_NAME_LENGTH+1];
	str[MAX_NAME_LENGTH] = '\0';
	int tries = 2;
	int i;
	int rv;
	struct timespec ts_start, ts_end;
	while ((chunk_id < 100000) && (lost_consec <= 10)) {
		snprintf(str, MAX_NAME_LENGTH, "%s/%d", base_name->full_name, chunk_id);
		next_name = content_name_create(str);
		
		printf("retrieving %s\n", str);
		ts_fromnow(&ts_start);
		for (i = 0; i < tries; i++) {
			if ((rv = retrieve_fun(next_name, &obj)) != 0) {
				mssleep(2 * interval_ms);
			}
		} 
		if (rv != 0) {
			printf("lost %s...\n", str);
			lost++;
			lost_consec++;
		} else {
			ts_fromnow(&ts_end);
			long delay_ms = ts_mselapsed(&ts_start, &ts_end);
			jitter_ms = ((chunk_id+1) * jitter_ms + delay_ms) / ((chunk_id+1) + 1);
			fwrite(obj->data, 1, obj->size, dest);
			content_obj_destroy(obj);
			lost_consec = 0;
		}

		if ((chunk_id % 10) == 0) {
			printf("detected jitter = %dms\n", (int)(jitter_ms));
		}

		content_name_delete(next_name);
		chunk_id++;
	}

	if (lost_consec > 10)
		fprintf(stderr, "lost %d consecutive chunks, closing application...\n", lost_consec);

	printf("\naverage session jitter = %5.4fms\n", jitter_ms);
	return 0;
}

int main(int argc, char ** argv)
{
	char name[MAX_NAME_LENGTH+1];
	name[MAX_NAME_LENGTH] = '\0';
	char source[257];
	source[256] = '\0';
	char dest[257];
	dest[256] = '\0';
	int flood = 0;
	int gotName = 0;
	int gotSource = 0;
	int gotDest = 0;
	int interval = 1000;
	
	int c;
	while ((c = getopt(argc, argv, "-h?fn:s:d:")) != -1) {
		switch (c) {
		case 'f':
			flood = 1;
			break;
		case 'n':
			strncpy(name, optarg, MAX_NAME_LENGTH);
			gotName = 1;
			break;
		case 's':
			strncpy(source, optarg, 256);
			gotSource = 1;
			break;
		case 'd':
			strncpy(dest, optarg, 256);
			gotDest = 1;
		case 'i':
			interval = atoi(optarg);
			break;
		case 'h':
		case '?':
		default:
			print_usage(argv[0]);
			exit(EXIT_SUCCESS);
		}
	}

	if (!gotName) {
		fprintf(stderr, "Need to provide a prefix!\n");
		exit(EXIT_FAILURE);
	}

	if (flood) {
		printf("using flooding daemon.\n");
	}

	struct content_name * con_name = content_name_create(name);	

	if (gotSource) {
		FILE *fp = fopen(source, "rb");
		if (fp == NULL) {
			fprintf(stderr, "Could not open specified file: %s.\n", source);
			exit(EXIT_FAILURE);
		}
		printf("Serving streaming data from: %s under prefix: %s.\n", source, name);
		server(con_name, interval, fp, flood);
	} else if (gotDest) {
		FILE *fp = fopen(dest, "wb");
		if (fp == NULL) {
			fprintf(stderr, "Could not open specified file: %s.\n", source);
			exit(EXIT_FAILURE);
		}
		printf("Retrieivng streaming data under prefix: %s to file: %s.\n", name, dest);
		client(con_name, interval, fp, flood);
	} else {
		fprintf(stderr, "Could not determine whether to serve or retrieve data!\n");
		exit(EXIT_FAILURE);
	}

	exit(EXIT_SUCCESS);
}

