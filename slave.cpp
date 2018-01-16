/// g++ -std=c++11 fifo.cpp slave.cpp -lpthread -o slave
#include "fifo.h"
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

const char* data[3] = 
{
	"wwwww",
	"qqqqq",
	"zzzzz", 
};

volatile bool g_running = true;

void quit(int sig)
{
	g_running = false;
}

int main()
{
	//signal(SIGINT, &quit);
	CFifoManager* fifo = CFifoManager::instance();
	if (!fifo)
	{
		printf("fifo instance failed!\n");
		return -1;
	}

	while (1)
	{
		if (fifo->open("mmap.fifo") < 0)
		{
			printf("open fifo failed!\n");
			sleep(1);
			continue;
		}
		break;
	}
	printf("fifo open success, start write\n");

	for (int t = 0; t < 10000; ++t)
	{
		for (int i = 0; i < 3; ++i)
		{
			int len = strlen(data[i]);
			int putLen = 0;
			while (1)
			{
				int ret = fifo->write((unsigned char*)data[i], len - putLen);
				if (ret < 0)
				{
					printf("fifo write error:%d\n", ret);
					return -1;
				}
				else if(ret == 0)
				{
					printf("fifo is full wait 100ms\n");
					usleep(1000 * 100);
				}

				//printf("put ret(%d) putLen(%d)  len(%d)\n", ret, putLen, len);

				if (ret + putLen == len)
				{
					break;
				}
				putLen += ret;
				printf("put ret(%d) + putLen(%d) != len(%d), continue put\n", ret, putLen, len);
			}
		}
	}
	printf("fifo write end\n");
	return 0;
}
