#include "pch.h"
#include "ClientController.h"
#include "ClientSocket.h"

std::map<UINT, CClientController::MSGFUNC>CClientController::m_mapFunc;
CClientController* CClientController::m_instance = NULL;

CClientController* CClientController::getInstance()
{
	if (m_instance == NULL)
	{
		m_instance = new CClientController();
		struct { UINT nMsg; MSGFUNC func; }MsgFuncs[] = 
		{ 
			{WM_SEND_PACK, &CClientController::OnSendPack},
			{WM_SEND_DATA, &CClientController::OnSendData},
			{WM_SHOW_STATUS, &CClientController::OnShowStatus},
			{WM_SHOW_WATCH, &CClientController::OnShowWatch},
			{(UINT)-1, NULL}
		};
		for (int i = 0; MsgFuncs[i].func != NULL; i++)
		{
			m_mapFunc.insert(std::pair<UINT, MSGFUNC>(MsgFuncs[i].nMsg, MsgFuncs[i].func));
		}
	}
	return nullptr;
}

int CClientController::InitController()
{
	m_hThread = (HANDLE)_beginthreadex(NULL, 0, &CClientController::threadEntry, this, 0, &m_nThreadID);
	m_statusDlg.Create(IDD_DLG_STATUS, &m_remoteDlg);
	return 0;
}

int CClientController::Invoke(CWnd*& pMainWnd)
{
	pMainWnd = &m_remoteDlg;
	return m_remoteDlg.DoModal();
}

LRESULT CClientController::SendMessage(MSG msg)
{
	HANDLE hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (hEvent == NULL) return -2;
	MSGINFO info(msg);
	PostThreadMessage(m_nThreadID, WM_SEND_MESSAGE, (WPARAM)&info, (LPARAM)&hEvent);
	//MSGINFO& inf = m_mapMassage.find(uuid)->second;
	WaitForSingleObject(hEvent, INFINITE);
	return info.result;
}

void CClientController::StartWatchScreen()
{
	m_isClosed = false;
	CWatchDialog dlg(&m_remoteDlg);
	HANDLE m_hThreadWatch = (HANDLE)_beginthread(&CClientController::threadWatchEntry, 0, this);
	dlg.DoModal();
	m_isClosed = TRUE;
	WaitForSingleObject(m_hThreadWatch, 500);
}

void CClientController::threadWatchScreen()
{
	Sleep(50);
	while (!m_isClosed) {
		if (m_remoteDlg.isFull() == false)
		{
			int ret = SendCommandPacket(6);
			if (ret == 6)
			{
				if (GetImage(m_remoteDlg.GetImage()) == 0)
				{
					m_remoteDlg.SetImageStatus(true);
				}
				else
				{
					TRACE("��ȡͼƬʧ��\n");
				}
			}
		}
		Sleep(1);
	}
}

void CClientController::threadWatchEntry(void* arg)
{
	CClientController* thiz = (CClientController*)arg;
	thiz->threadWatchScreen();
	_endthread();
}

void CClientController::threadDownLoadFile()
{
	FILE* pFile = fopen(m_strLocal, "wb+");
	if (pFile == NULL)
	{
		AfxMessageBox(_T("����û��Ȩ�ޱ�����ļ��������ļ��޷�����\n"));
		m_statusDlg.ShowWindow(SW_HIDE);
		m_remoteDlg.EndWaitCursor();
		return;
	}
	CClientSocket* pClient = CClientSocket::getInstance();
	do 
	{
		int ret = SendCommandPacket(4, false, (BYTE*)(LPCSTR)m_strRemote, m_strRemote.GetLength());
		long long nlength = *(long long*)pClient->GetPacket().strData.c_str();

		if (nlength == 0)
		{
			AfxMessageBox("�ļ�����Ϊ0�������޷���ȡ�ļ�\n");
			return;
		}
		long long nCount = 0;
		while (nCount < nlength)
		{
			pClient->DealCommand();
			if (ret < 0)
			{
				AfxMessageBox("����ʧ��\n");
				TRACE("����ʧ��, ret = %d\n", ret);
				break;
			}
			fwrite(pClient->GetPacket().strData.c_str(), 1, pClient->GetPacket().strData.size(), pFile);
			nCount += pClient->GetPacket().strData.size();
		}
	} while (false);
	
	fclose(pFile);
	pClient->CloseSocket();
	m_statusDlg.ShowWindow(SW_SHOW);
	m_remoteDlg.EndWaitCursor();
	m_remoteDlg.MessageBox(_T("�������\n"), _T("���"));
}

void CClientController::threadDownloadEntry(void* arg)
{
	CClientController* thiz = (CClientController*)arg;
	thiz->threadDownLoadFile();
	_endthread();
}

void CClientController::threadFunc()
{
	MSG msg;
	while (::GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
		if (msg.message == WM_SEND_MESSAGE)
		{
			MSGINFO* pmsg = (MSGINFO*)msg.wParam;
			HANDLE hEvent = (HANDLE)msg.lParam;
			std::map<UINT, MSGFUNC>::iterator it = m_mapFunc.find(pmsg->msg.message);
			if (it != m_mapFunc.end())
			{
				pmsg->result = (this->*it->second)(pmsg->msg.message, pmsg->msg.wParam, pmsg->msg.lParam);
			}
			else
			{
				pmsg->result = -1;
			}
			SetEvent(hEvent);
		}
		else
		{
			std::map<UINT, MSGFUNC>::iterator it = m_mapFunc.find(msg.message);
			if (it != m_mapFunc.end())
			{
				(this->*it->second)(msg.message, msg.wParam, msg.lParam);
			}
		}
	}
}

unsigned __stdcall CClientController::threadEntry(void* arg)
{
	CClientController* thiz = (CClientController*)arg;
	thiz->threadFunc();
	_endthreadex(0);
	return 0;
}

LRESULT CClientController::OnSendPack(UINT nMsg, WPARAM wParam, LPARAM lParam)
{
	CClientSocket* pClient = CClientSocket::getInstance();
	CPacket* pPacket = (CPacket*)wParam;
	return pClient->Send(*pPacket);
}

LRESULT CClientController::OnSendData(UINT nMsg, WPARAM wParam, LPARAM lParam)
{
	CClientSocket* pClient = CClientSocket::getInstance();
	char* pBuffer = (char*)wParam;
	return pClient->Send(pBuffer, (int)lParam);
}

LRESULT CClientController::OnShowStatus(UINT nMsg, WPARAM wParam, LPARAM lParam)
{
	return m_statusDlg.ShowWindow(SW_SHOW);
}

LRESULT CClientController::OnShowWatch(UINT nMsg, WPARAM wParam, LPARAM lParam)
{
	return m_watchDlg.DoModal();
}
