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

#define __NR_reserve_color 443

using namespace std;


int reserve_color(long nr_page) {
	return syscall(__NR_reserve_color, nr_page);
}

int main(int argc, char *argv[]) {
	if (argc != 2) {
		printf("Usage: %s <number of pages per color>\n", argv[0]);
		exit(1);
	}
	long nr_page = atol(argv[1]);
	reserve_color(nr_page);
	return 0;
}
