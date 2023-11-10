#pragma once
#include "ClientSocket.h"
#include "WatchDialog.h"
#include "StatusDlg.h"
#include "RemoteClientDlg.h"
#include <map>
#include "resource.h"
#include "MyTool.h"

//#define WM_SEND_PACK    (WM_USER + 1)//发送包数据
//#define WM_SEND_DATA    (WM_USER + 2)//发送数据
#define WM_SHOW_STATUS  (WM_USER + 3)//展示状态
#define WM_SHOW_WATCH   (WM_USER + 4)//远程监控
#define WM_SEND_MESSAGE (WM_USER + 0x1000)//自定义消息处理

class CClientController
{
public:
	//获取全局唯一对象   单例
	static CClientController* getInstance();
	//初始化操作
	int InitController();
	//启动
	int Invoke(CWnd*& pMainWnd);
	//发送消息
	LRESULT SendMessage(MSG msg);
	//更新网络服务器的地址
	void UpdateAddress(int nIP, int nPort)
	{
		CClientSocket::getInstance()->UpdateAddress(nIP, nPort);
	}
	int DealCommand()
	{
		return CClientSocket::getInstance()->DealCommand();
	}
	void CloseSocket()
	{
		return CClientSocket::getInstance()->CloseSocket();
	}
	//1 查看磁盘分区
	//2 查看指定目录下的文件
	//3 打开文件
	//4 下载文件
	//5 鼠标操作
	//6 发送屏幕内容
	//7 锁机
	//8 解锁
	//9 删除文件
	//1981 测试连接
	//返回值 状态，true是成功，false失败
	bool SendCommandPacket(
		HWND hWnd,//数据包收到后需要应答的窗口
		int nCmd,
		bool bAutoClose = true,
		BYTE* pData = NULL,
		size_t nLength = 0,
		WPARAM wParam = 0);
	
	int GetImage(CImage& image)
	{
		CClientSocket* pClient = CClientSocket::getInstance();
		return CMyTool::Bytes2Image(image, pClient->GetPacket().strData);

	}

	void DownloadEnd();

	int DownFile(CString strPath);

	void StartWatchScreen();
protected:
	void threadWatchScreen();
	static void threadWatchEntry(void* arg);
	void threadDownLoadFile();
	static void threadDownloadEntry(void* arg);
	CClientController():
		m_statusDlg(&m_remoteDlg), 
		m_watchDlg(&m_remoteDlg) 
	{
		m_isClosed = true;//监视是否关闭
		m_hThreadWatch = INVALID_HANDLE_VALUE;
		m_hThreadDownload = INVALID_HANDLE_VALUE;
		m_hThread = INVALID_HANDLE_VALUE;
		m_nThreadID = -1;
	}
	~CClientController() 
	{
		WaitForSingleObject(m_hThread, 100);
	}
	void threadFunc();
	static unsigned __stdcall threadEntry(void* arg);
	static void releaseInstance()
	{
		if (m_instance != NULL)
		{
			delete m_instance;
			m_instance = NULL;
			TRACE("delete CClientController\n");
		}
	}
	//LRESULT OnSendPack(UINT nMsg, WPARAM wParam, LPARAM lParam);
	//LRESULT OnSendData(UINT nMsg, WPARAM wParam, LPARAM lParam);
	LRESULT OnShowStatus(UINT nMsg, WPARAM wParam, LPARAM lParam);
	LRESULT OnShowWatch(UINT nMsg, WPARAM wParam, LPARAM lParam);
private:
	typedef struct MsgInfo
	{
		MSG msg;
		LRESULT result;
		MsgInfo(MSG m) 
		{
			result = 0;
			memcpy(&msg, &m, sizeof(MSG));
		}
		MsgInfo(const MsgInfo& m)
		{
			result = m.result;
			memcpy(&msg, &m, sizeof(MSG));
		}
		MsgInfo& operator=(const MsgInfo& m)
		{
			if (this != &m)
			{
				result = m.result;
				memcpy(&msg, &m, sizeof(MSG));
			}
			return *this;
		}
	}MSGINFO;
	typedef LRESULT(CClientController::* MSGFUNC)(UINT nMSG, WPARAM wParam, LPARAM lParam);
	static std::map<UINT, MSGFUNC>m_mapFunc;
	CWatchDialog m_watchDlg;//消息报，在对话框关闭之后，可能导致内存泄露
	CRemoteClientDlg m_remoteDlg;
	CStatusDlg m_statusDlg;
	HANDLE m_hThread;
	HANDLE m_hThreadDownload;
	HANDLE m_hThreadWatch;
	bool m_isClosed;//监视是否关闭
	CString m_strRemote;//下载文件的远程路径
	CString m_strLocal;//下载文件的本地路径
	unsigned m_nThreadID;
	static CClientController* m_instance;
	class CHelper
	{
	public:
		CHelper()
		{
			//CClientController::getInstance();
		}
		~CHelper()
		{
			CClientController::releaseInstance();
		}
	};
	static CHelper m_helper;
};

