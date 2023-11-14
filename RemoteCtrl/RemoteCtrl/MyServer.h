#pragma once
#include "MyThread.h"
#include <map>
#include "MyQueue.h"
#include <MSWSock.h>

enum COperator{
	CNone,
	CAccept,
	CRecv,
	CSend,
	CError
};

class CMyServer;
class MyClient;

typedef std::shared_ptr<MyClient> PCLIENT;

class COverlapped
{
public:
	OVERLAPPED m_overlapped;
	DWORD m_operator;//操作 参见COperator
	std::vector<char> m_buffer;
	ThreadWorker m_worker;//处理函数
	CMyServer* m_server;//服务器对象
};
template<COperator> class AcceptOverlapped;
typedef AcceptOverlapped<CAccept> ACCEPTOVERLAPPED;

class MyClient
{
public:
	MyClient();

	~MyClient()
	{
		closesocket(m_sock);
	}

	void SetOverlapped(PCLIENT& ptr);

	operator SOCKET()
	{
		return m_sock;
	}
	operator PVOID()
	{
		return &m_buffer[0];
	}
	operator LPOVERLAPPED();
	operator LPDWORD()
	{
		return &m_received;
	}
	sockaddr_in* GetLocalAddr()
	{
		return &m_laddr;
	}
	sockaddr_in* GetRemoteAddr()
	{
		return &m_raddr;
	}
private:
	SOCKET m_sock;
	DWORD m_received;
	std::shared_ptr<ACCEPTOVERLAPPED> m_overlapped;
	std::vector<char> m_buffer;
	sockaddr_in m_laddr;
	sockaddr_in m_raddr;
	bool m_isbusy;
};

template<COperator>
class AcceptOverlapped :public COverlapped, ThreadFuncBase
{
public:
	AcceptOverlapped();
	int AcceptWorker();
	PCLIENT m_client;
};


template<COperator>
class RecvOverlapped :public COverlapped, ThreadFuncBase
{
public:
	RecvOverlapped() :m_operator(CRecv), m_worker(this, &RecvOverlapped::RecvWorker)
	{
		memset(&m_overlapped, 0, sizeof(m_overlapped));
		m_buffer.resize(1024 * 256);
	}
	int RecvWorker()
	{
		//TODO
	}
};
typedef RecvOverlapped<CRecv> RECVOVERLAPPED;



template<COperator>
class SendOverlapped :public COverlapped, ThreadFuncBase
{
public:
	SendOverlapped() :m_operator(CSend), m_worker(this, &SendOverlapped::SendWorker)
	{
		memset(&m_overlapped, 0, sizeof(m_overlapped));
		m_buffer.resize(1024 * 256);
	}
	int SendWorker()
	{
		//TODO
	}
};
typedef SendOverlapped<CSend> SENDOVERLAPPED;

template<COperator>
class ErrorOverlapped :public COverlapped, ThreadFuncBase
{
public:
	ErrorOverlapped() :m_operator(CError), m_worker(this, &ErrorOverlapped::ErrorWorker)
	{
		memset(&m_overlapped, 0, sizeof(m_overlapped));
		m_buffer.resize(1024);
	}
	int ErrorWorker()
	{
		//TODO
	}
};
typedef ErrorOverlapped<CError> ERROROVERLAPPED;



class CMyServer :public ThreadFuncBase
{
public:
	CMyServer(const std::string& ip = "0.0.0.0", short port = 9527) :m_pool(10)
	{
		m_hIOCP = INVALID_HANDLE_VALUE;
		m_sock = INVALID_SOCKET;
		m_addr.sin_family = AF_INET;
		m_addr.sin_port = htons(port);
		m_addr.sin_addr.s_addr = inet_addr(ip.c_str());
	}
	~CMyServer() {}

	bool StartService()
	{
		CreateSocket();
		if (bind(m_sock, (sockaddr*)&m_addr, sizeof(m_addr)) == -1)
		{
			closesocket(m_sock);
			m_sock = INVALID_SOCKET;
			return false;
		}
		if (listen(m_sock, 3) == -1)
		{
			closesocket(m_sock);
			m_sock = INVALID_SOCKET;
			return false;
		}
		m_hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 4);
		if (m_hIOCP == NULL || m_hIOCP == INVALID_HANDLE_VALUE)
		{
			closesocket(m_sock);
			m_sock = INVALID_SOCKET;
			m_hIOCP = INVALID_HANDLE_VALUE;
			return false;
		}
		CreateIoCompletionPort((HANDLE)m_sock, m_hIOCP, (ULONG_PTR)this, 0);
		m_pool.Invoke();
		m_pool.DispatchWorker(ThreadWorker(this, (FUNCTYPE)&CMyServer::threadIocp));
		if (NewAccept())
		{
			return false;
		}
		/*m_pool.DispatchWorker(ThreadWorker(this, (FUNCTYPE)&CMyServer::threadIocp));
		m_pool.DispatchWorker(ThreadWorker(this, (FUNCTYPE)&CMyServer::threadIocp));
		m_pool.DispatchWorker(ThreadWorker(this, (FUNCTYPE)&CMyServer::threadIocp));*/
		return true;
	}

	bool NewAccept()
	{
		PCLIENT pClient(new MyClient());
		pClient->SetOverlapped(pClient);
		m_client.insert(std::pair<SOCKET, PCLIENT>(*pClient, pClient));
		if (!AcceptEx(m_sock, *pClient, *pClient, 0, sizeof(sockaddr_in) + 16, sizeof(sockaddr_in) + 16, *pClient, *pClient))
		{
			closesocket(m_sock);
			m_sock = INVALID_SOCKET;
			m_hIOCP = INVALID_HANDLE_VALUE;
			return false;
		}
		return true;
	}

private:
	void CreateSocket()
	{
		m_sock = WSASocket(PF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
		int opt = 1;
		setsockopt(m_sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
	}
	
	int threadIocp()
	{
		DWORD transferred = 0;
		ULONG_PTR CompletionKey = 0;
		OVERLAPPED* lpOverlapped = NULL;
		if (GetQueuedCompletionStatus(m_hIOCP, &transferred, &CompletionKey, &lpOverlapped, INFINITE))
		{
			if(transferred > 0 && CompletionKey != 0)
			{
				COverlapped* pOverlapped = CONTAINING_RECORD(lpOverlapped, COverlapped, m_overlapped);
				switch (pOverlapped->m_operator)
				{
				case CAccept:
				{
					ACCEPTOVERLAPPED* pAccept = (ACCEPTOVERLAPPED*)pOverlapped;
					m_pool.DispatchWorker(pAccept->m_worker);
				}
					break;
				case CRecv:
				{
					RECVOVERLAPPED* pRecv = (RECVOVERLAPPED*)pOverlapped;
					m_pool.DispatchWorker(pRecv->m_worker);
				}
					break;
				case CSend:
				{
					SENDOVERLAPPED* pSend = (SENDOVERLAPPED*)pOverlapped;
					m_pool.DispatchWorker(pSend->m_worker);
				}
					break;
				case CError:
				{
					ERROROVERLAPPED* pError = (ERROROVERLAPPED*)pOverlapped;
					m_pool.DispatchWorker(pError->m_worker);
				}
					break;
				}
			}
			else
			{
				return -1;
			}
		}
		return 0;
	}
private:
	MyThreadPool m_pool;
	HANDLE m_hIOCP;
	SOCKET m_sock;
	sockaddr_in m_addr;
	std::map<SOCKET, std::shared_ptr<MyClient>> m_client;
};

