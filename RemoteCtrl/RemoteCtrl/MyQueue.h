#pragma once
#include <atomic>
#include "pch.h"
#include <list>
#include "MyThread.h"

template<class T>
class CMyQueue
{//线程安全的队列 利用IOCP实现
public:
	enum 
	{
		EQNone,
		EQPush,
		EQPop,
		EQSize,
		EQClear
	};
	typedef struct IocpParam {
		int nOperator;
		T Data;
		HANDLE hEvent;//pop操作需要的
		IocpParam(int op, const T& sData = NULL, HANDLE hEve = NULL)
		{
			nOperator = op;
			Data = sData;
			hEvent = hEve;
		}
		IocpParam()
		{
			nOperator = EQNone;
		}
	}PPARAM;//post parameter 用于投递信息的结构体
	
public:
	CMyQueue()
	{
		m_lock = FALSE;
		m_hCompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, 1);
		m_hThread = INVALID_HANDLE_VALUE;
		if (m_hCompletionPort != NULL)
		{
			m_hThread = (HANDLE)_beginthread(&CMyQueue<T>::threadEntry, 0, this);
		}
	}
	virtual ~CMyQueue()
	{
		if (m_lock) return;
		m_lock = TRUE;
		PostQueuedCompletionStatus(m_hCompletionPort, 0, NULL, NULL);
		WaitForSingleObject(m_hThread, INFINITE);
		if(m_hCompletionPort != NULL)
		{
			HANDLE hTemp = m_hCompletionPort;
			m_hCompletionPort = NULL;
			CloseHandle(hTemp);
		}
	}
	bool PushBack(const T& data)
	{
		IocpParam* pParam = new IocpParam(EQPush, data);
		if (m_lock) 
		{
			delete pParam;
			return false;
		}
		bool ret = PostQueuedCompletionStatus(m_hCompletionPort, sizeof(PPARAM), (ULONG_PTR)pParam, NULL);
		if (ret == false) delete pParam;
		TRACE("pushback ret = %d, %08X\n", ret, pParam);
		return ret;
	}
	virtual bool PopFront(T& data)
	{
		HANDLE hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
		IocpParam pParam(EQPop, data, hEvent);
		if (m_lock)
		{
			if (hEvent) CloseHandle(hEvent);
			return false;
		}
		bool ret = PostQueuedCompletionStatus(m_hCompletionPort, sizeof(PPARAM), (ULONG_PTR)&pParam, NULL);
		if (ret == false)
		{
			CloseHandle(hEvent);
			return false;
		}
		ret = WaitForSingleObject(hEvent, INFINITE) == WAIT_OBJECT_0;
		if (ret)
		{
			data = pParam.Data;
		}
		return ret;
	}
	int Size()
	{
		HANDLE hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
		IocpParam pParam(EQSize, T(), hEvent);
		if (m_lock)
		{
			if (hEvent) CloseHandle(hEvent);
			return -1;
		}
		bool ret = PostQueuedCompletionStatus(m_hCompletionPort, sizeof(PPARAM), (ULONG_PTR)&pParam, NULL);
		if (ret == false)
		{
			CloseHandle(hEvent);
			return -1;
		}
		ret = WaitForSingleObject(hEvent, INFINITE) == WAIT_OBJECT_0;
		if (ret)
		{
			return pParam.nOperator;
		}
		return -1;
	}
	bool Clear()
	{
		if (m_lock) return false;
		IocpParam* pParam = new IocpParam(EQClear, T());
		bool ret = PostQueuedCompletionStatus(m_hCompletionPort, sizeof(PPARAM), (ULONG_PTR)pParam, NULL);
		if (ret == false) delete pParam;
		TRACE("clear ret = %d, %08X\n", ret, pParam);
		return ret;
	}
