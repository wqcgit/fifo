//
//  "$Id: fifo.h  2017-06-12 05:50:36Z wang_quancai $"
//
//  All Rights Reserved.
//
//	Description:    基于共享内存(mmap)的FIFO,用于多进程的通信
//	Revisions:		Year-Month-Day  SVN-Author  Modification
//

#ifndef __FIFO_MANAGER_H__
#define __FIFO_MANAGER_H__

#include <pthread.h>
#include <string>
#include <atomic>

/// fifo管理类
class CFifoManager
{
    /// 内存控制结构
    struct fifoCtl
    {
        unsigned int			in;
        unsigned int			out;
        bool                    ok;
        pthread_mutex_t			mutex;
        pthread_mutexattr_t		mutexAttr;

        /// 控制体构造
        fifoCtl();

        /// 控制体析构
        ~fifoCtl();
    };

    enum FifoState
    {
        FifoNone,
        FifoCreating,
        FifoValid,
        FifoOpenning,
        FifoDestroying,
    };

public:

    /// 创建fifo管理对象
    static CFifoManager* instance();

    /// host创建共享内存fifo
    /// param [file] [in]   映射文件
    /// param [size] [in]   映射大小
    /// return 0: 成功, <0 失败
    int create(const char* file, unsigned int size);

    /// host销毁共享内存fifo
    /// return 0: 成功, <0 失败
    int destroy();

    /// slave打开共享内存fifo
    /// param [file] [in]   映射文件
    /// return 0: 成功, <0 失败
    int open(const char* file);

    /// 向fifo写入数据
    /// param [data] [in]   待写入的数据
    /// param [len]  [in]   待写入的数据长度
    /// return >=0:实际写入的长度, <0 写入失败
    int write(unsigned char* data, unsigned int len);

    /// 从fifo读取数据
    /// param [buf]  [out]  待读取的缓存buf
    /// param [len]  [in]   缓存buf的长度
    /// return >=0:实际读取的长度, <0 读取失败
    int read(unsigned char* buf, unsigned int len);

private:

    /// 构造函数
    CFifoManager();

	/// 重置参数
	void reset();

private:

    /// 当前Fifo状态
    std::atomic<int>	m_state;

    /// 共享内存地址
    unsigned char*		m_mmap;

    /// 共享内存大小
    unsigned int        m_size;

	/// fifo缓存buffer
	unsigned char*		m_buffer;

	/// fifo大小
	unsigned int		m_bufLen;

    /// 共享内存控制结构
    fifoCtl*			m_ctl;
};

#endif

