﻿// RemoteCtrl.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//
#include "pch.h"
#include "framework.h"
#include "RemoteCtrl.h"
#include "ServerSocket.h"
#include "Command.h"
#include <conio.h>
#include "MyQueue.h"
#include <MSWSock.h>
#include "MyServer.h"

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
void iocp();

void udp_server();
void udp_client(bool ishost = true);


int main(int argc, char* argv[])
{
    if (!CMyTool::Init()) return 1;
    
    if (argc == 1)
    {
        char wstrDir[MAX_PATH];
        GetCurrentDirectoryA(MAX_PATH, wstrDir);
        STARTUPINFOA si;
        PROCESS_INFORMATION pi;
        string strCmd = argv[0];
        memset(&si, 0, sizeof(si));
        memset(&pi, 0, sizeof(pi));
        strCmd += " 1";
        bool bRet = CreateProcessA(NULL, (LPSTR)strCmd.c_str(), NULL, NULL, FALSE, 0, NULL, wstrDir, &si, &pi);
        if (bRet)
        {
            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);
            strCmd += " 2";
            bRet = CreateProcessA(NULL, (LPSTR)strCmd.c_str(), NULL, NULL, FALSE, 0, NULL, wstrDir, &si, &pi);
            if (bRet)
            {
                CloseHandle(pi.hThread);
                CloseHandle(pi.hProcess);
                udp_server();
            }
        }
    }
    else if (argc == 2)
    {
        udp_client(false);
    }
    //iocp();
    
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

void iocp()
{
    CMyServer server;
    server.StartService();
    getchar();
}

void udp_server()
{
    printf("%s,(%d):%s\n", __FILE__, __LINE__, __FUNCTION__);
}
void udp_client(bool ishost)
{
    if(ishost)
        printf("host %s,(%d):%s\n", __FILE__, __LINE__, __FUNCTION__);
    else
        printf("client %s,(%d):%s\n", __FILE__, __LINE__, __FUNCTION__);
}
