#pragma once
#include "pch.h"
#include "framework.h"
#include <string>
#include <vector>
#include <list>
#include <map>
#include <mutex>

#define WM_SEND_PACK    (WM_USER + 1)//发送包数据
#define WM_SEND_PACK_ACK (WM_USER + 2)//发送包数据应答

#pragma pack(push)
#pragma pack(1)
class CPacket
{
public:
	CPacket() :sHead(0), nLength(0), sCmd(0), sSum(0) {}
	//打包的重构
	CPacket(WORD nCmd, const BYTE* pData, size_t nSize)
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
	}
	//解包的重构
	CPacket(const CPacket& pack)
	{
		sHead = pack.sHead;
		nLength = pack.nLength;
		sCmd = pack.sCmd;
		strData = pack.strData;
		sSum = pack.sSum;
	}
	CPacket(const BYTE* pData, int& nSize) :sHead(0), nLength(0), sCmd(0), sSum(0)
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

enum {
	CSM_AUTOCLOSE = 1,//CSM = Client Socket 自动关闭模式;
};

typedef struct PacketData{
	std::string strData;
	UINT nMode;
	WPARAM wParam;
	PacketData(BYTE* pData, int nLen, UINT mode, WPARAM nParam = 0)
	{
		strData.resize(nLen);
		memcpy((char*)strData.c_str(), pData, nLen);
		nMode = mode;
		wParam = nParam;
	}
	PacketData(const PacketData& data)
	{
		strData = data.strData;
		nMode = data.nMode;
		wParam = data.wParam;
	}
	PacketData& operator=(const PacketData& data)
	{
		if (this != &data)
		{
			strData = data.strData;
			nMode = data.nMode;
			wParam = data.wParam;
		}
		return *this;
	}
}PACKET_DATA;
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
	bool InitSocket();
	

#define BUFFER_SIZE 4096000
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

	bool SendPacket(HWND hWnd, const CPacket& pack, bool isAutoClosed = TRUE, WPARAM nParam = 0);
	//bool SendPacket(const CPacket& pack, std::list<CPacket>& lstPacks, bool isAutoClosed = TRUE);
	
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
		if(m_nIP != nIP || m_nPort != nPort)
		{
			m_nIP = nIP;
			m_nPort = nPort;
		}
	}

private:
	UINT m_nThreadID;
	typedef void(CClientSocket::* MSGFUNC)(UINT nMsg, WPARAM wParam, LPARAM lParam);
	std::map<UINT, MSGFUNC> m_mapFunc;
	HANDLE m_hThread;
	std::mutex m_lock;
	bool m_bAutoClose;
	std::list<CPacket>m_listSend;
	std::map<HANDLE, std::list<CPacket>&> m_mapAck;
	std::map<HANDLE, bool> m_mapAutoClosed;
	int m_nIP;//地址
	int m_nPort;//端口
	SOCKET m_sock;
	CPacket m_packet;
	std::vector<char> m_buffer;
	CClientSocket& operator=(const CClientSocket& ss) {}
	CClientSocket(const CClientSocket& ss);
	
	CClientSocket();
	~CClientSocket()
	{
		closesocket(m_sock);
		m_sock = INVALID_SOCKET;
		WSACleanup();
	}
	static unsigned __stdcall threadEntry(void* arg);
	void threadFunc2();
	//void threadFunc();
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
	bool Send(const char* pData, int nSize)
	{
		if (m_sock == -1) return false;
		return send(m_sock, pData, nSize, 0);
	}
	bool Send(const CPacket& pack);
	void SendPack(UINT nMsg, WPARAM wParam, LPARAM lParam);
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

