#include "fifo.h"
#include <mutex>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

/// 控制体构造
CFifoManager::fifoCtl::fifoCtl()
: in(0)
, out(0)
, ok(false)
{
    pthread_mutexattr_init(&mutexAttr);
    if (pthread_mutexattr_setpshared(&mutexAttr, PTHREAD_PROCESS_SHARED) ||
        pthread_mutex_init(&mutex, &mutexAttr))
    {
        printf("fifo init shared mutex failed,%d\n", errno);
    }
    else
    {
        ok = true;
    }
}

/// 控制体析构
CFifoManager::fifoCtl::~fifoCtl()
{
	ok = false;
    pthread_mutex_destroy(&mutex);
    in = 0;
    out = 0;
}

/// 创建fifo管理对象
CFifoManager* CFifoManager::instance()
{
    static CFifoManager* s_fifoMgr;
    if (!s_fifoMgr)
    {
        static std::mutex s_fifoMtx;
        std::unique_lock<std::mutex> lck(s_fifoMtx);
        if (!s_fifoMgr)
        {
            s_fifoMgr = new CFifoManager();
        }
    }
    return s_fifoMgr;
}

/// 构造函数
CFifoManager::CFifoManager()
: m_state(FifoNone)
, m_mmap(0)
, m_size(0)
, m_ctl(0)
{
    printf("fifo ctl len = %d\n", sizeof(CFifoManager::fifoCtl));
}

