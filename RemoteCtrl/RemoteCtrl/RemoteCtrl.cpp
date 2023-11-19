// RemoteCtrl.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//
#include "pch.h"
#include "framework.h"
#include "RemoteCtrl.h"
#include "ServerSocket.h"
#include "Command.h"
#include <conio.h>
#include "MyQueue.h"
#include <MSWSock.h>
#include "ENetwork.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif
#include "ESocket.h"
#include "MyServer.h"
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

void initsock()
{
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
}

void clearsock()
{
    WSACleanup();
}


int main(int argc, char* argv[])
{
    initsock();
    /*if (!CMyTool::Init()) return 1;
    
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
    else
    {
        udp_client();
    }
    clearsock();*/
    iocp();
    
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

/// <summary>
/// 1、易用性
///     a 简化参数
///     b 类型适配
///     c 流程简化
/// 2、易移植性（高内聚，低耦合）
///     高内聚：和项目相关联的东西通通聚在一起
///     低耦合：和项目没有关联的东西全部都不留在里面
///     a 核心功能是什么？
///     b 业务逻辑是什么？
/// 思路：做或者实现一个需求的过程
///     确定需求（阶段性的）、选定技术方案（依据技术点）、从框架开发到细节实现（从顶到底）、编译问题、
///     内存泄露（线程结束、exit函数）、bug排查与功能调试（日志、断点、线程、调用堆栈、内存、监视、局部变量、自动变量）、
///     压力测试、功能上线
/// 设计：易用性、可移植性（可复用性）、安全性（线程安全、异常处理，资源处理）、稳定性（鲁棒性）、可扩展性
///     有度、有条件的、可读性、效率、易用性
/// </summary>

int RecvFormCB(void* arg, const EBuffer& buffer, ESockaddrIn& addr)
{
    EServer* server = (EServer*)arg;
    return server->Sendto(addr, buffer);
}

int SendToBC(void* arg, const ESockaddrIn& addr, int ret)
{
    EServer* server = (EServer*)arg;
    printf("sendto done!%p\r\n", server);
    return 0;
}

void udp_server()
{
    std::list<ESockaddrIn> lstclients;
    printf("%s,(%d):%s\n", __FILE__, __LINE__, __FUNCTION__);
    EServerParamter param(
        "127.0.0.1", 20000, ETYPE::ETypeUDP, NULL, NULL, NULL, RecvFormCB, SendToBC
    );
    EServer server(param);
    server.Invoke(&server);
    printf("%s,(%d):%s\n", __FILE__, __LINE__, __FUNCTION__);
    getchar();
    return;
    //SOCKET sock = socket(PF_INET, SOCK_DGRAM, 0);
   
}
void udp_client(bool ishost)
{
    Sleep(2000);
    sockaddr_in server, client;
    int len = sizeof(server);
    memset(&server, 0, sizeof(server));
    memset(&server, 0, sizeof(client));
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr("127.0.0.1");
    server.sin_port = htons(20000);
    SOCKET sock = socket(PF_INET, SOCK_DGRAM, 0);
    if (sock == INVALID_SOCKET)
    {
        printf("%s,(%d):%s\n", __FILE__, __LINE__, __FUNCTION__);
        return;
    }
    if(ishost)
    {
        printf("host %s,(%d):%s\n", __FILE__, __LINE__, __FUNCTION__);
        EBuffer msg = "hello world\n";
        int ret = sendto(sock, (char*)msg.c_str(), msg.size(), 0, (sockaddr*)&server, sizeof(server));
        if (ret > 0)
        {
            msg.resize(1024);
            memset((char*)msg.c_str(), 0, msg.size());
            recvfrom(sock, (char*)msg.c_str(), msg.size(), 0, (sockaddr*)&client, &len);
            printf("%s(%d):%s ip %08X port %d\n", __FILE__, __LINE__, __FUNCTION__, client.sin_addr.s_addr, ntohs(client.sin_port));
        }
    }
    else
    {
        printf("host %s,(%d):%s\n", __FILE__, __LINE__, __FUNCTION__);
        std::string msg = "hello world\n";
        int ret = sendto(sock, (char*)msg.c_str(), msg.size(), 0, (sockaddr*)&server, sizeof(server));
        if (ret > 0)
        {
            recvfrom(sock, (char*)msg.c_str(), msg.size(), 0, (sockaddr*)&client, &len);
            printf("%s(%d):%s ip %08X port %d\n", __FILE__, __LINE__, __FUNCTION__, client.sin_addr.s_addr, ntohs(client.sin_port));
        }
    }
    closesocket(sock);
}

