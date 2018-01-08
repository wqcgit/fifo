#include "fifo.h"
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

CFifoWithmmap* CFifoWithmmap::instance()
{
	static CFifoWithmmap* s_fifo;
	if (!s_fifo)
	{
		static std::mutex s_fifoMtx;
		std::unique_lock<std::mutex> lck(s_fifoMtx);
		if (!s_fifo)
		{
			s_fifo = new CFifoWithmmap();
		}
	}
	return s_fifo;
}

CFifoWithmmap::fifoCtl::fifoCtl(unsigned char* buf, unsigned int buf_size)
: buffer(buf)
, size(buf_size)
, in(0)
, out(0)
{
	pthread_mutexattr_init(&mutexAttr);
	if (pthread_mutexattr_setpshared(&mutexAttr, PTHREAD_PROCESS_SHARED))
	{
		printf("fifo set mutex shared failed,%d\n", errno);
		throw "set shared failed";
	}
	pthread_mutex_init(&mutex, &mutexAttr);
}

CFifoWithmmap::fifoCtl::~fifoCtl()
{
	pthread_mutex_destroy(&mutex);
}

CFifoWithmmap::CFifoWithmmap()
: m_init(false)
, m_mmap(0)
, m_ctl(0)
, m_mapsize(0)
{
	printf("fifo ctl len = %d\n", sizeof(CFifoWithmmap::fifoCtl));
}

/// type: 0(create), 1(open)
/// size must be power of 2
int CFifoWithmmap::init(const std::string& file, unsigned int size, int type/* = 0*/)
{
	if (size & (size - 1))
	{
		printf("fifo size (%u) error, must be power of 2\n", size);
		return -1;
	}

	std::unique_lock<std::mutex> lck(m_mutex);
	if (m_init)
	{
		printf("fifo already init!\n");
		return -1;
	}

	int open_flag = (type ? O_CREAT | O_RDWR : O_CREAT | O_RDWR | O_TRUNC);
	m_mapsize = sizeof(CFifoWithmmap::fifoCtl) + size;
	int fd = ::open(file.c_str(), open_flag, 00777);
	if (fd < 0)
	{
		printf("fifo open %s failed:%d\n", file.c_str(), errno);
		return -1;
	}
	if (ftruncate(fd, m_mapsize))
	{
		printf("file truncate file failed:%d!\n", errno);
		::close(fd);
		return -1;
	}
	void* mapAddr = ::mmap(0, m_mapsize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (mapAddr == MAP_FAILED)
	{
		printf("fifo map failed:%d\n", errno);
		::close(fd);
		return -1;
	}
	m_mmap = (unsigned char*)mapAddr;
	::close(fd);

	printf("fifo map addr = %p\n", m_mmap);
	/// create
	if (!type)
	{
		try
		{
			m_ctl = new(m_mmap) CFifoWithmmap::fifoCtl(m_mmap + sizeof(CFifoWithmmap::fifoCtl), size);
		}
		catch (...)
		{
			printf("fifo constuct ctl failed!\n");
			return -1;
		}
	}
	/// open
	else
	{
		m_ctl = (CFifoWithmmap::fifoCtl*)m_mmap;
		m_ctl->buffer = m_mmap + sizeof(CFifoWithmmap::fifoCtl);
	}

	m_init = true;
	return 0;
}

int CFifoWithmmap::put(unsigned char* data, unsigned int len)
{
	pthread_mutex_lock(&(m_ctl->mutex));
	if (!m_init)
	{
		printf("fifo put error, already fini\n");
		pthread_mutex_unlock(&(m_ctl->mutex));
		return -1;
	}

	unsigned int l;

	len = std::min(len, m_ctl->size - m_ctl->in + m_ctl->out);

	/* first put the data starting from fifo->in to buffer end */
	l = std::min(len, m_ctl->size - (m_ctl->in & (m_ctl->size - 1)));
	memcpy(m_ctl->buffer + (m_ctl->in & (m_ctl->size - 1)), data, l);

	/* then put the rest (if any) at the beginning of the buffer */
	memcpy(m_ctl->buffer, data + l, len - l);

	m_ctl->in += len;
	pthread_mutex_unlock(&(m_ctl->mutex));
	return len;
}

int CFifoWithmmap::get(unsigned char* buf, unsigned int len)
{
	pthread_mutex_lock(&(m_ctl->mutex));
	if (!m_init)
	{
		printf("fifo get error, already fini\n");
		pthread_mutex_unlock(&(m_ctl->mutex));
		return -1;
	}
	unsigned int l;

	len = std::min(len, m_ctl->in - m_ctl->out);

	/* first get the data from fifo->out until the end of the buffer */
	l = std::min(len, m_ctl->size - (m_ctl->out & (m_ctl->size - 1)));
	memcpy(buf, m_ctl->buffer + (m_ctl->out & (m_ctl->size - 1)), l);

	/* then get the rest (if any) from the beginning of the buffer */
	memcpy(buf + l, m_ctl->buffer, len - l);

	m_ctl->out += len;
	pthread_mutex_unlock(&(m_ctl->mutex));

	return len;
}

int CFifoWithmmap::fini()
{
	if (!m_init)
	{
		printf("fifo already fini!\n");
		return 0;
	}

	pthread_mutex_lock(&(m_ctl->mutex));
	m_init = false;
	pthread_mutex_unlock(&(m_ctl->mutex));
	m_ctl->~fifoCtl();
	munmap(m_mmap, m_mapsize);
	return 0;
}