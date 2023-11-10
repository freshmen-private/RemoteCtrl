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

bool CClientSocket::InitSocket()
{
	//TODO 校验socket是否创建成功
	if (m_sock != INVALID_SOCKET)
	{
		CloseSocket();
	}
	m_sock = socket(PF_INET, SOCK_STREAM, 0);
	if (m_sock == -1)
	{
		return false;
	}
	sockaddr_in serv_addr;
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(m_nIP);
	serv_addr.sin_port = htons(m_nPort);
	if (serv_addr.sin_addr.s_addr == INADDR_NONE)
	{
		AfxMessageBox("指定的IP地址不存在\n");
		return false;
	}
	//连接
	int ret = connect(m_sock, (sockaddr*)&serv_addr, sizeof(serv_addr));
	if (ret == -1)
	{
		AfxMessageBox("连接失败\n");
		TRACE("连接失败， %d %s", WSAGetLastError(), GetErrInfo(WSAGetLastError()).c_str());
		return false;
	}

	return true;
}

bool CClientSocket::SendPacket(const CPacket& pack, std::list<CPacket>& lstPacks, bool isAutoClosed)
{
	if (m_sock == INVALID_SOCKET && m_hThread == INVALID_HANDLE_VALUE)
	{
		//if (InitSocket() == false) return false;
		m_hThread = (HANDLE)_beginthread(&CClientSocket::threadEntry, 0, this);
	}
	m_lock.lock();
	auto pr = m_mapAck.insert(std::pair<HANDLE, std::list<CPacket>&>(pack.hEvent, lstPacks));
	m_mapAutoClosed.insert(std::pair<HANDLE, bool>(pack.hEvent, isAutoClosed));
	//m_lock.lock();
	m_listSend.push_back(pack);
	m_lock.unlock();
	WaitForSingleObject(pack.hEvent, INFINITE);
	std::map<HANDLE, std::list<CPacket>&>::iterator it;
	it = m_mapAck.find(pack.hEvent);
	if (it != m_mapAck.end())
	{
		m_lock.lock();
		m_mapAck.erase(it);
		m_lock.unlock();
		return true;
	}
	return false;
}

void CClientSocket::threadEntry(void* arg)
{
	CClientSocket* thiz = (CClientSocket*)arg;
	thiz->threadFunc();
}

void CClientSocket::threadFunc2()
{
	MSG msg;
	while (::GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
		if (m_mapFunc.find(msg.message) != m_mapFunc.end())
		{
			(this->*m_mapFunc[msg.message])(msg.message, msg.wParam, msg.lParam);
		}
	}
}

void CClientSocket::threadFunc()
{
	std::string strBuffer;
	strBuffer.resize(BUFFER_SIZE);
	char* pBuffer = (char*)strBuffer.c_str();
	int index = 0;
	InitSocket();
	while (m_sock != INVALID_SOCKET)
	{
		if (m_listSend.size() > 0)
		{
			TRACE("size of m_listSend = %d\n", m_listSend.size());
			m_lock.lock();
  			CPacket& head = m_listSend.front();
			m_lock.unlock();
			if (Send(head) == false)
			{
				TRACE("发送失败");
				continue;
			}
			std::map<HANDLE, std::list<CPacket>&>::iterator it;
			it = m_mapAck.find(head.hEvent);
			if (it != m_mapAck.end())
			{
				std::map<HANDLE, bool>::iterator it0 = m_mapAutoClosed.find(head.hEvent);
				do {
					int length = recv(m_sock, pBuffer + index, BUFFER_SIZE - index, 0);
					if (length > 0 || index > 0)
					{
						index += length;
						int size = index;
						CPacket pack((BYTE*)pBuffer, size);
						if (size > 0)
						{//TODO 对于文件夹信息获取，文件信息获取可能产生问题
							pack.hEvent = head.hEvent;
							it->second.push_back(pack);
							memmove(pBuffer, pBuffer + size, index - size);
							index -= size;
							if (it0->second)
							{
								SetEvent(head.hEvent);
								break;
							}
						}
						continue;
					}
					else if (length <= 0 && index <= 0)
					{
						CloseSocket();
						SetEvent(head.hEvent);//等到服务器关闭命令之后，再通知事情完成
						if (it0 != m_mapAutoClosed.end())
						{
							
							TRACE("set event %d %d\n", head.sCmd, it0->second);
						}
						else
						{
							TRACE("异常情况，没有对应的pair\n");
						}
						break;
					}
				} while (it0->second == false);
			}
			m_lock.lock();
			m_listSend.pop_front();
			m_mapAutoClosed.erase(head.hEvent);
			m_lock.unlock();
			InitSocket();
		}
		else
		{
			Sleep(1);
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

void CClientSocket::SendPack(UINT nMsg, WPARAM wParam, LPARAM lParam)
{//定义一个消息的数据结构（数据和数据长度），一个回调消息的数据结构（HWND, MESSAGE）
	if(InitSocket() == true)
	{
		int ret = send(m_sock, (char*)wParam, (int)lParam, 0);
		if (ret > 0)
		{

		}
		else
		{
			CloseSocket();
		}
	}
	else
	{

	}
}
