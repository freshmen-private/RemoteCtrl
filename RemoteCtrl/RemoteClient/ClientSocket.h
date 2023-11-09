#pragma once
#include "pch.h"
#include "framework.h"
#include <string>
#include <vector>
#include <list>
#include <map>

#pragma pack(push)
#pragma pack(1)
class CPacket
{
public:
	CPacket() :sHead(0), nLength(0), sCmd(0), sSum(0) {}
	//打包的重构
	CPacket(WORD nCmd, const BYTE* pData, size_t nSize, HANDLE hEvent)
	{
		sHead = 0xFEFF;
		nLength = nSize + 2 * sizeof(WORD);
		sCmd = nCmd;
		if (nSize > 0)
		{
			strData.resize(nSize);
			memcpy((void*)strData.c_str(), pData, nSize);
		}
		else
		{
			strData.clear();
		}
		sSum = 0;
		for (size_t j = 0; j < strData.size(); j++)
		{
			sSum += BYTE(strData[j]) & 0xFF;
		}
		this->hEvent = hEvent;
	}
	//解包的重构
	CPacket(const CPacket& pack)
	{
		sHead = pack.sHead;
		nLength = pack.nLength;
		sCmd = pack.sCmd;
		strData = pack.strData;
		sSum = pack.sSum;
		hEvent = pack.hEvent;
	}
	CPacket(const BYTE* pData, int& nSize) :sHead(0), nLength(0), sCmd(0), sSum(0), hEvent(INVALID_HANDLE_VALUE)
	{
		int i = 0;
		for (; i < nSize; i++)
		{
			if (*(WORD*)(pData + i) == 0xFEFF)
			{
				sHead = *(WORD*)(pData + i);
				i += sizeof(WORD);
				break;
			}
		}
		nLength = *(DWORD*)(pData + i);
		i += sizeof(DWORD);
		if (nLength + i > nSize)
		{
			nSize = 0;
			return;
		}
		sCmd = *(WORD*)(pData + i);
		i += sizeof(WORD);
		if (nLength > sizeof(WORD) + sizeof(WORD))
		{
			strData.resize(nLength - 2 * sizeof(WORD));
			memcpy((void*)strData.c_str(), pData + i, nLength - 2 * sizeof(WORD));
			TRACE("%s\n", strData.c_str() + 12);
		}
		i += nLength - 2 * sizeof(WORD);
		sSum = *(WORD*)(pData + i);
		i += sizeof(WORD);
		WORD sum = 0;
		for (int j = 0; j < strData.size(); j++)
		{
			sum += BYTE(strData[j]) & 0xFF;
		}
		if (sum == sSum)
		{
			nSize = i;
			return;
		}
		nSize = 0;
	}
	~CPacket() {}
	CPacket operator=(const CPacket& pack)
	{
		if (this != &pack)
		{
			sHead = pack.sHead;
			nLength = pack.nLength;
			sCmd = pack.sCmd;
			strData = pack.strData;
			sSum = pack.sSum;
			hEvent = pack.hEvent;
		}
		return *this;
	}
	int Size()
	{
		return nLength + sizeof(DWORD) + sizeof(WORD);
	}
	const char* Data(std::string& strOut) const
	{
		strOut.resize(nLength + sizeof(DWORD) + sizeof(WORD));
		BYTE* pData = (BYTE*)strOut.c_str();
		*(WORD*)pData = sHead; pData += sizeof(WORD);
		*(DWORD*)pData = nLength; pData += sizeof(DWORD);
		*(WORD*)pData = sCmd; pData += sizeof(WORD);
		memcpy(pData, strData.c_str(), strData.size()); pData += strData.size();
		*(WORD*)pData = sSum;

		return strOut.c_str();
	}
public:
	WORD sHead;//固定位 0xFEFF
	DWORD nLength;//包长度，从控制命令到和校验结束
	WORD sCmd;//控制命令
	std::string strData;//包数据
	WORD sSum;//和校验
	HANDLE hEvent;
};
#pragma pack(pop)
//单例对象

typedef struct MouseEvent
{
	MouseEvent()
	{
		nAction = 0;
		nButton = -1;
		ptXY.x = 0;
		ptXY.y = 0;
	}
	WORD nAction;//动作
	WORD nButton;//按键
	POINT ptXY;//坐标
}MOUSEEV, * PMOUSEEV;

