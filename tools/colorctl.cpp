#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <chrono>
#include <math.h>
#include <thread>
#include <fstream>
#include <sys/syscall.h>
#include <string.h>
#include <sched.h>

#define __NR_set_color 441
#define __NR_get_color 442

#define NR_COLORS 768


int set_color(int len, void *buffer, int pid) {
	return syscall(__NR_set_color, pid, len, buffer);
}

int main(int argc, char *argv[]) {
	if (argc < 3) {
		printf("Usage: %s <color list (e.g., 0-7,8-15)> <pid> ...\n", argv[0]);
		exit(1);
	}
	int pid = atoi(argv[3]);
	int buffer_len = (NR_COLORS + sizeof(long) * 8 - 1) / (sizeof(long) * 8);
	long *buffer = (long *)malloc(sizeof(long) * buffer_len);
	if (!buffer) {
		printf("Failed to allocate buffer\n");
		exit(1);
	}
	memset(buffer, 0, sizeof(long) * buffer_len);

	char *rest_list;
	char *interval = strtok_r(argv[1], ",", &rest_list);
	while (interval != NULL) {
		int start, end;
		if (strstr(interval, "-") != NULL) {
			char *rest_token;
			char *token = strtok_r(interval, "-", &rest_token);
			start = atoi(token);
			token = strtok_r(NULL, "-", &rest_token);
			end = atoi(token);
		} else {
			start = atoi(interval);
			end = start;
		}
		for (int i = start; i <= end; ++i) {
			int buffer_index = i / (sizeof(long) * 8);
			int bit_index = i % (sizeof(long) * 8);
			buffer[buffer_index] = buffer[buffer_index] | (1l << bit_index);
		}
		interval = strtok_r(NULL, ",", &rest_list);
	}

	int syscall_ret = set_color(sizeof(long) * buffer_len, buffer, pid);
	if (syscall_ret) {
		printf("Syscall error - ret: %d\n", syscall_ret);
		exit(1);
	}

	if (argc > 3)
		execvp(argv[3], argv + 3);
}
