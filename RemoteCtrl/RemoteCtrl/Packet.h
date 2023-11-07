#pragma once
#include "pch.h"
#include "framework.h"

#pragma pack(push)
#pragma pack(1)
class CPacket
{
public:
	CPacket() :sHead(0), nLength(0), sCmd(0), sSum(0) {}
	//打包的重构
	CPacket(WORD nCmd, const BYTE* pData, size_t nSize)
	{
		sHead = 0xFEFF;
		nLength = nSize + 2 * sizeof(WORD);
		sCmd = nCmd;
		if (nSize > 0)
		{
			strData.resize(nSize);
			memcpy((void*)strData.c_str(), pData, nSize);
		}
		else
		{
			strData.clear();
		}
		sSum = 0;
		for (size_t j = 0; j < strData.size(); j++)
		{
			sSum += BYTE(strData[j] & 0xFF);
		}
	}
	//解包的重构
	CPacket(const CPacket& pack)
	{
		sHead = pack.sHead;
		nLength = pack.nLength;
		sCmd = pack.sCmd;
		strData = pack.strData;
		sSum = pack.sSum;
	}
	CPacket(const BYTE* pData, int& nSize) :sHead(0), nLength(0), sCmd(0), sSum(0)
	{
		int i = 0;
		for (; i < nSize; i++)
		{
			if (*(WORD*)(pData + i) == 0xFEFF)
			{
				sHead = *(WORD*)(pData + i);
				i += sizeof(WORD);
				break;
			}
		}
		nLength = *(DWORD*)(pData + i);
		i += sizeof(DWORD);
		if (nLength + i > nSize)
		{
			nSize = 0;
			return;
		}
		sCmd = *(WORD*)(pData + i);
		i += sizeof(WORD);
		if (nLength > sizeof(WORD) + sizeof(WORD))
		{
			strData.resize(nLength - 2 * sizeof(WORD));
			memcpy((void*)strData.c_str(), pData + i, nLength - 2 * sizeof(WORD));
		}
		i += nLength - 2 * sizeof(WORD);
		sSum = *(WORD*)(pData + i);
		i += sizeof(WORD);
		WORD sum = 0;
		for (int j = 0; j < strData.size(); j++)
		{
			sum += BYTE(strData[j]) & 0xFF;
		}
		if (sum == sSum)
		{
			nSize = i;
			return;
		}
		nSize = 0;
	}
	~CPacket() {}
	CPacket operator=(const CPacket& pack)
	{
		if (this != &pack)
		{
			sHead = pack.sHead;
			nLength = pack.nLength;
			sCmd = pack.sCmd;
			strData = pack.strData;
			sSum = pack.sSum;
		}
		return *this;
	}
	int Size()
	{
		return nLength + sizeof(DWORD) + sizeof(WORD);
	}
	const char* Data()
	{
		strOut.resize(nLength + sizeof(DWORD) + sizeof(WORD));
		BYTE* pData = (BYTE*)strOut.c_str();
		*(WORD*)pData = sHead; pData += sizeof(WORD);
		*(DWORD*)pData = nLength; pData += sizeof(DWORD);
		*(WORD*)pData = sCmd; pData += sizeof(WORD);
		memcpy(pData, strData.c_str(), strData.size()); pData += strData.size();
		*(WORD*)pData = sSum;

		return strOut.c_str();
	}
public:
	WORD sHead;//固定位 0xFEFF
	DWORD nLength;//包长度，从控制命令到和校验结束
	WORD sCmd;//控制命令
	std::string strData;//包数据
	WORD sSum;//和校验
	std::string strOut;
};
#pragma pack(pop)

typedef struct MouseEvent
{
	MouseEvent()
	{
		nAction = 0;
		nButton = -1;
		ptXY.x = 0;
		ptXY.y = 0;
	}
	WORD nAction;//动作
	WORD nButton;//按键
	POINT ptXY;//坐标
}MOUSEEV, * PMOUSEEV;

typedef struct file_info {
	file_info() {
		IsInvalid = FALSE;
		IsDirectory = FALSE;
		HasNext = TRUE;
		memset(szFileName, 0, sizeof(szFileName));
	}
	BOOL IsInvalid;//是否有效
	BOOL IsDirectory;//是否是目录
	BOOL HasNext;
	char szFileName[256];//文件名
}FILEINFO, * PFILEINFO;