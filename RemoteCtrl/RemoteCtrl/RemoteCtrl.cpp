// RemoteCtrl.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//
#include "pch.h"
#include "framework.h"
#include "RemoteCtrl.h"
#include "ServerSocket.h"
#include "Command.h"

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
//开机启动的时候，程序的权限是跟随启动用户的
//如果两者权限不一致，则会导致程序启动失败
//开机启动对环境变量有影响，如果依赖DLL（动态库），则可能启动失败
// 解决方案：
//【复制这些DLL到system32下面或者syswow64下面】
//system32下面多是64位程序，而syswow64下面多是32位程序
//【使用静态库，而非动态库】
//

void WriteRegisterTable(const CString& strPath)
{
    CString strSubKey = _T("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run");
    int ret = 0;
    char sPath[MAX_PATH] = "";
    char sSys[MAX_PATH] = "";
    std::string strExe = "\\RemoteCtrl.exe ";
    GetCurrentDirectoryA(MAX_PATH, sPath);
    GetSystemDirectoryA(sSys, sizeof(sSys));

    std::string strCmd = "mklink " + std::string(sSys) + strExe + std::string(sPath) + strExe;
    system(strCmd.c_str());
    HKEY hKey = NULL;
    ret = RegOpenKeyEx(HKEY_LOCAL_MACHINE, strSubKey, 0, KEY_WRITE, &hKey);
    if (ret != ERROR_SUCCESS)
    {
        RegCloseKey(hKey);
        MessageBox(NULL, _T("设置自动开机启动失败，是否权限不足？\n程序启动失败"), _T("错误"), MB_ICONERROR | MB_TOPMOST);
        exit(0);
    }
    ret = RegSetValueEx(hKey, _T("RemoteCtrl"), 0, REG_EXPAND_SZ, (BYTE*)(LPCTSTR)strPath, strPath.GetLength() * sizeof(TCHAR));
    if (ret != ERROR_SUCCESS)
    {
        RegCloseKey(hKey);
        MessageBox(NULL, _T("设置自动开机启动失败，是否权限不足？\n程序启动失败"), _T("错误"), MB_ICONERROR | MB_TOPMOST);
        exit(0);
    }
    RegCloseKey(hKey);
}
/// <summary>
/// 改bug的思路
/// 0 观察现象
/// 1 先确定范围
/// 2 分析错误的可能性
/// 3 调试或者打日志
/// 4 处理错误
/// 5 验证/长时间稳定验证/多次验证/多条件的验证
/// </summary>
/// <param name="strPath"></param>
void WriteStartUpDir(const CString& strPath)
{
    CString strCmd = GetCommandLine();
    strCmd.Replace(_T("\""), _T(""));
    bool ret = CopyFile(strCmd, strPath, FALSE);
    //fopen CFile system(copy) CopyFile OpenFile
    if (ret == FALSE)
    {
        MessageBox(NULL, _T("复制文件失败，是否权限不足？"), _T("错误"), MB_ICONERROR | MB_TOPMOST);
        exit(0);
    }
}

void ChooseAutoInvoke()
{
    TCHAR wcsSystem[MAX_PATH] = _T("");
    //CString strPath = CString(wcsSystem) + CString(_T("%SystemRoot%\\SysWOW64\\RemoteCtrl.exe"));
    CString strPath = _T("C:\\Users\\zzz\\AppData\\Roaming\\Microsoft\\Windows\\Start Menu\\Programs\\Startup\\RemoteCtrl");
    if (PathFileExists(strPath))
    {
        return;
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
        WriteStartUpDir(strPath);
    }
    else if (ret == IDCANCEL)
    {
        exit(0);
    }
}

void ShowError()
{
    LPWSTR lpMessageBuf = NULL;
    FormatMessage(
        FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_ALLOCATE_BUFFER,
        NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPWSTR)&lpMessageBuf, 0, NULL);
    OutputDebugString(lpMessageBuf);
    LocalFree(lpMessageBuf);
    exit(0);
}

bool isAdmin()
{
    HANDLE hToken = NULL;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken))
    {
        ShowError();
        return FALSE;
    }
    TOKEN_ELEVATION eve;
    DWORD len = 0;
    if (!GetTokenInformation(hToken, TokenElevation, &eve, sizeof(eve), &len))
    {
        ShowError();
        return FALSE;
    }
    if (len == sizeof(eve))
    {
        return eve.TokenIsElevated;
    }
    printf("length of token information is %d\r\n", len);
    return FALSE;
}

void RunAsAdmin()
{
    HANDLE hToken = NULL;
    bool ret = LogonUser(L"Administrator", NULL, NULL, LOGON32_LOGON_INTERACTIVE, LOGON32_PROVIDER_DEFAULT, &hToken);
    if (!ret)
    {
        ShowError();
        exit(0);
    }
    OutputDebugString(L"current administrator success!\n");
    STARTUPINFO si = { 0 };
    PROCESS_INFORMATION pi = { 0 };
    TCHAR sPath[MAX_PATH] = _T("");
    GetCurrentDirectory(MAX_PATH, sPath);
    CString strCmd = sPath;
    strCmd += _T("RemoteCtrl.exe");
    //ret = CreateProcessWithTokenW(hToken, LOGON_WITH_PROFILE, NULL, (LPWSTR)(LPCWSTR)strCmd, CREATE_UNICODE_ENVIRONMENT, NULL, NULL, &si, &pi);
    ret = CreateProcessWithLogonW(
        _T("Administrator"), NULL, NULL, LOGON_WITH_PROFILE, NULL, 
        (LPWSTR)(LPCWSTR)strCmd, CREATE_UNICODE_ENVIRONMENT, NULL,
        NULL, &si, &pi);
    if (!ret)
    {
        ShowError();
        MessageBox(NULL, _T("创建进程失败"), _T("程序错误"), 0);
        exit(0);
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hToken);
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
            if (isAdmin())
            {
                OutputDebugString(L"current is run as administrator\n");
            }
            else
            {
                OutputDebugString(L"current is run as normal user\n");
                RunAsAdmin();
                return nRetCode;
            }
            CCommand cmd;
            ChooseAutoInvoke();
            CServerSocket* pserver = CServerSocket::getInstance();
            int ret = pserver->Run(&CCommand::RunCommand, &cmd);
            switch (ret)
            {
            case -1:
                MessageBox(NULL, _T("网络初始化失败，未能成功初始化，请检查网络状态"), _T("网络初始化失败"), MB_OK | MB_ICONERROR);
                exit(0);
                break;
            case -2:
                MessageBox(NULL, _T("多次无法正常接入用户，结束程序"), _T("接入用户失败"), MB_OK | MB_ICONERROR);
                exit(0);
                break;
            }
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
