#include "pch.h"
#include "MyServer.h"
#pragma warning(disable:4407)
template<COperator op>
AcceptOverlapped<op>::AcceptOverlapped()
{
	m_worker = ThreadWorker(this, (FUNCTYPE)&AcceptOverlapped<op>::AcceptWorker);
	m_operator = CAccept;
	memset(&m_overlapped, 0, sizeof(m_overlapped));
	m_buffer.resize(1024);
	m_server = NULL;
}

MyClient::MyClient() : 
	m_isbusy(false), m_flags(0),
	m_overlapped(new ACCEPTOVERLAPPED()),
	m_recv(new RECVOVERLAPPED()),
	m_send(new SENDOVERLAPPED())
{
	m_sock = WSASocket(PF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	m_buffer.resize(1024);
	memset(&m_laddr, 0, sizeof(m_laddr));
	memset(&m_raddr, 0, sizeof(m_raddr));
}
void MyClient::SetOverlapped(PCLIENT& ptr)
{
	m_overlapped->m_client = ptr;
	m_recv->m_client = ptr;
	m_send->m_client = ptr;
}
MyClient::operator LPOVERLAPPED()
{
	return &m_overlapped->m_overlapped;
}

LPWSABUF MyClient::RecvWSABuf()
{
	return &m_recv->m_wsabuffer;
}

LPWSABUF MyClient::SendWSABuf()
{
	return &m_send->m_wsabuffer;
}

template<COperator op>
int AcceptOverlapped<op>::AcceptWorker()
{
	int lLength = 0, rLength = 0;
	if (*(LPDWORD)*m_client.get() > 0)
	{
		GetAcceptExSockaddrs(*m_client, 0,
			sizeof(sockaddr_in) + 16, sizeof(sockaddr_in) + 16,
			(sockaddr**)m_client->GetLocalAddr(), &lLength,//本地地址
			(sockaddr**)m_client->GetRemoteAddr(), &rLength);//远端地址
		int ret = WSARecv((SOCKET)*m_client, m_client->RecvWSABuf(), 1, *m_client, &m_client->flags(), *m_client, NULL);
		if (ret == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING)
		{

		}
		if (!m_server->NewAccept())
		{
			return -2;
		}
	}
	return -1;
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

int CMyServer::threadIocp()
{
	DWORD transferred = 0;
	ULONG_PTR CompletionKey = 0;
	OVERLAPPED* lpOverlapped = NULL;
	if (GetQueuedCompletionStatus(m_hIOCP, &transferred, &CompletionKey, &lpOverlapped, INFINITE))
	{
		if (transferred > 0 && CompletionKey != 0)
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

template<COperator op>
inline SendOverlapped<op>::SendOverlapped()
{
	m_operator = op;
	m_worker = ThreadWorker(this, (FUNCTYPE)&SendOverlapped<op>::SendWorker);
	memset(&m_overlapped, 0, sizeof(m_overlapped));
	m_buffer.resize(1024 * 256);
}
template<COperator op>
inline RecvOverlapped<op>::RecvOverlapped()
{
	m_operator = op;
	m_worker = ThreadWorker(this, (FUNCTYPE)&RecvOverlapped<op>::RecvWorker);
	memset(&m_overlapped, 0, sizeof(m_overlapped));
	m_buffer.resize(1024 * 256);
}
