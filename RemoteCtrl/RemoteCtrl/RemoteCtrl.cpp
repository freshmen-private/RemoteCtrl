// RemoteCtrl.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//
#include "pch.h"
#include "framework.h"
#include "RemoteCtrl.h"
#include "ServerSocket.h"
#include "Command.h"
#include <conio.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif
//#pragma comment( linker, "/subsystem:windows /entry:WinMainCRTStartup" )
//#pragma comment( linker, "/subsystem:windows /entry:mainCRTStartup" )
//#pragma comment( linker, "/subsystem:console /entry:WinMainCRTStartup" )
//#pragma comment( linker, "/subsystem:console /entry:mainCRTStartup" )

// 唯一的应用程序对象
//define INVOKE_PATH _T("C:\\Windows\\SysWOW64\\RemoteCtrl.exe")
#define INVOKE_PATH _T("C:\\Users\\zzz\\AppData\\Roaming\\Microsoft\\Windows\\Start Menu\\Programs\\Startup\\RemoteCtrl")

CWinApp theApp;

using namespace std;
//业务和通用
bool ChooseAutoInvoke(const CString& strPath)
{
    TCHAR wcsSystem[MAX_PATH] = _T("");
    if (PathFileExists(strPath))
    {
        return false;
    }
    CString strInfo = _T("该程序只允许用于合法的用途！\n");
    strInfo += _T("继续运行该程序，将使得这台机器处于被监控状态！\n");
    strInfo += _T("如果你不希望这样，请请按“取消”按钮，推出程序！\n");
    strInfo += _T("按下“是”按钮，该程序将被复制到你的机器上，并随系统启动而自动运行！\n");
    strInfo += _T("按“否”按钮，程序只运行一次，不会在系统内留下任何东西！\n");
    int ret = MessageBox(NULL, strInfo, _T("警告"), MB_YESNOCANCEL | MB_ICONWARNING | MB_TOPMOST);
    if (ret == IDYES) 
    {
        //WriteRegisterTable(strPath);
        if (CMyTool::WriteStartUpDir(strPath))
        {
            MessageBox(NULL, _T("复制文件失败，是否权限不足"), _T("错误"), MB_ICONERROR | MB_TOPMOST);
            return false;
        }
    }
    else if (ret == IDCANCEL)
    {
        return false;
    }
    return true;
}

#define IOCP_LIST_EMPTY 0
#define IOCP_LIST_PUSH 1
#define IOCP_LIST_POP 2
enum {
    IocpListEmpty = 0,
    IocpListPush,
    IocpListPop
};

typedef struct iocpParam {
    int nOperator;
    std::string strData;
    iocpParam(int op, const char* sData){
        nOperator = op;
        strData = sData;
    }
    iocpParam() {
        nOperator = -1;
    }
}IOCPPARAM;

void threadQueueEntry(HANDLE hIOCP)
{
    std::list<std::string> lstString;
    DWORD dwTransferred = 0;
    ULONG_PTR CompletionKey = 0;
    OVERLAPPED* pOverlapped = NULL;
    while (GetQueuedCompletionStatus(hIOCP, &dwTransferred, &CompletionKey, &pOverlapped, INFINITE))
    {
        if (dwTransferred == 0 && CompletionKey == NULL)
        {
            printf("thread is prepare to exit");
            break;
        }
    }
    _endthread();//代码到此为止，会导致本地变量无法调用析构，从而使得内存发生泄露
}
int main()
{
    if (!CMyTool::Init()) return 1;
    HANDLE hIOCP = INVALID_HANDLE_VALUE;//IO Completion Port
    hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, 1);//epoll的区别1:
    HANDLE hThread = (HANDLE)_beginthread(threadQueueEntry, 0, hIOCP);
    printf("press anykey to exit...\n");
    ULONGLONG tick = GetTickCount64();
    while (_kbhit() != 0)
    {
        if (GetTickCount64() - tick > 1000)
        {
            PostQueuedCompletionStatus(hIOCP, sizeof(IOCPPARAM), (ULONG_PTR)new IOCPPARAM(IocpListPush, "hello world"), NULL);
            tick = GetTickCount64();
        }
        if (GetTickCount64() - tick > 1000)
        {
            PostQueuedCompletionStatus(hIOCP, sizeof(IOCPPARAM), (ULONG_PTR)new IOCPPARAM(IocpListPush, "hello world"), NULL);
            tick = GetTickCount64();
        }
        Sleep(1);
    }
    if (hIOCP != NULL)
    {// TODO 唤醒完成端口
        PostQueuedCompletionStatus(hIOCP, 0, NULL, NULL);
        WaitForSingleObject(hThread, INFINITE);
    }
    CloseHandle(hIOCP);
    printf("exit done\n");
    exit(0);
    /*if (CMyTool::isAdmin())
    {
        if (!CMyTool::Init()) return 1;
        if (ChooseAutoInvoke(INVOKE_PATH))
        {
            CCommand cmd;
            CServerSocket* pserver = CServerSocket::getInstance();
            int ret = pserver->Run(&CCommand::RunCommand, &cmd);
            switch (ret)
            {
            case -1:
                MessageBox(NULL, _T("网络初始化失败，未能成功初始化，请检查网络状态"), _T("网络初始化失败"), MB_OK | MB_ICONERROR);
                break;
            case -2:
                MessageBox(NULL, _T("多次无法正常接入用户，结束程序"), _T("接入用户失败"), MB_OK | MB_ICONERROR);
                break;
            }
        }
    }
    else
    {
        if (!CMyTool::RunAsAdmin())
        {
            CMyTool::ShowError();
            return 1;
        }
    }*/
    return 0;
}
