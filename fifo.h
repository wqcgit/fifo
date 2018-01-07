#ifndef __FIFO_WITH_MMAP_H__
#define __FIFO_WITH_MMAP_H__

#include <pthread.h>
#include <string>
#include <atomic>
#include <mutex>

class CFifoWithmmap
{
	struct fifoCtl
	{
		unsigned char*			buffer;
		unsigned int			size;
		unsigned int			in;
		unsigned int			out;
		pthread_mutex_t			mutex;
		pthread_mutexattr_t		mutexAttr;
		
		fifoCtl(unsigned char* buf, unsigned int buf_size);

		~fifoCtl();
	};

public:

	static CFifoWithmmap* instance();

	/// type: 0(create), 1(open)
	/// size must be power of 2
	int init(const std::string& file, unsigned int size, int type = 0);

	int put(unsigned char* data, unsigned int len);

	int get(unsigned char* buf, unsigned int len);

	int fini();

private:

	CFifoWithmmap();

private:

	std::atomic<bool>	m_init;

	std::mutex			m_mutex;

	unsigned char*		m_mmap;

	fifoCtl*			m_ctl;

	unsigned int		m_mapsize;
};

#endif
