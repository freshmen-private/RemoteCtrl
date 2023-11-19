#pragma once
#include "pch.h"
#include <atomic>
#include <vector>
#include <mutex>

class ThreadFuncBase {};
typedef int(ThreadFuncBase::* FUNCTYPE)();
class ThreadWorker
{
public:
	ThreadWorker() :thiz(NULL), func(NULL) {}
	ThreadWorker(void* obj, FUNCTYPE f) :thiz((ThreadFuncBase*)obj), func(f) {}
	ThreadWorker(const ThreadWorker& worker)
	{
		thiz = worker.thiz;
		func = worker.func;
	}
	ThreadWorker& operator=(const ThreadWorker& worker)
	{
		if (this != &worker)
		{
			thiz = worker.thiz;
			func = worker.func;
		}
		return *this;
	}
	int operator()()
	{
		if (thiz)
		{
			return (thiz->*func)();
		}
		return -1;
	}
	bool IsValid() const
	{
		return (thiz != NULL) && (func != NULL);
	}
private:
	ThreadFuncBase* thiz;
	int (ThreadFuncBase::* func)();
};

class CMyThread
{
public:
	CMyThread()
	{
		m_hThread = NULL;
		handleID = 0;
		m_bStatus = false;
	}
	~CMyThread()
	{
		Stop();
	}

	bool Start()
	{
		m_bStatus = true;
		m_hThread = (HANDLE)_beginthread(&CMyThread::ThreadEntry, 0, this);
		if (!IsValid())
		{
			m_bStatus = false;
		}
		return m_bStatus;
	}

	bool IsValid()//����true��ʾ��Ч�� ����false��ʾ�߳��쳣�����Ѿ���ֹ
	{
		if (m_hThread == NULL || m_hThread == INVALID_HANDLE_VALUE) return false;
		return WaitForSingleObject(m_hThread, 0) == WAIT_TIMEOUT;
	}

	bool Stop()
	{
		if (m_bStatus == false) return true;
		m_bStatus = false;
		DWORD ret = WaitForSingleObject(m_hThread, 1000);
		if (ret == WAIT_OBJECT_0)
		{
			TerminateThread(m_hThread, -1);
		}
		UpdateWorker();
		return ret == WAIT_OBJECT_0;
	}
	void UpdateWorker(const ::ThreadWorker& worker = ::ThreadWorker())
	{
		if (m_worker.load() != NULL && m_worker.load() != &worker)
		{
			::ThreadWorker* pWorker = m_worker.load();
			m_worker.store(NULL);
			delete pWorker;
		}
		if (m_worker.load() == &worker) { return; }
		if (!worker.IsValid())
		{
			m_worker.store(NULL);
			return;
		}
		::ThreadWorker* pWorker = new ::ThreadWorker(worker) ;
		m_worker.store(pWorker);
	}
	//true ��ʾ���У�false��ʾ�Ѿ������˹���
	bool IsIdle()
	{
		if (m_worker.load() == NULL)return true;
		return !m_worker.load()->IsValid();
	}

private:
	void ThreadWorker()
	{
		while (m_bStatus)
		{
			if (m_worker.load() == NULL)
			{
				Sleep(1);
				continue;
			}
			::ThreadWorker worker = *m_worker.load();
			if (worker.IsValid())
			{
				//HANDLE current = OpenThread(THREAD_ALL_ACCESS, FALSE, GetCurrentThreadId());
				if (WaitForSingleObject(m_hThread, 0) == WAIT_TIMEOUT)
				{
					int ret = worker();
					if (ret != 0)
					{
						CString str;
						str.Format(_T("thread found warning code %d %d\n"), ret, m_hThread);
						OutputDebugString(str);
					}
					else if (ret == 0)
					{
						::ThreadWorker* pWorker = m_worker.load();
						m_worker.store(NULL);
						delete pWorker;
					}
				}
				//TRACE("operation is completed\n");
			}
			else
			{
				 Sleep(1);
			}
		 }
	}

	static void ThreadEntry(void* arg)
	{
		CMyThread* thiz = (CMyThread*)arg;
		if (thiz)
		{
			thiz->ThreadWorker();
		}
		_endthread();
	}
private:
	HANDLE m_hThread;
	int handleID;
	bool m_bStatus;//false ��ʾ�߳̽�Ҫ�رգ� true��ʾ�߳���������
	std::atomic<::ThreadWorker*> m_worker;
};

class MyThreadPool
{
public:
	MyThreadPool(size_t size)
	{
		m_threads.resize(size);
		for (size_t i = 0; i < size; i++)
		{
			m_threads[i] = new CMyThread();
		}
	}
	MyThreadPool() {}
	~MyThreadPool() 
	{
		Stop();
		for (size_t i = 0; i < m_threads.size(); i++)
		{
			delete m_threads[i];
			m_threads[i] = NULL;
		}
		m_threads.clear();
	}
	bool Invoke()
	{
		bool ret = true;
		for (size_t i = 0; i < m_threads.size(); i++)
		{
			if (m_threads[i]->Start() == false)
			{
				ret = false;
				break;
			}
		}
		if (ret == false)
		{
			for (size_t i = 0; i < m_threads.size(); i++)
			{
				m_threads[i]->Stop();
			}
		}
		return ret;
	}
	void Stop()
	{
		for (size_t i = 0; i < m_threads.size(); i++)
		{
			m_threads[i]->Stop();
		}
	}
	//����-1 ��ʾ����ʧ�ܣ������̶߳���æ ����0 ��ʾ��n���̷߳��������������
	int DispatchWorker(const ThreadWorker& worker)
	{
		int index = -1;
		m_lock.lock();
		for (int i = 0; i < m_threads.size(); i++)
		{
			if (m_threads[i]->IsIdle())
			{
				m_threads[i]->UpdateWorker(worker);
				index = i;
				break;
			}
		}
		m_lock.unlock();
		return index;
	}

	bool CheckThreadValid(size_t index) 
	{
		if (index < m_threads.size())
		{
			return m_threads[index]->IsValid();
		}
		return false;
	}
private:
	std::mutex m_lock;
	std::vector<CMyThread*> m_threads;
};