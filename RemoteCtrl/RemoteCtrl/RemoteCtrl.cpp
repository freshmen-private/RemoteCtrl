// RemoteCtrl.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include "pch.h"
#include "framework.h"
#include "RemoteCtrl.h"
#include "ServerSocket.h"
#include <direct.h>
#include <atlimage.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif
//#pragma comment( linker, "/subsystem:windows /entry:WinMainCRTStartup" )
//#pragma comment( linker, "/subsystem:windows /entry:mainCRTStartup" )
//#pragma comment( linker, "/subsystem:console /entry:WinMainCRTStartup" )
//#pragma comment( linker, "/subsystem:console /entry:mainCRTStartup" )

// 唯一的应用程序对象

CWinApp theApp;

using namespace std;
void Dump(BYTE* pData, size_t nSize)
{
    std::string strOut;
    for (size_t i = 0; i < nSize; i++)
    {
        char buf[8] = "";
        if ((i < 0) && (i % 16 == 0))
        {
            strOut += "\n";
        }
        snprintf(buf, sizeof(buf), "%02X", pData[i] & 0xFF);
        strOut += buf;
        strOut += " ";
    }
    strOut += "\n";
    OutputDebugStringA(strOut.c_str());
}

int MakeDriverInfo()
{
    std::string result;
    for(int i = 1; i < 26; i++)
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
    Dump((BYTE*)pack.Data(), pack.Size());
    CServerSocket::getInstance()->Send(pack);
    return 0;
}
#include <io.h>
#include <list>



int MakeDirecteryInfo()
{
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
        /*finfo.IsInvalid = TRUE;
        finfo.IsDirectory = TRUE;*/
        finfo.HasNext = FALSE;
        //memcpy(finfo.szFileName, strPath.c_str(), strPath.size());
        //lstFileInfos.push_back(finfo);
        CPacket pack(2, (BYTE*)&finfo, sizeof(finfo));
        CServerSocket::getInstance()->Send(pack);
        OutputDebugString(_T("没有访问权限\n"));
        return -2;
    }
    _finddata_t fdata;
    __int64 hfind = 0;
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
        CServerSocket::getInstance()->Send(pack);
    } while (!_findnext(hfind, &fdata));
    //发送信息到控制端
    FILEINFO finfo;
    finfo.HasNext = FALSE;
    CPacket pack(2, (BYTE*)&finfo, sizeof(finfo));
    CServerSocket::getInstance()->Send(pack);
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
    if(pFile != NULL)
    {
        fseek(pFile, 0, SEEK_END);
        data = _ftelli64(pFile);
        CPacket head(4, (BYTE*)&data, 8);
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
            nflags = 0x10;
            break;
        case 1:
            nflags = 0x20;
            break;
        case 2:
            nflags = 0x40;
            break;
        case 3:
            nflags = 0x80;
            break;
        }
        switch (mouse.nAction)
        {
        case 0:
            nflags |= 0x01;
            break;
        case 1:
            nflags |= 0x02;
            break;
        case 2:
            nflags |= 0x04;
            break;
        case 3:
            nflags |= 0x08;
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
    BitBlt(screen.GetDC(), 0, 0, 1920, 1020, hScreen, 0, 0, SRCCOPY);
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

#include "LockDialog.h"
CLockDialog dlg;
unsigned threadid = 0;

unsigned __stdcall threadLockDlg(void* arg)
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
    dlg.SetWindowPos(&dlg.wndTopMost, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
    ShowCursor(false);
    ::ShowWindow(::FindWindow(_T("Shell_TrayWnd"), NULL), SW_HIDE);
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
    _endthreadex(0);
    return 0; 
}

int LockMachine()
{
    if (dlg.m_hWnd == NULL || dlg.m_hWnd == INVALID_HANDLE_VALUE)
    {
        //_beginthread(threadLockDlg, 0, NULL);
        _beginthreadex(NULL, 0, threadLockDlg, NULL, 0, &threadid);
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

int ExcuteCommand(int nCmd)
{
    TRACE("sCmd:%d\n", nCmd);
    int ret = 0;
    switch (nCmd)
    {
    case 1:
        ret = MakeDriverInfo();
        break;
    case 2://查看指定目录下的文件
        ret = MakeDirecteryInfo();
        break;
    case 3:
        ret = RunFile();
        break;
    case 4:
        ret = DownloadFile();
        break;
    case 5:
        ret = MouseEvent();
        break;
    case 6:
        ret = SendScreen();
        break;
    case 7:
        ret = LockMachine();
        break;
    case 8:
        ret = UnLockMachine();
        break;
    case 1981:
        ret = TestConnect();
    }
    return ret;
}

int main()
{
    int nRetCode = 0;

    HMODULE hModule = ::GetModuleHandle(nullptr);

    if (hModule != nullptr)
    {
        // 初始化 MFC 并在失败时显示错误
        if (!AfxWinInit(hModule, nullptr, ::GetCommandLine(), 0))
        {
            // TODO: 在此处为应用程序的行为编写代码。
            wprintf(L"错误: MFC 初始化失败\n");
            nRetCode = 1;
        }
        else
        {
            //先难后易有以下好处：1、进度可控；2、方便对接；3、可行性评估，提早暴露风险
            // TODO: socket, listen, accept, read, write, close
            //套接字初始化
            //CServerSocket* pserver = CServerSocket::getInstance();
            CServerSocket* pserver = CServerSocket::getInstance();
            int count = 0;
            if (pserver->InitSocket() == false)
            {
                MessageBox(NULL, _T("网络初始化失败，未能成功初始化，请检查网络状态"), _T("网络初始化失败"), MB_OK | MB_ICONERROR);
                exit(0);
            }
            while (CServerSocket::getInstance() != NULL)
            {
                if (pserver->AcceptClient() == false)
                {
                    if(count >=3)
                    {
                        MessageBox(NULL, _T("多次无法正常接入用户，结束程序"), _T("接入用户失败"), MB_OK | MB_ICONERROR);
                        exit(0);
                    }
                    MessageBox(NULL, _T("无法正常接入用户，自动重试"), _T("接入用户失败"), MB_OK | MB_ICONERROR);
                    count++;
                }
                int ret = pserver->DealCommand();
                TRACE("sCmd:%d\n", ret);
                if(ret >= 0)
                {
                    ret = ExcuteCommand(pserver->GetPacket().sCmd);
                    if (ret != 0)
                    {
                        TRACE("执行命令失败:%d ret = %d\n", pserver->GetPacket().sCmd, ret);
                    }
                    pserver->CloseClient();
                }
            }
            //TODO
            
            //全局的静态变量
            
        }
    }
    else
    {
        // TODO: 更改错误代码以符合需要
        wprintf(L"错误: GetModuleHandle 失败\n");
        nRetCode = 1;
    }

    return nRetCode;
}
