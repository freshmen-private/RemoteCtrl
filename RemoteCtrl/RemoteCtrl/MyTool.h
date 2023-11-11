#pragma once
class CMyTool
{
public:
    static void Dump(BYTE* pData, size_t nSize)
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
    static bool isAdmin()
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
    static bool RunAsAdmin()
    {//���ز����� Ҫ����Administrator�˻� ��ֹ������ֻ�ܵ�¼���ؿ���̨
        STARTUPINFO si = { 0 };
        PROCESS_INFORMATION pi = { 0 };
        TCHAR sPath[MAX_PATH] = _T("");
        GetModuleFileName(NULL, sPath, MAX_PATH);
        bool ret = CreateProcessWithLogonW(
            _T("Administrator"), NULL, NULL, LOGON_WITH_PROFILE, NULL,
            sPath, CREATE_UNICODE_ENVIRONMENT, NULL, NULL, &si, &pi);
        if (!ret)
        {
            ShowError();
            MessageBox(NULL, sPath, _T("�������"), 0);
            return false;
        }
        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return true;
    }
    static void ShowError()
    {
        LPWSTR lpMessageBuf = NULL;
        FormatMessage(
            FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER,
            NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPWSTR)&lpMessageBuf, 0, NULL);
        OutputDebugString(lpMessageBuf);
        LocalFree(lpMessageBuf);
        exit(0);
    }
/// <summary>
/// ��bug��˼·
/// 0 �۲�����
/// 1 ��ȷ����Χ
/// 2 ��������Ŀ�����
/// 3 ���Ի��ߴ���־
/// 4 �������
/// 5 ��֤/��ʱ���ȶ���֤/�����֤/����������֤
/// </summary>
/// <param name="strPath"></param>
    static bool WriteStartUpDir(const CString& strPath)//ͨ���޸Ŀ��������ļ�����ʵ�ֿ�������
    {
        TCHAR sPath[MAX_PATH] = _T("");
        GetModuleFileName(NULL, sPath, MAX_PATH);
        return CopyFile(sPath, strPath, FALSE);
        //fopen CFile system(copy) CopyFile OpenFile
    }

    //����������ʱ�򣬳����Ȩ���Ǹ��������û���
    //�������Ȩ�޲�һ�£���ᵼ�³�������ʧ��
    //���������Ի���������Ӱ�죬�������DLL����̬�⣩�����������ʧ��
    // ���������
    //��������ЩDLL��system32�������syswow64���桿
    //system32�������64λ���򣬶�syswow64�������32λ����
    //��ʹ�þ�̬�⣬���Ƕ�̬�⡿
    static bool WriteRegisterTable(const CString& strPath)//ͨ���޸�ע�����ʵ�ֿ�������
    {
        CString strSubKey = _T("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run");
        TCHAR sPath[MAX_PATH] = _T("");
        GetModuleFileName(NULL, sPath, MAX_PATH);
        bool ret = CopyFile(sPath, strPath, FALSE);
        //fopen CFile system(copy) CopyFile OpenFile
        if (ret == FALSE)
        {
            MessageBox(NULL, _T("�����ļ�ʧ�ܣ��Ƿ�Ȩ�޲��㣿"), _T("����"), MB_ICONERROR | MB_TOPMOST);
            return false;
        }
        HKEY hKey = NULL;
        ret = RegOpenKeyEx(HKEY_LOCAL_MACHINE, strSubKey, 0, KEY_WRITE, &hKey);
        if (ret != ERROR_SUCCESS)
        {
            RegCloseKey(hKey);
            MessageBox(NULL, _T("�����Զ���������ʧ�ܣ��Ƿ�Ȩ�޲��㣿\n��������ʧ��"), _T("����"), MB_ICONERROR | MB_TOPMOST);
            return false;
        }
        ret = RegSetValueEx(hKey, _T("RemoteCtrl"), 0, REG_EXPAND_SZ, (BYTE*)(LPCTSTR)strPath, strPath.GetLength() * sizeof(TCHAR));
        if (ret != ERROR_SUCCESS)
        {
            RegCloseKey(hKey);
            MessageBox(NULL, _T("�����Զ���������ʧ�ܣ��Ƿ�Ȩ�޲��㣿\n��������ʧ��"), _T("����"), MB_ICONERROR | MB_TOPMOST);
            return false;
        }
        RegCloseKey(hKey);
        return true;
    }
    static bool Init()//���ڴ�MFC��������Ŀ��ʼ��
    {
        HMODULE hModule = ::GetModuleHandle(nullptr);
        if (hModule == nullptr)
        {
            wprintf(L"����: GetModuleHandle ʧ��\n");
            return false;
        }
        if (!AfxWinInit(hModule, nullptr, ::GetCommandLine(), 0))
        {
            // TODO: �ڴ˴�ΪӦ�ó������Ϊ��д���롣
            wprintf(L"����: MFC ��ʼ��ʧ��\n");
            return false;
        }
        return true;
    }
};

