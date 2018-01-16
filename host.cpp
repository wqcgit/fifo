/// g++ -std=c++11 fifo.cpp host.cpp -lpthread -o host
#include "fifo.h"
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

volatile bool g_running = true;

void quit(int sig)
{
	g_running = false;
}

int main()
{
	signal(SIGINT, &quit);
	CFifoManager* fifo = CFifoManager::instance();
	if (!fifo)
	{
		printf("fifo instance failed!\n");
		return -1;
	}

	if (fifo->create("mmap.fifo", 1024) < 0)
	{
		printf("create fifo failed!\n");
		return -1;
	}

	printf("fifo create success, start read\n");

	while (g_running)
	{
		unsigned char buf[1024 * 3 + 1] = { 0, };
		int ret = fifo->read(buf, 1024 * 3 + 1);
		if (ret < 0)
		{
			printf("fifo read failed, %d!\n", ret);
			return -1;
		}
		else if (ret == 0)
		{
			//printf("read wait for data\n");
			usleep(1000 * 10);
		}
		else
		{
			printf("read result = %s\n", buf);
		}
	}

	printf("fifo read end\n");
	fifo->destroy();
	printf("fifo destroy end\n");
	return 0;
}
