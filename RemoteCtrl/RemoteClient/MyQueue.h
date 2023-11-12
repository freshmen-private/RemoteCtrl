#pragma once

#include <list>
#include <string>

template<class T>
class CMyQueue
{//线程安全的队列 利用IOCP实现
public:
	CMyQueue();
	~CMyQueue();
	bool PushBack(const T& data);
	bool PopFront(T& data);
	int Size();
	void Clear();
private:
	static void threadEntry(void* arg);
	void threadMain();
private:
	std::list<T>m_lstData;
	HANDLE m_hCompletionPort;
	HANDLE m_hThread;
public:
	typedef struct IocpParam {
		int nOperator;
		T strData;
		HANDLE hEvent;//pop操作需要的
		IocpParam(int op, const char* sData)
		{
			nOperator = op;
			strData = sData;
		}
		IocpParam()
		{
			nOperator = -1;
		}
	}PPARAM;//post parameter 用于投递信息的结构体
	enum {
		EQPush = 0,
		EQPop,
		EQSize
		EQClear
	};
};


