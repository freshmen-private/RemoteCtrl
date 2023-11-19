#include "pch.h"
#include "MyServer.h"
#include "MyTool.h"
#pragma warning(disable:4407)
template<COperator op>
AcceptOverlapped<op>::AcceptOverlapped()
{
	m_worker = ThreadWorker(this, (FUNCTYPE)&AcceptOverlapped<op>::AcceptWorker);
	m_operator = CAccept;
	memset(&m_overlapped, 0, sizeof(m_overlapped));
	m_buffer.resize(1024);
	m_wsabuffer.buf = &m_buffer[0];
	m_wsabuffer.len = 1024;
	m_server = NULL;
}

MyClient::MyClient() :
	m_isbusy(false), m_flags(0),
	m_overlapped(new ACCEPTOVERLAPPED()),
	m_recv(new RECVOVERLAPPED()),
	m_send(new SENDOVERLAPPED()),
	m_vecSend(this, (SENDCALLBACK)&MyClient::SendData),
	m_used(0)
{
	m_sock = WSASocket(PF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	m_buffer.resize(1024);
	memset(&m_laddr, 0, sizeof(m_laddr));
	memset(&m_raddr, 0, sizeof(m_raddr));
}
void MyClient::SetOverlapped(PCLIENT& ptr)
{
	m_overlapped->m_client = ptr.get();
	m_recv->m_client = ptr.get();
	m_send->m_client = ptr.get();
}
MyClient::operator LPOVERLAPPED()
{
	return &m_overlapped->m_overlapped;
}

LPWSABUF MyClient::RecvWSABuffer()
{
	return &m_recv->m_wsabuffer;
}

LPWSAOVERLAPPED MyClient::RecvOverlapped()
{
	return &m_recv->m_overlapped;
}

LPWSABUF MyClient::SendWSABuf()
{
	return &m_send->m_wsabuffer;
}

LPWSAOVERLAPPED MyClient::SendOverlapped()
{
	return &m_send->m_overlapped;
}

int MyClient::Recv()
{
	int ret = recv(m_sock, m_buffer.data() + m_used, (int)(m_buffer.size() - m_used), 0);
	int err = WSAGetLastError();
	TRACE("%d\n", err);
	if (ret <= 0)
	{
		return -1;
	}
	m_used += (size_t)ret;
	CMyTool::Dump((BYTE*)m_buffer.data(), ret);
	//TODO �������ݻ�û���
	return 0;
}

int MyClient::Send(void* buffer, size_t nSize)
{
	std::vector<char> data(nSize);
	memcpy(data.data(), buffer, nSize);
	if (m_vecSend.PushBack(data))
	{
		return 0;
	}
	return -1;
}

int MyClient::SendData(std::vector<char>& data)
{
	if (m_vecSend.Size() > 0)
	{
		int ret = WSASend(m_sock, SendWSABuf(), 1, &m_received, m_flags, &m_send->m_overlapped, NULL);
		if (ret != 0 && WSAGetLastError() != WSA_IO_PENDING)
		{
			CMyTool::ShowError();
			return -1;
		}
	}
	return 0;
}

template<COperator op>
int AcceptOverlapped<op>::AcceptWorker()
{
	int lLength = 0, rLength = 0;
	if (m_client->GetBufferSize() > 0)
	{
		sockaddr* plocal = NULL, * premote = NULL;
		GetAcceptExSockaddrs(*m_client, 0,
			sizeof(sockaddr_in) + 16, sizeof(sockaddr_in) + 16,
			(sockaddr**)&plocal, &lLength,//���ص�ַ
			(sockaddr**)&premote, &rLength);//Զ�˵�ַ
		memcpy(m_client->GetLocalAddr(), plocal, sizeof(sockaddr_in));
		memcpy(m_client->GetRemoteAddr(), premote, sizeof(sockaddr_in));
		m_server->BindNewSocket(*m_client);
		int recvbytes;
		int ret = WSARecv((SOCKET)*m_client, m_client->RecvWSABuffer(), 1, (LPDWORD)*m_client, &m_client->flags(), m_client->RecvOverlapped(), NULL);
		DWORD ERR = GetLastError();
		if (ret == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING)
		{
			//TRACE("NO MESSAGE\n");
		}
		if (!m_server->NewAccept())
		{
			return -2;
		}
	}
	//TRACE(m_client->RecvWSABuffer()->buf);
	CMyTool::Dump((BYTE*)m_client->RecvWSABuffer()->buf, *(LPDWORD)*m_client);
	return 0;
}

CMyServer::~CMyServer()
{
	closesocket(m_sock);
	std::map<SOCKET, PCLIENT>::iterator it = m_client.begin();
	for (; it != m_client.end(); it++)
	{
		it->second.reset();
	}
	m_client.clear();
	CloseHandle(m_hIOCP);
	m_pool.Stop();
	WSACleanup();
}

bool CMyServer::StartService()
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

bool CMyServer::NewAccept()
{
	PCLIENT pClient(new MyClient());
	pClient->SetOverlapped(pClient);
	m_client.insert(std::pair<SOCKET, PCLIENT>(*pClient, pClient));
	if (!AcceptEx(m_sock, *pClient, *pClient, 0, sizeof(sockaddr_in) + 16, sizeof(sockaddr_in) + 16, *pClient, *pClient))
	{
		if(WSAGetLastError()!=WSA_IO_PENDING)
		{
			closesocket(m_sock);
			m_sock = INVALID_SOCKET;
			m_hIOCP = INVALID_HANDLE_VALUE;
			return false;
		}
	}
	return true;
}

void CMyServer::BindNewSocket(SOCKET s)
{
	CreateIoCompletionPort((HANDLE)s, m_hIOCP, (ULONG_PTR)this, 0);
}

void CMyServer::CreateSocket()
{
	WSADATA WSAData;
	WSAStartup(MAKEWORD(2, 2), &WSAData);
	m_sock = WSASocket(PF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	int opt = 1;
	setsockopt(m_sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
}

int CMyServer::threadIocp()
{
	DWORD transferred = 0;
	ULONG_PTR CompletionKey = 0;
	OVERLAPPED* lpOverlapped = NULL;
	if (GetQueuedCompletionStatus(m_hIOCP, &transferred, &CompletionKey, &lpOverlapped, INFINITE))
	{
		if (CompletionKey != 0)
		{
			COverlapped* pOverlapped = CONTAINING_RECORD(lpOverlapped, COverlapped, m_overlapped);
			pOverlapped->m_server = this;
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

template<COperator op>
inline SendOverlapped<op>::SendOverlapped()
{
	m_operator = op;
	m_worker = ThreadWorker(this, (FUNCTYPE)&SendOverlapped<op>::SendWorker);
	memset(&m_overlapped, 0, sizeof(m_overlapped));
	m_buffer.resize(1024 * 256);
	m_wsabuffer.buf = m_buffer.data();
	m_wsabuffer.len = 1024 * 256;
}
template<COperator op>
inline RecvOverlapped<op>::RecvOverlapped()
{
	m_operator = op;
	m_worker = ThreadWorker(this, (FUNCTYPE)&RecvOverlapped<op>::RecvWorker);
	memset(&m_overlapped, 0, sizeof(m_overlapped));
	m_buffer.resize(1024 * 256);
	m_wsabuffer.buf = m_buffer.data();
	m_wsabuffer.len = 1024 * 256;
}
