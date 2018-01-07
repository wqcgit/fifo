#include "fifo.h"
#include <stdio.h>
#include <unistd.h>
#include <signal.h>

volatile bool g_running = true;

void quit(int sig)
{
	g_running = false;
}

int main()
{
	signal(SIGINT, &quit);
	CFifoWithmmap* fifo = CFifoWithmmap::instance();
	if (!fifo)
	{
		printf("create fifo failed!\n");
		return -1;
	}
	if (fifo->init("mmap.fifo", 1024 * 1024, 1) < 0)
	{
		printf("init failed!\n");
		return -1;
	}

	printf("fifo init success, start get\n");

	while (g_running)
	{
		unsigned char buf[1024 * 1024 + 1] = { 0, };
		int ret = fifo->get(buf, 1024 * 1024 + 1);
		if (ret < 0)
		{
			printf("fifo get failed!\n");
			return -1;
		}
		else if (ret == 0)
		{
			usleep(1000*10);
		}
		else
		{
			printf("buf = %s\n", buf);
		}
	}

	printf("fifo get end\n");
	fifo->fini();
	printf("fifo fini end\n");
	return 0;
}
