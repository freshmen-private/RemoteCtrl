#pragma once
#include "Resource.h"
#include <map>
#include <atlimage.h>
#include <direct.h>
#include "MyTool.h"
#include <io.h>
#include <list>
#include <stdio.h>
#include "LockDialog.h"
#include "Packet.h"

#pragma warning(dissable:4996)
class CCommand
{
public:
	CCommand();
	~CCommand() {}
	int ExcuteCommand(int, std::list<CPacket>&, CPacket&);
    static void RunCommand(void* arg, int status, std::list<CPacket>& lstPackets, CPacket& inPacket)
    {
        CCommand* thiz = (CCommand*)arg;
        if (status > 0)
        {
            int ret = thiz->ExcuteCommand(status, lstPackets, inPacket);
            if (ret != 0)
            {
                TRACE("执行命令失败:%d ret = %d\n", status, ret);
            }
        }
        else
        {
            MessageBox(NULL, _T("无法正常接入用户，自动重试"), _T("接入用户失败"), MB_OK | MB_ICONERROR);
        }
    }
protected:
	typedef int(CCommand::* CMDFUNC)(std::list<CPacket>&, CPacket&);//成员函数指针
	std::map<int, CMDFUNC>m_mapFunction;//从命令号到功能的映射
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

    int MakeDriverInfo(std::list<CPacket>& lstPackets, CPacket& inPacket)
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
        lstPackets.push_back(CPacket(1, (BYTE*)result.c_str(), result.size()));
        return 0;
    }

    int MakeDirecteryInfo(std::list<CPacket>& lstPackets, CPacket& inPacket)
    {
        std::string strPath = inPacket.strData;
        //std::list<FILEINFO> lstFileInfos;
        if (_chdir(strPath.c_str()) != 0)
        {
            FILEINFO finfo;
            finfo.HasNext = FALSE;
            lstPackets.push_back(CPacket(2, (BYTE*)&finfo, sizeof(finfo)));
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
            lstPackets.push_back(CPacket(2, (BYTE*)&finfo, sizeof(finfo)));
            return -3;
        }
        do
        {
            FILEINFO finfo;
            finfo.IsDirectory = ((fdata.attrib & _A_SUBDIR) != 0);
            memcpy(finfo.szFileName, fdata.name, strlen(fdata.name));
            TRACE("%s\n", finfo.szFileName);
            lstPackets.push_back(CPacket(2, (BYTE*)&finfo, sizeof(finfo)));
        } while (!_findnext(hfind, &fdata));
        //发送信息到控制端
        FILEINFO finfo;
        finfo.HasNext = FALSE;
        lstPackets.push_back(CPacket(2, (BYTE*)&finfo, sizeof(finfo)));
        return 0;
    }

    int RunFile(std::list<CPacket>& lstPackets, CPacket& inPacket)
    {
        std::string strPath = inPacket.strData;
        ShellExecuteA(NULL, NULL, strPath.c_str(), NULL, NULL, SW_SHOWNORMAL);
        lstPackets.push_back(CPacket(3, NULL, 0));
        return 0;
    }

    int DownloadFile(std::list<CPacket>& lstPackets, CPacket& inPacket)
    {
        std::string strPath = inPacket.strData;
        long long data = 0;
        FILE* pFile = NULL;
        errno_t err = fopen_s(&pFile, strPath.c_str(), "rb");
        if (err != 0)
        {
            lstPackets.push_back(CPacket(4, (BYTE*)&data, 8));
            return -1;
        }
        if (pFile != NULL)
        {
            fseek(pFile, 0, SEEK_END);
            data = _ftelli64(pFile);
            lstPackets.push_back(CPacket(4, (BYTE*)&data, 8));
            fseek(pFile, 0, SEEK_SET);
            char buffer[1024] = "";
            size_t rlen = 0;
            do
            {
                rlen = fread(buffer, 1, 1024, pFile);
                lstPackets.push_back(CPacket(4, (BYTE*)buffer, rlen));
            } while (rlen >= 1024);
            fclose(pFile);
        }
        lstPackets.push_back(CPacket(4, NULL, 0));
        return 0;
    }

    int MouseEvent(std::list<CPacket>& lstPackets, CPacket& inPacket)
    {
        MOUSEEV mouse;
        memcpy(&mouse, inPacket.strData.c_str(), sizeof(MOUSEEV));
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
        lstPackets.push_back(CPacket(5, NULL, 0));
        return 0;
    }

    int SendScreen(std::list<CPacket>& lstPackets, CPacket& inPacket)
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
            lstPackets.push_back(CPacket(6, pData, nSize));
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

    int LockMachine(std::list<CPacket>& lstPackets, CPacket& inPacket)
    {
        if (dlg.m_hWnd == NULL || dlg.m_hWnd == INVALID_HANDLE_VALUE)
        {
            //_beginthread(threadLockDlg, 0, NULL);
            _beginthreadex(NULL, 0, &CCommand::threadLockDlg, this, 0, &threadid);
        }

        lstPackets.push_back(CPacket(7, NULL, 0));
        return 0;
    }

    int UnLockMachine(std::list<CPacket>& lstPackets, CPacket& inPacket)
    {
        //dlg.SendMessage(WM_KEYDOWN, 0x1b, 0x00010001);
        ::SendMessage(dlg.m_hWnd, WM_KEYDOWN, 0x1b, 0x00010001);
        PostThreadMessage(threadid, WM_KEYDOWN, 0x1b, 0x00010001);
        lstPackets.push_back(CPacket(7, NULL, 0));
        return 0;
    }

    int TestConnect(std::list<CPacket>& lstPackets, CPacket& inPacket)
    {
        lstPackets.push_back(CPacket(1981, NULL, 0));
        return 0;
    }

    int DeleteLocalFile(std::list<CPacket>& lstPackets, CPacket& inPacket)
    {
        std::string strPath = inPacket.strData;
        TCHAR sPath[MAX_PATH];
        //mbstowcs(sPath, strPath.c_str(), strPath.size());//中文容易乱码
        MultiByteToWideChar(CP_ACP, 0, strPath.c_str(), strPath.size(), sPath,
            sizeof(sPath) / sizeof(WCHAR));
        DeleteFileA(strPath.c_str());
        lstPackets.push_back(CPacket(9, NULL, 0));
        return 0;
    }
};

