#pragma once
#include "Resource.h"
#include <map>
#include <atlimage.h>
#include "ServerSocket.h"
#include <direct.h>
#include "MyTool.h"
#include <io.h>
#include <list>
#include <stdio.h>
#include "LockDialog.h"

#pragma warning(dissable:4996)
class CCommand
{
public:
	CCommand();
	~CCommand() {}
	int ExcuteCommand(int nCmd);
protected:
	typedef int(CCommand::* CMDFUNC)();
	std::map<int, CMDFUNC>m_mapFunction;
    CLockDialog dlg;
    unsigned threadid;
protected:
    static unsigned __stdcall threadLockDlg(void* arg)
    {
        CCommand* thiz = (CCommand*)arg;
        thiz->threadLockDlgMain();
        _endthreadex(0);
        return 0;
    }

    void threadLockDlgMain()
    {
        dlg.Create(IDD_DIALOG_INFO, NULL);
        dlg.ShowWindow(SW_SHOW);
        CRect rect;
        rect.left = 0;
        rect.right = GetSystemMetrics(SM_CXFULLSCREEN);
        rect.top = 0;
        rect.bottom = GetSystemMetrics(SM_CXFULLSCREEN);
        rect.bottom = LONG(rect.bottom * 1.03);
        dlg.MoveWindow(rect);
        CWnd* pText = dlg.GetDlgItem(IDC_STATIC);
        if (pText)
        {
            CRect rtText;
            pText->GetWindowRect(rtText);
            int x = (rect.right - rtText.Width() / 2) / 2;
            int y = (rect.bottom - rtText.Height() / 2) / 2;
            pText->MoveWindow(x, y, rtText.Width(), rtText.Height());
        }
        dlg.SetWindowPos(&dlg.wndTopMost, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
        ShowCursor(false);
        ::ShowWindow(::FindWindow(_T("Shell_TrayWnd"), NULL), SW_HIDE);
        dlg.GetWindowRect(rect);
        rect.left = 0;
        rect.right = 0;
        rect.top = 1;
        rect.bottom = 1;
        ClipCursor(rect);
        MSG msg;
        while (GetMessage(&msg, NULL, 0, 0))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_KEYDOWN && msg.wParam == 0x1b)
            {
                TRACE("msg:%08X wparam:%08x lparm:%08X\r\n", msg.message, msg.wParam, msg.lParam);
                break;
            }
        }
        ::ShowWindow(::FindWindow(_T("Shell_TrayWnd"), NULL), SW_SHOW);
        ShowCursor(true);
        dlg.DestroyWindow();
    }

    int MakeDriverInfo()
    {
        std::string result;
        for (int i = 1; i < 26; i++)
        {
            if (_chdrive(i) == 0)
            {
                if (result.size() > 0)
                {
                    result += ",";
                }
                result += 'A' + i - 1;
            }
        }
        CPacket pack(1, (BYTE*)result.c_str(), result.size());
        CMyTool::Dump((BYTE*)pack.Data(), pack.Size());
        CServerSocket::getInstance()->Send(pack);
        return 0;
    }

    int MakeDirecteryInfo()
    {
        int count = 0;
        std::string strPath;
        //std::list<FILEINFO> lstFileInfos;
        if (CServerSocket::getInstance()->GetFilePath(strPath) == false)
        {
            OutputDebugString(_T("当前的命令不是获取文件列表，命令解析错误\n"));
            return -1;
        }
        if (_chdir(strPath.c_str()) != 0)
        {
            FILEINFO finfo;
            finfo.HasNext = FALSE;
            CPacket pack(2, (BYTE*)&finfo, sizeof(finfo));
            CServerSocket::getInstance()->Send(pack);
            OutputDebugString(_T("没有访问权限\n"));
            return -2;
        }
        _finddata_t fdata;
        intptr_t hfind = 0;
        if ((hfind = _findfirst("*", &fdata)) == -1)
        {
            OutputDebugString(_T("没有找到任何文件\n"));
            FILEINFO finfo;
            finfo.HasNext = FALSE;
            //TRACE("%s\n", finfo.szFileName);
            CPacket pack(2, (BYTE*)&finfo, sizeof(finfo));
            CServerSocket::getInstance()->Send(pack);
            return -3;
        }
        do
        {
            FILEINFO finfo;
            finfo.IsDirectory = ((fdata.attrib & _A_SUBDIR) != 0);
            memcpy(finfo.szFileName, fdata.name, strlen(fdata.name));
            TRACE("%s\n", finfo.szFileName);
            CPacket pack(2, (BYTE*)&finfo, sizeof(finfo));
            bool ret = CServerSocket::getInstance()->Send(pack);
            if (ret)
            {
                count++;
            }
        } while (!_findnext(hfind, &fdata));
        //发送信息到控制端
        FILEINFO finfo;
        finfo.HasNext = FALSE;
        CPacket pack(2, (BYTE*)&finfo, sizeof(finfo));
        CServerSocket::getInstance()->Send(pack);
        TRACE("send fileinfo count = %d\n", count + 1);
        return 0;
    }

    int RunFile()
    {
        std::string strPath;
        CServerSocket::getInstance()->GetFilePath(strPath);
        ShellExecuteA(NULL, NULL, strPath.c_str(), NULL, NULL, SW_SHOWNORMAL);
        CPacket pack(3, NULL, 0);
        CServerSocket::getInstance()->Send(pack);
        return 0;
    }

    int DownloadFile()
    {
        std::string strPath;
        CServerSocket::getInstance()->GetFilePath(strPath);
        long long data = 0;
        FILE* pFile = NULL;
        errno_t err = fopen_s(&pFile, strPath.c_str(), "rb");
        if (err != 0)
        {
            CPacket pack(4, (BYTE*)&data, 8);
            CServerSocket::getInstance()->Send(pack);
            return -1;
        }
        if (pFile != NULL)
        {
            fseek(pFile, 0, SEEK_END);
            data = _ftelli64(pFile);
            CPacket head(4, (BYTE*)&data, 8);
            CServerSocket::getInstance()->Send(head);
            fseek(pFile, 0, SEEK_SET);
            char buffer[1024] = "";
            size_t rlen = 0;
            do
            {
                rlen = fread(buffer, 1, 1024, pFile);
                CPacket pack(4, (BYTE*)buffer, rlen);
                CServerSocket::getInstance()->Send(pack);
            } while (rlen >= 1024);
            CPacket pack(4, NULL, 0);
            CServerSocket::getInstance()->Send(pack);
            fclose(pFile);
        }
        return 0;
    }

    int MouseEvent()
    {
        MOUSEEV mouse;
        if (CServerSocket::getInstance()->GetMouseEvent(mouse))
        {
            SetCursorPos(mouse.ptXY.x, mouse.ptXY.y);
            DWORD nflags = 0;
            switch (mouse.nButton)
            {
            case 0:
                nflags = 0x10;//左键
                break;
            case 1:
                nflags = 0x20;//右键
                break;
            case 2:
                nflags = 0x40;//中键
                break;
            case 3:
                nflags = 0x80;//弹起
                break;
            }
            switch (mouse.nAction)
            {
            case 0:
                nflags |= 0x01;//单击
                break;
            case 1:
                nflags |= 0x02;//双击
                break;
            case 2:
                nflags |= 0x04;//按下
                break;
            case 3:
                nflags |= 0x08;//弹起
                break;
            }
            switch (nflags)
            {
            case 0x12:
                mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, GetMessageExtraInfo());
                mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, GetMessageExtraInfo());
            case 0x11:
                mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, GetMessageExtraInfo());
                mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, GetMessageExtraInfo());
                break;
            case 0x14:
                mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, GetMessageExtraInfo());
                break;
            case 0x18:
                mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, GetMessageExtraInfo());
                break;
            case 0x22:
                mouse_event(MOUSEEVENTF_RIGHTDOWN, 0, 0, 0, GetMessageExtraInfo());
                mouse_event(MOUSEEVENTF_RIGHTUP, 0, 0, 0, GetMessageExtraInfo());
            case 0x21:
                mouse_event(MOUSEEVENTF_RIGHTDOWN, 0, 0, 0, GetMessageExtraInfo());
                mouse_event(MOUSEEVENTF_RIGHTUP, 0, 0, 0, GetMessageExtraInfo());
                break;
            case 0x24:
                mouse_event(MOUSEEVENTF_RIGHTDOWN, 0, 0, 0, GetMessageExtraInfo());
                break;
            case 0x28:
                mouse_event(MOUSEEVENTF_RIGHTUP, 0, 0, 0, GetMessageExtraInfo());
                break;
            case 0x42:
                mouse_event(MOUSEEVENTF_MIDDLEDOWN, 0, 0, 0, GetMessageExtraInfo());
                mouse_event(MOUSEEVENTF_MIDDLEUP, 0, 0, 0, GetMessageExtraInfo());
            case 0x41:
                mouse_event(MOUSEEVENTF_MIDDLEDOWN, 0, 0, 0, GetMessageExtraInfo());
                mouse_event(MOUSEEVENTF_MIDDLEUP, 0, 0, 0, GetMessageExtraInfo());
                break;
            case 0x44:
                mouse_event(MOUSEEVENTF_MIDDLEDOWN, 0, 0, 0, GetMessageExtraInfo());
                break;
            case 0x48:
                mouse_event(MOUSEEVENTF_MIDDLEUP, 0, 0, 0, GetMessageExtraInfo());
                break;
            default:
                mouse_event(MOUSEEVENTF_MOVE, mouse.ptXY.x, mouse.ptXY.y, 0, GetMessageExtraInfo());
                break;
            }
            CPacket pack(5, NULL, 0);
            CServerSocket::getInstance()->Send(pack);
        }
        else
        {
            OutputDebugString(_T("获取鼠标操作参数失败\n"));
            return -1;
        }
        return 0;
    }

    int SendScreen()
    {
        CImage screen;//GDI(glable divice interface
        HDC hScreen = ::GetDC(NULL);
        int nBitPerPixel = GetDeviceCaps(hScreen, BITSPIXEL);
        int nWidth = GetDeviceCaps(hScreen, HORZRES);
        int nHeight = GetDeviceCaps(hScreen, VERTRES);
        screen.Create(nWidth, nHeight, nBitPerPixel);
        BitBlt(screen.GetDC(), 0, 0, nWidth, nHeight, hScreen, 0, 0, SRCCOPY);
        ReleaseDC(NULL, hScreen);
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, 0);
        if (hMem == NULL)
        {
            return -1;
        }
        IStream* pStream = NULL;
        HRESULT ret = CreateStreamOnHGlobal(hMem, TRUE, &pStream);
        if (ret == S_OK)
        {
            screen.Save(pStream, Gdiplus::ImageFormatPNG);
            LARGE_INTEGER bg = { 0 };
            pStream->Seek(bg, STREAM_SEEK_SET, NULL);
            PBYTE pData = (PBYTE)GlobalLock(hMem);
            SIZE_T nSize = GlobalSize(hMem);
            CPacket pack(6, pData, nSize);
            CServerSocket::getInstance()->Send(pack);
            GlobalUnlock(hMem);
        }
        //DWORD tick = GetTickCount();
        //screen.Save(_T("test2023.png"), Gdiplus::ImageFormatPNG);
        //TRACE("png:%d", GetTickCount() - tick);
        //screen.Save(_T("test2023.jpg"), Gdiplus::ImageFormatJPEG);
        //TRACE("jpg:%d", GetTickCount() - tick);
        pStream->Release();
        GlobalFree(hMem);
        screen.ReleaseDC();
        return 0;
    }

    int LockMachine()
    {
        if (dlg.m_hWnd == NULL || dlg.m_hWnd == INVALID_HANDLE_VALUE)
        {
            //_beginthread(threadLockDlg, 0, NULL);
            _beginthreadex(NULL, 0, &CCommand::threadLockDlg, this, 0, &threadid);
        }
        CPacket pack(7, NULL, 0);
        CServerSocket::getInstance()->Send(pack);
        return 0;
    }

    int UnLockMachine()
    {
        //dlg.SendMessage(WM_KEYDOWN, 0x1b, 0x00010001);
        ::SendMessage(dlg.m_hWnd, WM_KEYDOWN, 0x1b, 0x00010001);
        PostThreadMessage(threadid, WM_KEYDOWN, 0x1b, 0x00010001);
        CPacket pack(7, NULL, 0);
        CServerSocket::getInstance()->Send(pack);
        return 0;
    }

    int TestConnect()
    {
        CPacket pack(1981, NULL, 0);
        CServerSocket::getInstance()->Send(pack);
        return 0;
    }

    int DeleteLocalFile()
    {
        TRACE("DeleteFile begin\n");
        std::string strPath;
        CServerSocket::getInstance()->GetFilePath(strPath);
        TCHAR sPath[MAX_PATH];
        //mbstowcs(sPath, strPath.c_str(), strPath.size());//中文容易乱码
        MultiByteToWideChar(CP_ACP, 0, strPath.c_str(), strPath.size(), sPath,
            sizeof(sPath) / sizeof(WCHAR));
        DeleteFileA(strPath.c_str());
        CPacket pack(9, NULL, 0);
        bool ret = CServerSocket::getInstance()->Send(pack);
        return 0;
    }
};

