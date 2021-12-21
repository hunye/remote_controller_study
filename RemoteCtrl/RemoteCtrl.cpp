// RemoteCtrl.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include "pch.h"
#include "framework.h"
#include "RemoteCtrl.h"
#include "ServerSocket.h"
#include <direct.h>
#include <io.h>
#include <list>
#include<atlimage.h>
#ifdef _DEBUG
#define new DEBUG_NEW
#endif
// 唯一的应用程序对象

CWinApp theApp;
using namespace std;
typedef struct file_info{
	file_info() {
		IsInvalid = 0;
		IsDirectory = -1;
		HasNext = true;
		memset(szFileName, 0, sizeof(szFileName));
	}
	char szFileName[256];
	BOOL IsDirectory; //是否为目录
	BOOL IsInvalid; //是否无效
	BOOL HasNext; //是否还有后续

}FILEINFO, *PFILEINFO;
void Dump(BYTE* pData, size_t nSize) {
	string strOut;
	for (size_t i = 0; i < nSize; i++) {
		char buf[8] = "";
		if (i > 0 && i % 16 == 0) strOut += "\n";
		snprintf(buf, sizeof(buf), "%02X ", pData[i] & 0xff); //每次读入两个字节
		strOut += buf;
	}
	strOut += "\n";
	OutputDebugStringA(strOut.c_str());
}
int MakeDriverInfo() {
	string res;
	for (int i = 1; i <= 26; i++) {
		if(_chdrive(i) == 0){
			if (res.size()) res += ',';
			res += 'A' + i-1;

		}
	}
	CPacket pack(1, (BYTE*)res.c_str(), res.size());
	Dump((BYTE*)pack.Data(),pack.Size());
	CServerSocket::getInstance()->Send(pack);
	return 0;
}
int MakeDirectoryInfo() {
	string strPath;
	list<FILEINFO> lstFileInfo;
	if (CServerSocket::getInstance()->GetFilePath(strPath) == false) {
		OutputDebugString(_T("当前的命令，不是获取文件列表，命令解析错误！"));
		return -1;
	}
	if (_chdir(strPath.c_str())!=0) {
		FILEINFO finfo;
		finfo.IsInvalid = TRUE;
		finfo.IsDirectory = TRUE;
		finfo.HasNext = FALSE;
		memcpy(finfo.szFileName, strPath.c_str(), strPath.size());
		//lstFileInfo.push_back(finfo);
		CPacket pack(2, (BYTE*)&finfo, sizeof(finfo));
		CServerSocket::getInstance()->Send(pack);
		OutputDebugString(_T("没有权限，访问目录！"));
		return -2;
	}
	_finddata_t fdata;
	int hfind = 0;
	if ((hfind=_findfirst("*", &fdata)) == -1) {
		OutputDebugString(_T("没有找到任何文件！"));
		return -3;
	}
	do {
		FILEINFO finfo;
		finfo.IsDirectory = (fdata.attrib&_A_SUBDIR) != 0;
		finfo.IsInvalid = FALSE;
		memcpy(finfo.szFileName, fdata.name, strlen(fdata.name));
		//lstFileInfo.push_back(finfo);
		CPacket pack(2, (BYTE*)&finfo, sizeof(finfo));
		CServerSocket::getInstance()->Send(pack);

	} while (!_findnext(hfind, &fdata));
	FILEINFO finfo;
	finfo.HasNext = FALSE;
	CPacket pack(2, (BYTE*)&finfo, sizeof(finfo));
	CServerSocket::getInstance()->Send(pack);
	return 0;
}
int RunFile() {
	string strPath;
	CServerSocket::getInstance()->GetFilePath(strPath);
	ShellExecuteA(NULL, NULL, strPath.c_str(), NULL, NULL, SW_SHOWNORMAL);
	CPacket pack(3, NULL, 0);
	CServerSocket::getInstance()->Send(pack);
	return 0;
}
int DownloadFile() {
	string strPath;
	CServerSocket::getInstance()->GetFilePath(strPath);
	long long data = 0;
	FILE* pfile = NULL;
	//二进制文件不能按照文本的方式打开
	//fopen_s调查完之后会验证一下
	errno_t err=fopen_s(&pfile, strPath.c_str(), "rb");
	if (err!=0 ) {
		CPacket pack(4, (BYTE*)&data, 8);
		CServerSocket::getInstance()->Send(pack);
		return -1;
	}
	if (pfile != NULL) {
		fseek(pfile, 0, SEEK_END);
		data = _ftelli64(pfile);
		CPacket head(4, (BYTE*)&data, 8);
		fseek(pfile, 0, SEEK_SET);
		CServerSocket::getInstance()->Send(head);
		char buff[1024] = "";//tcp命令最大4k
		size_t rlen = 0;
		do {
			rlen = fread(buff, 1, 1024, pfile);
			CPacket pack(4, (BYTE*)buff, rlen);
			CServerSocket::getInstance()->Send(pack);
		} while (rlen >= 1024);
		fclose(pfile);
	}
	CPacket pack(4, NULL, 0);
	CServerSocket::getInstance()->Send(pack);
	
	return 0;
}
int MouseEvent() {
	MOUSEEV mouse;
	if (CServerSocket::getInstance()->GetMouseEvent(mouse)) {
		
		DWORD nFlags = 0;
		switch (mouse.nButton) {
		case 0://左键
			nFlags = 1;
			break;
		case 1://右键
			nFlags = 2;
			break;
		case 2://中键
			nFlags = 4;
			break;
		case 4://没有按键
			nFlags = 8;
			break;

		}
		if (nFlags != 8) SetCursorPos(mouse.ptXY.x, mouse.ptXY.y);
		switch (mouse.nAction) {
		case 0://单机
			nFlags |= 0x10;
			break;
		case 1://双击
			nFlags |= 0x20;
			break;
		case 2://按下
			nFlags |= 0x40;
			break;
		case 3://放开
			nFlags |= 0x80;
			break;
		default:
			break;
		}
		switch (nFlags) {//避免嵌套
		case 0x21://左键双击
			mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, GetMessageExtraInfo());
			mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, GetMessageExtraInfo());
		case 0x11://左键单机
			mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, GetMessageExtraInfo());
			mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, GetMessageExtraInfo());
			break;
		case 0x41://左键按下
			mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, GetMessageExtraInfo());
			break;
		case 0x81://左键放开
			mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, GetMessageExtraInfo());
			break;
		case 0x22://右键双击
			mouse_event(MOUSEEVENTF_RIGHTDOWN, 0, 0, 0, GetMessageExtraInfo());
			mouse_event(MOUSEEVENTF_RIGHTUP, 0, 0, 0, GetMessageExtraInfo());
		case 0x12://右键单击
			mouse_event(MOUSEEVENTF_RIGHTDOWN, 0, 0, 0, GetMessageExtraInfo());
			mouse_event(MOUSEEVENTF_RIGHTUP, 0, 0, 0, GetMessageExtraInfo());
			break;
		case 0x42://右键按下
			mouse_event(MOUSEEVENTF_RIGHTDOWN, 0, 0, 0, GetMessageExtraInfo());
			break;
		case 0x82://右键放开
			mouse_event(MOUSEEVENTF_RIGHTUP, 0, 0, 0, GetMessageExtraInfo());
			break;
		case 0x24://中键双击
			mouse_event(MOUSEEVENTF_MIDDLEDOWN, 0, 0, 0, GetMessageExtraInfo());
			mouse_event(MOUSEEVENTF_MIDDLEUP, 0, 0, 0, GetMessageExtraInfo());
		case 0x14://中键单击
			mouse_event(MOUSEEVENTF_MIDDLEDOWN, 0, 0, 0, GetMessageExtraInfo());
			mouse_event(MOUSEEVENTF_MIDDLEUP, 0, 0, 0, GetMessageExtraInfo());
			break;
		case 0x44://中键按下
			mouse_event(MOUSEEVENTF_MIDDLEDOWN, 0, 0, 0, GetMessageExtraInfo());
			break;
		case 0x84://中键放开
			mouse_event(MOUSEEVENTF_MIDDLEUP, 0, 0, 0, GetMessageExtraInfo());
			break;
		case 0x08://没有按键,单纯鼠标移动
			mouse_event(MOUSEEVENTF_MOVE, mouse.ptXY.x, mouse.ptXY.y, 0, GetMessageExtraInfo());
			break;
		}
		CPacket pack(4, NULL, 0);
		CServerSocket::getInstance()->Send(pack);

	}
	else {
		OutputDebugString(_T("获取鼠标操作参数失败！"));
		return -1;
	}

	return 0;
}
int SendScreen() {
	CImage screen;//GDI
	HDC hScreen=::GetDC(NULL);
	int nBitPerPixel = GetDeviceCaps(hScreen, BITSPIXEL);
	int nWidth = GetDeviceCaps(hScreen, HORZRES);
	int nHeight = GetDeviceCaps(hScreen, VERTRES);
	screen.Create(nWidth, nHeight, nBitPerPixel);
	BitBlt(screen.GetDC(), 0, 0, 1920, 1020, hScreen, 0, 0, SRCCOPY);
	ReleaseDC(NULL, hScreen);
	//DWORD tick=GetTickCount();//监控运行时间
	HGLOBAL hMem=GlobalAlloc(GMEM_MOVEABLE, 0);//把文件保存到内存
	if (hMem == NULL) return -1;
	IStream *pStream = NULL;
	HRESULT ret=CreateStreamOnHGlobal(hMem, TRUE, &pStream);//内存上创建一个流

	if (ret == S_OK) {
		screen.Save(pStream, Gdiplus::ImageFormatPNG);
		pStream->Seek({ 0 }, STREAM_SEEK_SET, NULL);
		PBYTE pData=(PBYTE)GlobalLock(hMem);
		SIZE_T nSize = GlobalSize(hMem);
		CPacket pack(6, pData, nSize);
		CServerSocket::getInstance()->Send(pack);
		GlobalUnlock(hMem);
	}
	

	//screen.Save(_T("test2021.png"), Gdiplus::ImageFormatPNG); //保存到本地
	//TRACE("png: %d ms\r\n", GetTickCount() - tick);
	pStream->Release();
	GlobalFree(hMem);
	screen.ReleaseDC();
	
	return 0;
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
			//命令需求

            // TODO: 在此处为应用程序的行为编写代码。
			//WSADATA data;
			//WSAStartup(MAKEWORD(1, 1), &data);//TODO:
			//CServerSocket* pserver = CServerSocket::getInstance();
			//int count = 0;
			//if (pserver->InitSocket() == false) {
			//	MessageBox(NULL, _T("请检查网络"), _T("网络初始化失败！"), MB_OK | MB_ICONERROR);
			//	exit(0);
			//}
			//while (CServerSocket::getInstance()!=NULL) {
			//	if (pserver->AcceptClient() == false) {
			//		if (count >= 3) {
			//			MessageBox(NULL, _T("多次无法接入，结束程序"), _T("接入用户失败！"), MB_OK | MB_ICONERROR);
			//			exit(0);
			//		}
			//		MessageBox(NULL, _T("接入用户失败"), _T("接入用户失败！"), MB_OK | MB_ICONERROR);
			//		count++;
			//	}
			//	int ret = pserver->DealCommand();
			//}

			//文件需求
			int nCmd = 6;
			switch (nCmd) {
			case 1://查看磁盘分区
				MakeDriverInfo();
				break;
			case 2://查看指定目录下的文件
				MakeDirectoryInfo();
				break;
			case 3://打开文件
				RunFile();
				break;
			case 4://下载文件
				DownloadFile();
				break;
			case 5://鼠标操作
				MouseEvent();
				break;
			case 6://发送屏幕内容
				SendScreen();
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
