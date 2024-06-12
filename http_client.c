#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <time.h>
#include <stdatomic.h>

#include <pthread.h>
#include <curl/curl.h>

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_t *pids;

uint32_t thread_num;
atomic_int send_times;

unsigned char rid[7];

#define URL "http://8.8.8.8"

static char num_to_char(int num)
{
	if (num < 10)
		return '0' + num;
	else if (num < 36)
		return 'a' + num - 10;
	else
		return 'A' + num - 36;
}

static int char_to_num(char ch)
{
	if (ch >= '0' && ch <= '9')
		return ch - '0';
	else if (ch >= 'a' && ch <= 'z')
		return ch - 'a' + 10;
	else
		return ch - 'A' + 36;
}

static void init_rid(char *buf)
{
	for(int i=0; i<7; i++) {
		rid[6 - i] = char_to_num(buf[i]);
	}
}

static void gen_rid(char *buf)
{
	int carry = 0;

	rid[0]++;

	for(int i=0; i<7; i++) {
		int res = carry + rid[i];
		carry = res >= 62 ? 1 : 0;
		res %= 62;
		rid[i] = res;
	}

	for(int i=0; i<7; i++) {
		buf[i] = num_to_char(rid[6 - i]);
	}
}

static size_t cb(char *ptr, size_t size, size_t nmemb, void *userdata)
{
	return size * nmemb;
}

static void *task(void *arg)
{
	char url[64];

	while (1) {
		CURL *curl = curl_easy_init();
		if (!curl)
			continue;

		char url[64] = {};
		char buf[8] = {};

		pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

		pthread_mutex_lock(&mutex);
		gen_rid(buf);
		pthread_mutex_unlock(&mutex);

		pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);

		snprintf(url, sizeof(url), "%s?rid=%s", URL, buf);
		curl_easy_setopt(curl, CURLOPT_URL, url);

		curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 1000L);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, cb);

		CURLcode res = curl_easy_perform(curl);
		if (res == CURLE_OK)
			atomic_fetch_add(&send_times, 1);

		curl_easy_cleanup(curl);
	}
END:
	pthread_exit(NULL);
}

int main(int argc, char **argv)
{
	int ret;
	struct timespec start, end;
	
	/* Parse argument */
	if (argc < 2 || argc > 3) {
		printf("Invalid parameters\n");
		return -1;
	}

	thread_num = strtoul(argv[1], NULL, 0);

	if (argc == 3)
		init_rid(argv[2]);

	/* Initial time */
	clock_gettime(CLOCK_MONOTONIC, &start);

	/* Create threads */
	pids = calloc(thread_num, sizeof(pthread_t));
	for(int i=0; i<thread_num; i++) {
		ret = pthread_create(&pids[i], NULL, task, NULL);
		if (ret)
			return -1;
	}

	/* Pause */
	getc(stdin);

	for(int i=0; i<thread_num; i++) {
		pthread_cancel(pids[i]);
	}

	for(int i=0; i<thread_num; i++) {
		pthread_join(pids[i], NULL);
	}

	/* Compute execution time */
	clock_gettime(CLOCK_MONOTONIC, &end);

	uint64_t diff = 1000 * (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1000000;

	/* Print result */
	printf("Total times: %u, elapse ms: %lu\n", send_times, diff);
	printf("Rate: %lf r/ms\n", (double)send_times / diff);

	/* Print last rid */
	char rid_buf[8] = {};
	gen_rid(rid_buf);
	printf("Last rid: %s\n", rid_buf);

	return 0;
}