protected:
	static void threadEntry(void* arg)
	{
		CMyQueue<T>* thiz = (CMyQueue<T>*) arg;
		thiz->threadMain();
		_endthread();
	}
	virtual void DealParam(PPARAM* pParam)
	{
		switch (pParam->nOperator)
		{
		case EQPush:
			m_lstData.push_back(pParam->Data);
			delete pParam; 
			break;
		case EQPop:
			if (m_lstData.size() > 0)
			{
				pParam->Data = m_lstData.front();
				m_lstData.pop_front();
			}
			if (pParam->hEvent != NULL)
				SetEvent(pParam->hEvent);
			break;
		case EQSize:
			pParam->nOperator = (int)m_lstData.size();
			if (pParam->hEvent != NULL)
				SetEvent(pParam->hEvent);
			break;
		case EQClear:
			m_lstData.clear();
			delete pParam;
			break;
		default:
			OutputDebugStringA("unknown operator\n");
			break;
		}
	}
	virtual void threadMain()
	{
		PPARAM* pParam = NULL;
		DWORD dwTransferred = 0;
		ULONG_PTR CompeletionKey = 0;
		OVERLAPPED* pOverlapped = NULL;
		while (GetQueuedCompletionStatus(m_hCompletionPort, &dwTransferred, &CompeletionKey, &pOverlapped, INFINITE))
		{
			if (dwTransferred == 0 || CompeletionKey == NULL)
			{
				printf("thread is readly to exit\n");
				break;
			}
			pParam = (PPARAM*)CompeletionKey;
			DealParam(pParam);
		}
		while (GetQueuedCompletionStatus(m_hCompletionPort, &dwTransferred, &CompeletionKey, &pOverlapped, 0))
		{
			if (dwTransferred == 0 || CompeletionKey == NULL)
			{
				printf("thread is real to exit\n");
				continue;
			}
			pParam = (PPARAM*)CompeletionKey;
			DealParam(pParam);
		}
		HANDLE hTemp = m_hCompletionPort;
		m_hCompletionPort = NULL;
		CloseHandle(hTemp);
	}
protected:
	std::list<T> m_lstData;
	HANDLE m_hCompletionPort;
	HANDLE m_hThread;
	std::atomic<bool> m_lock;//队列正在析构
};

typedef int(ThreadFuncBase::* MYCALLBACK)();

template<class T>
class MySendQueue :public CMyQueue<T>, public ThreadFuncBase
{
public:
	typedef int(ThreadFuncBase::* MYCALLBACK)(T& data);
	MySendQueue(ThreadFuncBase* obj, MYCALLBACK callback) :
		CMyQueue<T>(),
		m_base(obj),
		m_callback(callback)
	{
		m_thread.Start();
		m_thread.UpdateWorker(::ThreadWorker(this, (FUNCTYPE)&MySendQueue<T>::threadTick));
	}
	virtual ~MySendQueue() 
	{
		m_base = NULL;
		m_callback = NULL;
		m_thread.Stop();
	}
protected:
	virtual bool PopFront(T& data) 
	{
		return false;
	}
	bool PopFront()
	{
		typename CMyQueue<T>::IocpParam* Param = new typename CMyQueue<T>::IocpParam(CMyQueue<T>::EQPop, T());
		if (CMyQueue<T>::m_lock)
		{
			delete Param;
			return false;
		}
		bool ret = PostQueuedCompletionStatus(CMyQueue<T>::m_hCompletionPort, sizeof(*Param), (ULONG_PTR)&Param, NULL);
		if (ret == false)
		{
			delete Param;
			return false;
		}
		return ret;
	}
	int threadTick()
	{
		if (WaitForSingleObject(CMyQueue<T>::m_hThread, 0) != WAIT_TIMEOUT) return 0;
		if (CMyQueue<T>::m_lstData.size() > 0)
		{
			PopFront();
		}
		Sleep(1);
		return 0;
	}

	virtual void DealParam(typename CMyQueue<T>::PPARAM* pParam)
	{
		switch (pParam->nOperator)
		{
		case CMyQueue<T>::EQPush:
			CMyQueue<T>::m_lstData.push_back(pParam->Data);
			delete pParam;
			break;
		case CMyQueue<T>::EQPop:
			if (CMyQueue<T>::m_lstData.size() > 0)
			{
				pParam->Data = CMyQueue<T>::m_lstData.front();
				if((m_base->*m_callback)(pParam->Data) == 0)
				{
					CMyQueue<T>::m_lstData.pop_front();
				}
			}
			delete pParam;
			break;
		case CMyQueue<T>::EQSize:
			pParam->nOperator = (int)CMyQueue<T>::m_lstData.size();
			if (pParam->hEvent != NULL)
				SetEvent(pParam->hEvent);
			break;
		case CMyQueue<T>::EQClear:
			CMyQueue<T>::m_lstData.clear();
			delete pParam;
			break;
		default:
			OutputDebugStringA("unknown operator\n");
			break;
		}
	}
private:
	ThreadFuncBase* m_base;
	MYCALLBACK m_callback;
	CMyThread m_thread;
};
typedef MySendQueue<std::vector<char>>::MYCALLBACK SENDCALLBACK;