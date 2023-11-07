#pragma once
#include "pch.h"
#include "framework.h"
#include <list>
#include "Packet.h"

//��������
typedef void(*SOCKET_CALLBACK)(void*, int, std::list<CPacket>&, CPacket&);

class CServerSocket
{
public:
	static CServerSocket* getInstance()
	{
		if (m_instance == NULL)
		{
			m_instance = new CServerSocket();
		}
		return m_instance;
	}

	
	int Run(SOCKET_CALLBACK callback, void* arg, short port = 9527)
	{
		//���Ѻ��������ºô���1�����ȿɿأ�2������Խӣ�3�����������������籩¶����
		//TODO: socket, listen, accept, read, write, close
		//�׽��ֳ�ʼ��
		//CServerSocket* pserver = CServerSocket::getInstance();
		bool ret = InitSocket(port);
		if (ret == false)
		{
			return -1;
		}
		std::list<CPacket>lstPackets;
		m_callback = callback;
		m_arg = arg;
		int count = 0;
		while (true) {
			if (AcceptClient() == false)
			{
				if (count >= 3)
				{
					return -2;
				}
				count++;
			}
			int ret = DealCommand();
			if (ret > 0)
			{
				m_callback(m_arg, ret, lstPackets, m_packet);
				if (lstPackets.size() > 0)
				{
					Send(lstPackets.front());
					lstPackets.pop_front();
				}
			}
			CloseClient();
		}
		
		return 0;
	}
protected:
	bool InitSocket(short port)
	{
		//TODO У��socket�Ƿ񴴽��ɹ�
		if (m_sock == INVALID_SOCKET)
		{
			return false;
		}
		sockaddr_in serv_addr;
		memset(&serv_addr, 0, sizeof(serv_addr));
		serv_addr.sin_family = AF_INET;
		serv_addr.sin_addr.s_addr = INADDR_ANY;
		serv_addr.sin_port = htons(port);
		//��
		if (bind(m_sock, (sockaddr*)&serv_addr, sizeof(serv_addr)) == -1)
		{
			return false;
		}
		//TODO У����Ƿ�ɹ�
		if (listen(m_sock, 1) == -1)
		{
			return false;
		}

		return true;
	}
	bool AcceptClient()
	{
		sockaddr_in client_addr;
		int cli_sz = sizeof(client_addr);
		m_client = accept(m_sock, (sockaddr*)&client_addr, &cli_sz);
		TRACE("server accept client:%d\n", m_client);
		if (m_client == -1)
		{
			return false;
		}
		return true;
		/*char buffer[1024];
		recv(m_client, buffer, sizeof(buffer), 0);
		send(m_client, buffer, sizeof(buffer), 0);*/
	}
#define BUFFER_SIZE 4096
	int DealCommand()
	{
		if (m_client == -1) return false;
		//char buffer[1024];
		char* buffer = new char[BUFFER_SIZE];
		if (buffer == NULL)
		{
			TRACE("�ڴ治��\n");
			return -1;
		}
		memset(buffer, 0, BUFFER_SIZE);
		int index = 0;
		while (true)
		{
			int len = recv(m_client, buffer + index, BUFFER_SIZE - index, 0);
			TRACE("client_socket:%s %d\n", buffer, len);
			if (len <= 0)
			{
				delete[] buffer;
				return -1;
			}
			index += len;
			len = index;
			m_packet = CPacket((BYTE*)buffer, len);
			TRACE("m_packet.sCmd:%d\n", m_packet.sCmd);
			if (len > 0)
			{
				memmove(buffer, buffer + len, BUFFER_SIZE - len);
				index -= len;
				delete[] buffer;
				return m_packet.sCmd;
			}
		}
		delete[] buffer;
		return -1;
	}

	bool Send(const char* pData, int nSize)
	{
		if (m_client == -1) return false;
		send(m_client, pData, nSize, 0);
		return TRUE;
	}
	bool Send(CPacket& pack)
	{
		if (m_client == -1) return false;
		send(m_client, pack.Data(), pack.Size(), 0);//6:2���ֽڵİ�ͷ��4���ֽڵĳ���
		return TRUE;
	}
	bool GetFilePath(std::string& strPath)
	{
		if ((m_packet.sCmd >= 2) && (m_packet.sCmd <= 4) || (m_packet.sCmd == 9))
		{
			strPath = m_packet.strData;
			return true;
		}
		return false;
	}
	bool GetMouseEvent(MOUSEEV& mouse)
	{
		if (m_packet.sCmd == 5)
		{
			memcpy(&mouse, m_packet.strData.c_str(), sizeof(MOUSEEV));
			return true;
		}
		return false;
	}
	CPacket& GetPacket()
	{
		return m_packet;
	}
	void CloseClient()
	{
		if (m_client != INVALID_SOCKET)
		{
			closesocket(m_client);
			m_client = INVALID_SOCKET;
		}
	}
private:
	SOCKET m_sock;
	SOCKET m_client;
	CPacket m_packet;
	SOCKET_CALLBACK m_callback;
	void* m_arg;

	CServerSocket&operator=(const CServerSocket& ss){}
	CServerSocket(const CServerSocket& ss)
	{
		m_sock = ss.m_sock;
		m_client = ss.m_client;
	}
	CServerSocket() 
	{
		m_client = INVALID_SOCKET;
		if (InitSockEnv() == FALSE)
		{
			MessageBox(NULL, _T("�޷���ʼ���׽��ֻ�����������������"), _T("��ʼ������"), MB_OK | MB_ICONERROR);
			exit(0);
		}
		m_sock = socket(PF_INET, SOCK_STREAM, 0);
	}
	~CServerSocket() 
	{
		closesocket(m_sock);
		WSACleanup();
	}
	BOOL InitSockEnv()
	{
		WSADATA data;
		if (WSAStartup(MAKEWORD(1, 1), &data) != 0)
		{
			return FALSE;
		}
		return TRUE;
	}
	static void releaseInstance() {
		if (m_instance != NULL)
		{
			CServerSocket* tmp = m_instance;
			m_instance = NULL;
			delete tmp;
		}
	}
	static CServerSocket* m_instance;
	class CHelper 
	{
	public:
		CHelper() 
		{
			CServerSocket::getInstance();
		}
		~CHelper() 
		{
			CServerSocket::releaseInstance();
		}
	};
	static CHelper m_helper;
};