typedef struct file_info {
	file_info() {
		IsInvalid = FALSE;
		IsDirectory = FALSE;
		HasNext = TRUE;
		memset(szFileName, 0, sizeof(szFileName));
	}
	BOOL IsInvalid;//是否有效
	BOOL IsDirectory;//是否是目录
	BOOL HasNext;
	char szFileName[256];//文件名
}FILEINFO, * PFILEINFO;

std::string GetErrInfo(int wsaErrCode);

class CClientSocket
{
public:
	static CClientSocket* getInstance()
	{
		if (m_instance == NULL)
		{
			m_instance = new CClientSocket();
		}
		return m_instance;
	}
	bool InitSocket()
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
		if ( ret == -1 )
		{
			AfxMessageBox("连接失败\n");
			TRACE("连接失败， %d %s", WSAGetLastError(), GetErrInfo(WSAGetLastError()).c_str());
			return false;
		}

		return true;
	}

#define BUFFER_SIZE 409600
	int DealCommand()
	{
		if (m_sock == -1) return false;
		char* buffer = m_buffer.data();
		static int index = 0;
		static int count = 0;
		while (true)
		{
			int len = recv(m_sock, buffer + index, BUFFER_SIZE - index, 0);
			if (len <= 0 && index <= 0)
			{
				return -1;
			}
			count++;
			TRACE("%d\n", count);
			index += len;
			len = index;
			m_packet = CPacket((BYTE*)buffer, len);
			if (len > 0)
			{
				memmove(buffer, buffer + len, index - len);
				index -= len;
			}
			return m_packet.sCmd;
		}
		return -1;
	}

	bool Send(const char* pData, int nSize)
	{
		if (m_sock == -1) return false;
		send(m_sock, pData, nSize, 0);
		return TRUE;
	}
	bool Send(const CPacket& pack)
	{
		if (m_sock == -1) return false;
		std::string strOut;
		pack.Data(strOut);
		send(m_sock, strOut.c_str(), strOut.size(), 0);//6:2个字节的包头，4个字节的长度
		return TRUE;
	}
	bool GetFilePath(std::string& strPath)
	{
		if ((m_packet.sCmd >= 2) && (m_packet.sCmd <= 4))
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

	void CloseSocket()
	{
		closesocket(m_sock);
		m_sock = INVALID_SOCKET;
	}

	void UpdateAddress(int nIP, int nPort)
	{
		m_nIP = nIP;
		m_nPort = nPort;
	}

private:
	std::list<CPacket>m_listSend;
	std::map<HANDLE, std::list<CPacket>> m_mapAck;
	int m_nIP;//地址
	int m_nPort;//端口
	SOCKET m_sock;
	CPacket m_packet;
	std::vector<char> m_buffer;
	CClientSocket& operator=(const CClientSocket& ss) {}
	CClientSocket(const CClientSocket& ss)
	{
		m_sock = ss.m_sock;
		m_nIP = ss.m_nIP;
		m_nPort = ss.m_nPort;
	}
	CClientSocket() :
		m_nIP(INADDR_ANY),
		m_nPort(0)
	{
		if (InitSockEnv() == FALSE)
		{
			MessageBox(NULL, _T("无法初始化套接字环境，请检查网络设置"), _T("初始化错误"), MB_OK | MB_ICONERROR);
			exit(0);
		}
		m_buffer.resize(BUFFER_SIZE);
		memset(m_buffer.data(), 0, BUFFER_SIZE);
	}
	~CClientSocket()
	{
		closesocket(m_sock);
		m_sock = INVALID_SOCKET;
		WSACleanup();
	}
	static void threadEntry(void* arg);
	void threadFunc();
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
			CClientSocket* tmp = m_instance;
			m_instance = NULL;
			delete tmp;
			TRACE("delete cclientsocket\n");
		}
	}
	static CClientSocket* m_instance;
	class CHelper
	{
	public:
		CHelper()
		{
			CClientSocket::getInstance();
		}
		~CHelper()
		{
			CClientSocket::releaseInstance();
		}
	};
	static CHelper m_helper;
};

