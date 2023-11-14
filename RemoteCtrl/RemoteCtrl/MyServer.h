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
	PCLIENT m_client;//对应的客户端
	WSABUF m_wsabuffer;
};
template<COperator> class AcceptOverlapped;
typedef AcceptOverlapped<CAccept> ACCEPTOVERLAPPED;
template<COperator> class RecvOverlapped;
typedef RecvOverlapped<CRecv> RECVOVERLAPPED;
template<COperator> class SendOverlapped;
typedef SendOverlapped<CSend> SENDOVERLAPPED;

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
	LPWSABUF RecvWSABuf();
	LPWSABUF SendWSABuf();
	DWORD& flags() { return m_flags; }
	sockaddr_in* GetLocalAddr() { return &m_laddr; }
	sockaddr_in* GetRemoteAddr() { return &m_raddr; }
	size_t GetBufferSize()const { return m_buffer.size(); }
	int Recv()
	{
		int ret = recv(m_sock, m_buffer.data() + m_used, (int)(m_buffer.size() - m_used), 0);
		if (ret <= 0)
		{
			return -1;
		}
		m_used += (size_t)ret;
		//TODO 解析数据还没完成
		return 0;
	}
private:
	SOCKET m_sock;
	DWORD m_received;
	DWORD m_flags;
	std::shared_ptr<ACCEPTOVERLAPPED> m_overlapped;
	std::shared_ptr<RECVOVERLAPPED> m_recv;
	std::shared_ptr<SENDOVERLAPPED> m_send;
	std::vector<char> m_buffer;
	size_t m_used; //已经使用的缓冲区大小
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
	RecvOverlapped();
	int RecvWorker()
	{
		int ret = m_client->Recv();
		return ret;
	}
};

template<COperator>
class SendOverlapped :public COverlapped, ThreadFuncBase
{
public:
	SendOverlapped();
	int SendWorker()
	{
		//TODO
		return -1;
	}
};

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
		return -1;
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

	bool StartService();
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
	
	int threadIocp();
private:
	MyThreadPool m_pool;
	HANDLE m_hIOCP;
	SOCKET m_sock;
	sockaddr_in m_addr;
	std::map<SOCKET, std::shared_ptr<MyClient>> m_client;
};
