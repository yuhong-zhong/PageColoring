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

int get_color(int len, void *buffer, int pid) {
	return syscall(__NR_get_color, pid, len, buffer);
}

int main(int argc, char *argv[]) {
	if (argc != 2) {
		printf("Usage: %s <pid>\n", argv[0]);
		exit(1);
	}

        int pid = atoi(argv[1]);
	int buffer_len = (NR_COLORS + sizeof(long) * 8 - 1) / (sizeof(long) * 8);
	long *buffer = (long *) malloc(sizeof(long) * buffer_len);
	if (!buffer) {
		printf("Failed to allocate buffer\n");
		exit(1);
	}
	memset(buffer, 0, sizeof(long) * buffer_len);

	int syscall_ret = get_color(sizeof(long) * buffer_len, buffer, pid);
	if (syscall_ret < 0) {
		printf("Syscall error - ret: %d\n", syscall_ret);
		exit(1);
	}

	printf("Enabled color: ");
	for (int i = 0; i < NR_COLORS; ++i) {
		int buffer_index = i / (sizeof(long) * 8);
		int bit_index = i % (sizeof(long) * 8);
		if (buffer[buffer_index] & (1l << bit_index)) {
			printf("%d ", i);
		}
	}
	printf("\n");
}