/// host创建共享内存fifo
/// param [file] [in]   映射文件
/// param [size] [in]   fifo大小
/// return 0: 成功, <0 失败
int CFifoManager::create(const char* file, unsigned int size)
{
    if ((size & (size - 1)) || !file)
    {
        printf("fifo size (%u) error, must be power of 2, file:%p\n", size, file);
        return -1;
    }

    if (m_state != CFifoManager::FifoNone)
    {
        printf("fifo create state[%d] busy\n", m_state.load());
        return -1;
    }

    m_state = CFifoManager::FifoCreating;
    m_size = sizeof(CFifoManager::fifoCtl) + size;
	m_bufLen = size;
    
    int fd = 0;
    bool mapOk = false;
    do
    {
        fd = ::open(file, O_CREAT | O_RDWR | O_TRUNC, 00777);
        if (fd < 0)
        {
            printf("fifo open %s failed:%d\n", file, errno);
            break;
        }

        if (::ftruncate(fd, m_size))
        {
            printf("fifo truncate file failed:%d!\n", errno);
            break;
        }

        void* mapAddr = ::mmap(0, m_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (mapAddr == MAP_FAILED)
        {
            printf("fifo map file failed:%d\n", errno);
            break;
        }
        m_mmap = (unsigned char*)mapAddr;
		m_buffer = m_mmap + sizeof(CFifoManager::fifoCtl);
        printf("fifo create map addr = %p, fifo addr = %p\n", m_mmap, m_buffer);

        m_ctl = new(m_mmap)CFifoManager::fifoCtl();
        if (!m_ctl->ok)
        {
            printf("fifo constuct ctl failed!\n");
            m_ctl->~fifoCtl();
            break;
        }
        mapOk = true;
    } while (0);

    if (fd > 0)
    {
        ::close(fd);
    }

    /// create failed
    if (!mapOk)
    {
		reset();
        return -1;
    }

    m_state = CFifoManager::FifoValid;
    return 0;
}

/// host销毁共享内存fifo
/// return 0: 成功, <0 失败
int CFifoManager::destroy()
{
    if (m_state != CFifoManager::FifoValid)
    {
        printf("fifo destroy state[%d] busy!\n", m_state.load());
        return -1;
    }

    m_state = CFifoManager::FifoDestroying;
    /// ensure read/write end
    pthread_mutex_lock(&(m_ctl->mutex));
    pthread_mutex_unlock(&(m_ctl->mutex));
    m_ctl->~fifoCtl();
	reset();
    m_state = CFifoManager::FifoNone;
    return 0;
}

/// slave打开共享内存fifo
/// param [file] [in]   映射文件
/// return 0: 成功, <0 失败
int CFifoManager::open(const char* file)
{
    if (!file)
    {
        printf("fifo open error, file is null\n");
        return -1;
    }

    if (m_state != CFifoManager::FifoNone)
    {
        printf("fifo open state[%d] busy\n", m_state.load());
        return -1;
    }

    m_state = CFifoManager::FifoOpenning;
    int fd = 0;
    bool mapOk = false;
    do
    {
        fd = ::open(file, O_RDWR, 00777);
        if (fd < 0)
        {
            printf("fifo open %s failed:%d\n", file, errno);
            break;
        }

        struct stat fileStat = { 0, };
        if (::fstat(fd, &fileStat))
        {
            printf("fifo fstat file failed:%d!\n", errno);
            break;
        }

        m_size = fileStat.st_size;
		printf("fifo open mmap size = %d\n", m_size);
        void* mapAddr = ::mmap(0, m_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (mapAddr == MAP_FAILED)
        {
            printf("fifo map file failed:%d\n", errno);
            break;
        }
        m_mmap = (unsigned char*)mapAddr;
		m_buffer = m_mmap + sizeof(CFifoManager::fifoCtl);
		m_bufLen = m_size - sizeof(CFifoManager::fifoCtl);
        printf("fifo open map addr = %p, fifo addr = %p\n", m_mmap, m_buffer);

        m_ctl = (CFifoManager::fifoCtl*)m_mmap;
        if (!m_ctl->ok)
        {
            printf("fifo open ctl failed!\n");
            break;
        }
        mapOk = true;
    } while (0);
    if (fd > 0)
    {
        ::close(fd);
    }

    /// open failed
    if (!mapOk)
    {
		reset();
        return -1;
    }

    m_state = CFifoManager::FifoValid;
    return 0;
}

/// 向fifo写入数据
/// param [data] [in]   待写入的数据
/// param [len]  [in]   待写入的数据长度
/// return >=0:实际写入的长度, <0 写入失败
int CFifoManager::write(unsigned char* data, unsigned int len)
{
    pthread_mutex_lock(&(m_ctl->mutex));
    if (m_state != CFifoManager::FifoValid)
    {
        printf("fifo write error, state[%d] invalid\n", m_state.load());
        pthread_mutex_unlock(&(m_ctl->mutex));
        return -1;
    }

    unsigned int l;

    len = std::min(len, m_bufLen - m_ctl->in + m_ctl->out);

    /* first put the data starting from fifo->in to buffer end */
    l = std::min(len, m_bufLen - (m_ctl->in & (m_bufLen - 1)));
    memcpy(m_buffer + (m_ctl->in & (m_bufLen - 1)), data, l);

    /* then put the rest (if any) at the beginning of the buffer */
    memcpy(m_buffer, data + l, len - l);

    m_ctl->in += len;
    pthread_mutex_unlock(&(m_ctl->mutex));
    return len;
}

/// 从fifo读取数据
/// param [buf]  [out]  待读取的缓存buf
/// param [len]  [in]   缓存buf的长度
/// return >=0:实际读取的长度, <0 读取失败
int CFifoManager::read(unsigned char* buf, unsigned int len)
{
    pthread_mutex_lock(&(m_ctl->mutex));
    if (m_state != CFifoManager::FifoValid)
    {
        printf("fifo read error, state[%d] invalid\n", m_state.load());
        pthread_mutex_unlock(&(m_ctl->mutex));
        return -1;
    }
    unsigned int l;

    len = std::min(len, m_ctl->in - m_ctl->out);

    /* first get the data from fifo->out until the end of the buffer */
    l = std::min(len, m_bufLen - (m_ctl->out & (m_bufLen - 1)));
    memcpy(buf, m_buffer + (m_ctl->out & (m_bufLen - 1)), l);

    /* then get the rest (if any) from the beginning of the buffer */
    memcpy(buf + l, m_buffer, len - l);

    m_ctl->out += len;
    pthread_mutex_unlock(&(m_ctl->mutex));
    return len;
}

/// 重置参数
void CFifoManager::reset()
{
	if (m_mmap)
	{
		::munmap(m_mmap, m_size);
	}
	m_mmap = 0;
	m_size = 0;
	m_buffer = 0;
	m_bufLen = 0;
	m_ctl = 0;
	m_state = CFifoManager::FifoNone;
}

