#pragma once
#include "pch.h"
#include "framework.h"
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
		char buff[1024] = { 0 };
		while (true) {
			int len = recv(m_client, buff, sizeof(buff) - 1, 0);
			if (len <= 0) {
				return -1;
			}
			//TODO : 处理命令
		}
	}
	bool Send(const char* pData, int nSize) {
		if (m_client == -1) return false;
		return send(m_client, pData, nSize, 0)>0;
	}
private:
	SOCKET m_sock, m_client;
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