#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sched.h>
#include <signal.h>
#include <math.h>
#include <chrono>
#include <thread>
#include <fstream>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/ioctl.h>
#include <linux/page_coloring.h>

#define __NR_set_color 441
#define __NR_get_color 442
#define __NR_reserve_color 443

#define NR_COLORS 768
#define PPOOL_PATH "/proc/color_ppool"

#define BUG_ON(cond)											\
	do {												\
		if (cond) {										\
			fprintf(stdout, "BUG_ON: %s (L%d) %s\n", __FILE__, __LINE__, __FUNCTION__);	\
			raise(SIGABRT);									\
		}											\
	} while (0)


int set_color_syscall(int len, void *buffer, int pid) {
	return syscall(__NR_set_color, pid, len, buffer);
}

int reserve_color_syscall(long nr_page) {
	return syscall(__NR_reserve_color, nr_page);
}

void print_help_message() {
	printf("Usage:\n"
	       "  colorctl set_color <color list (e.g., 0-7,8-15)> <pid> <command (optional)>\n"
	       "  colorctl fill_color <number of pages per color>\n"
	       "  colorctl set_ppool <pid> <command (optional)>\n"
	       "  colorctl unset_ppool <pid>\n"
	       "  colorctl fill_ppool <target number of pages> <nid> <color list (e.g., 0-7,8-15)>\n");
}

void convert_list_to_bitmap(char *list, uint64_t *bitmap) {
	char *rest_list;
	char *interval = strtok_r(list, ",", &rest_list);
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
			int buffer_index = i / (sizeof(uint64_t) * 8);
			int bit_index = i % (sizeof(uint64_t) * 8);
			bitmap[buffer_index] = bitmap[buffer_index] | (1l << bit_index);
		}
		interval = strtok_r(NULL, ",", &rest_list);
	}
}

void set_color(int argc, char *argv[]) {
	if (argc < 2) {
		print_help_message();
		exit(1);
	}
	int pid = atoi(argv[1]);
	int buffer_len = (NR_COLORS + sizeof(uint64_t) * 8 - 1) / (sizeof(uint64_t) * 8);
	uint64_t *buffer = (uint64_t *) malloc(sizeof(*buffer) * buffer_len);
	BUG_ON(buffer == NULL);
	memset(buffer, 0, sizeof(*buffer) * buffer_len);
	convert_list_to_bitmap(argv[0], buffer);
	int ret = set_color_syscall(sizeof(*buffer) * buffer_len, buffer, pid);
	BUG_ON(ret != 0);
	free(buffer);

	if (argc > 2)
		execvp(argv[2], argv + 2);
}

void fill_color(int argc, char *argv[]) {
	if (argc != 1) {
		print_help_message();
		exit(1);
	}
	long num_pages_per_color = atol(argv[0]);
	int ret = reserve_color_syscall(num_pages_per_color);
	BUG_ON(ret != 0);
}

void set_ppool(int argc, char *argv[]) {
	if (argc < 1) {
		print_help_message();
		exit(1);
	}
	int pid = atoi(argv[0]);
	BUG_ON(pid < 0);
	int fd = open(PPOOL_PATH, O_RDONLY);
	BUG_ON(fd < 0);
	int ret = ioctl(fd, PPOOL_IOC_ENABLE, &pid);
	BUG_ON(ret != 0);
	ret = close(fd);
	BUG_ON(ret != 0);

	if (argc > 1)
		execvp(argv[1], argv + 1);
}

void unset_ppool(int argc, char *argv[]) {
	if (argc != 1) {
		print_help_message();
		exit(1);
	}
	int pid = atoi(argv[0]);
	BUG_ON(pid < 0);
	int fd = open(PPOOL_PATH, O_RDONLY);
	BUG_ON(fd < 0);
	int ret = ioctl(fd, PPOOL_IOC_DISABLE, &pid);
	BUG_ON(ret != 0);
	ret = close(fd);
	BUG_ON(ret != 0);
}

void fill_ppool(int argc, char *argv[]) {
	if (argc != 3) {
		print_help_message();
		exit(1);
	}
	uint64_t target_num_pages = atol(argv[0]);
	int nid = atoi(argv[1]);
	BUG_ON(nid < 0);
	int buffer_len = (NR_COLORS + sizeof(uint64_t) * 8 - 1) / (sizeof(uint64_t) * 8);
	uint64_t *buffer = (uint64_t *) malloc(sizeof(*buffer) * buffer_len);
	BUG_ON(buffer == NULL);
	memset(buffer, 0, sizeof(*buffer) * buffer_len);
	convert_list_to_bitmap(argv[2], buffer);

	struct ppool_fill_req req;
	req.target_num_pages = target_num_pages;
	req.nid = nid;
	req.user_mask_ptr = buffer;

	int fd = open(PPOOL_PATH, O_RDONLY);
	BUG_ON(fd < 0);
	int ret = ioctl(fd, PPOOL_IOC_FILL, &req);
	BUG_ON(ret != 0);
	ret = close(fd);
	BUG_ON(ret != 0);
	free(buffer);
}

int main(int argc, char *argv[]) {
	if (argc == 1) {
		print_help_message();
		exit(1);
	}
	char *op_type_str = argv[1];
	if (strcmp(op_type_str, "set_color") == 0) {
		set_color(argc - 2, argv + 2);
	} else if (strcmp(op_type_str, "fill_color") == 0) {
		fill_color(argc - 2, argv + 2);
	} else if (strcmp(op_type_str, "set_ppool") == 0) {
		set_ppool(argc - 2, argv + 2);
	} else if (strcmp(op_type_str, "unset_ppool") == 0) {
		unset_ppool(argc - 2, argv + 2);
	} else if (strcmp(op_type_str, "fill_ppool") == 0) {
		fill_ppool(argc - 2, argv + 2);
	} else {
		// Default: set_color
		set_color(argc - 1, argv + 1);
	}
	return 0;
}
