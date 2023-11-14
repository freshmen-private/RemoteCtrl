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

MyClient::MyClient() : m_isbusy(false), m_overlapped(new ACCEPTOVERLAPPED())
{
	m_sock = WSASocket(PF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	m_buffer.resize(1024);
	memset(&m_laddr, 0, sizeof(m_laddr));
	memset(&m_raddr, 0, sizeof(m_raddr));
}
void MyClient::SetOverlapped(PCLIENT& ptr)
{
	m_overlapped->m_client = ptr;
}
MyClient::operator LPOVERLAPPED()
{
	return &m_overlapped->m_overlapped;
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
		if (!m_server->NewAccept())
		{
			return -2;
		}
	}
	return -1;
}
