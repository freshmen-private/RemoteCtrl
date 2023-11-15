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
	MyClient* m_client;//对应的客户端
	WSABUF m_wsabuffer;
	virtual ~COverlapped()
	{
		m_buffer.clear();
	}
};
template<COperator> class AcceptOverlapped;
typedef AcceptOverlapped<CAccept> ACCEPTOVERLAPPED;
template<COperator> class RecvOverlapped;
typedef RecvOverlapped<CRecv> RECVOVERLAPPED;
template<COperator> class SendOverlapped;
typedef SendOverlapped<CSend> SENDOVERLAPPED;

class MyClient: public ThreadFuncBase
{
public:
	MyClient();

	~MyClient()
	{
		m_buffer.clear();
		closesocket(m_sock);
		m_recv.reset();
		m_send.reset();
		m_overlapped.reset();
		m_vecSend.Clear();
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
	LPWSAOVERLAPPED RecvOverlapped();
	LPWSABUF SendWSABuf();
	LPWSAOVERLAPPED SendOverlapped();
	DWORD& flags() { return m_flags; }
	sockaddr_in* GetLocalAddr() { return &m_laddr; }
	sockaddr_in* GetRemoteAddr() { return &m_raddr; }
	size_t GetBufferSize()const { return m_buffer.size(); }
	int Recv();
	int Send(void* buffer, size_t nSize);
	int SendData(std::vector<char>& data);
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
	MySendQueue<std::vector<char>> m_vecSend;//发送队列
};

template<COperator>
class AcceptOverlapped :public COverlapped, ThreadFuncBase
{
public:
	AcceptOverlapped();
	int AcceptWorker();
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
	~CMyServer();

	bool StartService();
	bool NewAccept();
	void BindNewSocket(SOCKET s);

private:
	void CreateSocket();
	int threadIocp();
private:
	MyThreadPool m_pool;
	HANDLE m_hIOCP;
	SOCKET m_sock;
	sockaddr_in m_addr;
	std::map<SOCKET, std::shared_ptr<MyClient>> m_client;
};
