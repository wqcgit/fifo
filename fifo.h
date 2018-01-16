//
//  "$Id: fifo.h  2017-06-12 05:50:36Z wang_quancai $"
//
//  All Rights Reserved.
//
//	Description:    ���ڹ����ڴ�(mmap)��FIFO,���ڶ���̵�ͨ��
//	Revisions:		Year-Month-Day  SVN-Author  Modification
//

#ifndef __FIFO_MANAGER_H__
#define __FIFO_MANAGER_H__

#include <pthread.h>
#include <string>
#include <atomic>

/// fifo������
class CFifoManager
{
    /// �ڴ���ƽṹ
    struct fifoCtl
    {
        unsigned int			in;
        unsigned int			out;
        bool                    ok;
        pthread_mutex_t			mutex;
        pthread_mutexattr_t		mutexAttr;

        /// �����幹��
        fifoCtl();

        /// ����������
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

    /// ����fifo�������
    static CFifoManager* instance();

    /// host���������ڴ�fifo
    /// param [file] [in]   ӳ���ļ�
    /// param [size] [in]   ӳ���С
    /// return 0: �ɹ�, <0 ʧ��
    int create(const char* file, unsigned int size);

    /// host���ٹ����ڴ�fifo
    /// return 0: �ɹ�, <0 ʧ��
    int destroy();

    /// slave�򿪹����ڴ�fifo
    /// param [file] [in]   ӳ���ļ�
    /// return 0: �ɹ�, <0 ʧ��
    int open(const char* file);

    /// ��fifoд������
    /// param [data] [in]   ��д�������
    /// param [len]  [in]   ��д������ݳ���
    /// return >=0:ʵ��д��ĳ���, <0 д��ʧ��
    int write(unsigned char* data, unsigned int len);

    /// ��fifo��ȡ����
    /// param [buf]  [out]  ����ȡ�Ļ���buf
    /// param [len]  [in]   ����buf�ĳ���
    /// return >=0:ʵ�ʶ�ȡ�ĳ���, <0 ��ȡʧ��
    int read(unsigned char* buf, unsigned int len);

private:

    /// ���캯��
    CFifoManager();

	/// ���ò���
	void reset();

private:

    /// ��ǰFifo״̬
    std::atomic<int>	m_state;

    /// �����ڴ��ַ
    unsigned char*		m_mmap;

    /// �����ڴ��С
    unsigned int        m_size;

	/// fifo����buffer
	unsigned char*		m_buffer;

	/// fifo��С
	unsigned int		m_bufLen;

    /// �����ڴ���ƽṹ
    fifoCtl*			m_ctl;
};

#endif

