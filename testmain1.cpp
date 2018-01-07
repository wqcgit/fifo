#include "fifo.h"
#include <stdio.h>
#include <string.h>

const char* data[3] = { "wwwww",
						"qqqqq",
						"zzzzz", };

volatile bool g_running = true;

void quit(int sig)
{
	g_running = false;
}

int main()
{
	CFifoWithmmap* fifo = CFifoWithmmap::instance();
	if (!fifo)
	{
		printf("create fifo failed!\n");
		return -1;
	}
	if (fifo->init("mmap.fifo", 1024 * 1024) < 0)
	{
		printf("init failed!\n");
		return -1;
	}

	printf("fifo init success, start put\n");

	for (int t = 0; t < 1000; ++t)
	{
		for (int i = 0; i < 3; ++i)
		{
			int len = strlen(data[i]);
			int putLen = 0;
			while (1)
			{
				int ret = fifo->put((unsigned char*)data[i], len - putLen);
				if (ret < 0)
				{
					printf("fifo put error:%d\n", ret);
					return -1;
				}

				if (ret + putLen == len)
				{
					break;
				}
				printf("put ret(%d) != len(%d), continue put\n", ret, len);
				putLen = ret;
			}
		}
	}
	printf("fifo put end\n");
	//fifo->fini();
	printf("fifo fini end\n");
	return 0;
}
