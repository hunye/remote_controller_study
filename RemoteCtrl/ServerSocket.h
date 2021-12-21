#pragma once
#include "pch.h"
#include "framework.h"
class CPacket {
public:
	CPacket():sHead(0), nLength(0), sCmd(0), sSum(0) {

	}
	CPacket(WORD nCmd, const BYTE* pData, size_t nSize) {
		sHead = 0xFEFF;
		nLength = nSize + 4;
		sCmd = nCmd;
		if (nSize > 0) {
			strData.resize(nSize);
			memcpy((void*)strData.c_str(), pData, nSize);
		}
		else {
			strData.clear();
		}
		sSum = 0;
		for (size_t j = 0; j < strData.size(); ++j) {
			sSum += BYTE(strData[j]) & 0xFF;
		}
	}
	CPacket(const BYTE* pData, size_t& nSize) { //解包
		size_t i = 0;
		for ( ; i < nSize; ++i) {
			if (*(WORD*)(pData + i) == 0xFEFF) {
				sHead = *(WORD*)(pData + i);
				i += 2;
				break;
			}
		}
		if (i+4+2+2 > nSize) {
			nSize = 0;
			return;
		}
		nLength = *(DWORD*)(pData + i); i += 4;
		if (nLength + i > nSize) {
			nSize = 0;
			return;
		}
		sCmd = *(WORD*)(pData + i); i += 2;
		strData.resize(nLength - 2 - 2);
		memcpy((void*)strData.c_str(), pData + i, nLength-4);
		i += nLength - 4;
		sSum = *(WORD*)(pData + i); i += 2;
		WORD sum = 0;
		for (size_t j = 0; j < strData.size(); ++j) {
			sum += BYTE(strData[j]) & 0xFF;
		}
		if (sSum == sum) {
			nSize = i; 
			return;
		}
		nSize = 0;
		return;
	}
	CPacket(const CPacket& pack) {
		sHead = pack.sHead;
		nLength = pack.nLength;
		sCmd = pack.sCmd;
		sSum = pack.sSum;
	}
	~CPacket() {

	}
	CPacket& operator=(CPacket& pack) {
		sHead = pack.sHead;
		nLength = pack.nLength;
		sCmd = pack.sCmd;
		sSum = pack.sSum;
	}
	int Size() {
		return nLength + 6;
	}
	const char * Data() {
		strOut.resize(Size());
		BYTE* pData = (BYTE*)strOut.c_str();
		*(WORD*)pData = sHead; pData += 2;
		*(DWORD*)pData = nLength; pData += 4;
		*(WORD*)pData = sCmd; pData += 2;
		OutputDebugStringA(strOut.c_str());
		memcpy(pData, strData.c_str(), strData.size()); pData += strData.size();
		*(WORD*)pData = sSum;

		return strOut.c_str();
	}
public:
	WORD sHead;//包头 FE FF
	DWORD nLength;
	WORD sCmd;//控制命令
	std::string strData;
	WORD sSum;//和校验
	std::string strOut; //整个包的数据

};
typedef struct MouseEvent{
	MouseEvent() {
		nAction = 0;
		nButton = -1;
		ptXY.x = 0;
		ptXY.y = 0;
	}
	WORD nAction; //点击，移动，双击
	WORD nButton; //左键，右键， 滚轮
	POINT ptXY; //坐标
}MOUSEEV, *PMOUSEEV;

class CServerSocket
{
public:
	static CServerSocket* getInstance() {
		//静态函数没有this指针，无法访问成员变量
		//static是全局的，脱离this指针可以全局访问的，所有与this无关

		if (m_instance == NULL) { 
			m_instance = new CServerSocket();

		}
		return m_instance;
	}
	bool InitSocket() {
		
		if (m_sock == -1) return false;
		sockaddr_in serv_addr;
		memset(&serv_addr, 0, sizeof serv_addr);
		serv_addr.sin_family = AF_INET;
		serv_addr.sin_addr.s_addr = INADDR_ANY;
		serv_addr.sin_port = htons(5001);

		if (bind(m_sock, (sockaddr*)&serv_addr, sizeof(serv_addr)) == -1) {
			return false;
		}
		if (listen(m_sock, 1)==-1)//一对一
		{
			return false;
		}
		return true;
	}
	bool AcceptClient() {
		sockaddr_in client_addr;
		int client_sz = sizeof(client_addr);
		m_client=accept(m_sock, (sockaddr*)&client_addr, &client_sz);
		if (m_client == -1) return false;
		return true;
		
	}
	int DealCommand() {
		if (m_client == -1) return -1;
		char *buff = new char[4096];
		memset(buff, 0, sizeof buff);
		size_t index = 0;
		while (true) {
			size_t len = recv(m_client, buff+index, sizeof(buff)-index, 0);
			if (len <= 0) {
				return -1;
			}
			index += len;
			len = index;
			CPacket packet((BYTE*)buff, len);
			if (len > 0) {
				memmove(buff, buff + len, sizeof(buff)-len);
				index -= len;
				return packet.sCmd;
			}
		}
		return -1;
	}
	bool Send(const char* pData, int nSize) {
		if (m_client == -1) return false;
		return send(m_client, pData, nSize, 0)>0;
	}
	bool Send(CPacket& pack) {
		if (m_client == -1) return false;

		return send(m_client, pack.Data(), pack.Size(), 0) > 0;
	}
	bool GetFilePath(std::string &strPath) {
		if (m_packet.sCmd >= 2 || m_packet.sCmd<=4) {
			strPath = m_packet.strData;
			return true;
		}
		return false;
		
	}
	bool GetMouseEvent(MOUSEEV& mouse) {
		if (m_packet.sCmd == 5) {
			memcpy(&mouse, m_packet.strData.c_str(), sizeof(MOUSEEV));
			return true;
		}
		return false;
	}
private:
	SOCKET m_sock, m_client;
	CPacket m_packet;
	CServerSocket& operator=(const CServerSocket&) {
		
	}
	CServerSocket(const CServerSocket& ss) {
		m_sock = ss.m_sock;
		m_client = ss.m_client;

	}
	CServerSocket() {
		m_sock = -1;//INVALID_SOCKET
		m_client = -1;
		if (InitSockEnv() == FALSE) {
			MessageBox(NULL, _T("无法初始化套接字环境"), _T("初始化错误"), MB_OK|MB_ICONERROR);
			exit(0);
		}
		m_sock = socket(PF_INET, SOCK_STREAM, 0);
	}
	~ CServerSocket() {
		if (m_sock) closesocket(m_sock);
		WSACleanup(); 
	}
	BOOL InitSockEnv() {
		WSADATA data;
		if (WSAStartup(MAKEWORD(1, 1), &data) != 0) {
			return FALSE;
		}
		return true;
	}
	static void releaseInstance() {
		if (m_instance) {
			CServerSocket* tmp = m_instance;
			m_instance = NULL;
			delete tmp;
		}
	}
	class CHelper {
	public:
		CHelper() {
			CServerSocket::getInstance();
		}
		~CHelper() {
			CServerSocket::releaseInstance();
		}
	};
	static CHelper m_helper;
	static CServerSocket* m_instance;
};

//extern CServerSocket server;