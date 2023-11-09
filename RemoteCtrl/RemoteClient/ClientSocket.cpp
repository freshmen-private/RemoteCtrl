//#pragma once
#include "pch.h"
#include "ClientSocket.h"

std::string GetErrInfo(int wsaErrCode)
{
	std::string ret;
	LPVOID lpMsgBuf = NULL;
	FormatMessage(
		FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER,
		NULL,
		wsaErrCode,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL
	);
	ret = (char*)lpMsgBuf;
	LocalFree(lpMsgBuf);
	return ret;
}

CClientSocket* CClientSocket::m_instance = NULL;
CClientSocket::CHelper CClientSocket::m_helper;
CClientSocket* pclient = CClientSocket::getInstance();

void CClientSocket::threadEntry(void* arg)
{
	CClientSocket* thiz = (CClientSocket*)arg;
	thiz->threadFunc();
}

void CClientSocket::threadFunc()
{
	std::string strBuffer;
	strBuffer.resize(BUFFER_SIZE);
	char* pBuffer = (char*)strBuffer.c_str();
	int index = 0;
	while (m_sock != INVALID_SOCKET)
	{
		if (m_listSend.size() > 0)
		{
			TRACE("size of m_listSend = %d\n", m_listSend.size());
			CPacket& head = m_listSend.front();
			if (Send(head) == false)
			{
				TRACE("����ʧ��");
				continue;
			}
			auto pr = m_mapAck.insert(std::pair<HANDLE, std::list<CPacket>>(head.hEvent, std::list<CPacket>()));
			std::list<CPacket>lstRecv;
			int length = recv(m_sock, pBuffer, BUFFER_SIZE - index, 0);
			if (length > 0 || index > 0)
			{
				index += length;
				int size = index;
				CPacket pack((BYTE*)pBuffer, size);
				if (size > 0)
				{
					//TODO �����ļ�����Ϣ��ȡ���ļ���Ϣ��ȡ���ܲ�������
					pack.hEvent = head.hEvent;
					pr.first->second.push_back(pack);
					SetEvent(head.hEvent);
				}
				continue;
			}
			else if (length <= 0 && index <= 0)
			{
				CloseSocket();
			}
			m_listSend.pop_front();
		}
	}
	CloseSocket();
}

bool CClientSocket::Send(const CPacket& pack)
{
	if (m_sock == -1) return false;
	std::string strOut;
	pack.Data(strOut);
	return send(m_sock, strOut.c_str(), strOut.size(), 0);
}
