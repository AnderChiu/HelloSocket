﻿#include "Alloctor.h"
#include "EasyTcpServer.hpp"
#include<thread>
bool g_bRun = true;
void cmdThread() {
	while (true) {
		char cmdBuf[256] = {};
		std::cin >> cmdBuf;
		if (0 == strcmp(cmdBuf, "exit")) {
			g_bRun = false;
			printf("退出cmdThread线程\n");
			break;
		}
		else {
			printf("不支持的命令。\n");
		}
	}
}

class MyServer : public EasyTcpServer {
public:
	//只会被一个线程触发 安全
	virtual void OnClientJoin(ClientSocket* pClient) {
		EasyTcpServer::OnClientJoin(pClient);
	}
	//cellserver 4 多个线程触发 不安全 如果只开启1个cellServer就是安全的
	virtual void OnClientLeave(ClientSocket* pClient) {
		EasyTcpServer::OnClientLeave(pClient);
	}

	virtual void OnNetRecv(ClientSocket* pClient) {
		EasyTcpServer::OnNetRecv(pClient);
	}

	//cellserver 4 多个线程触发 不安全 如果只开启1个cellServer就是安全的
	virtual void OnNetMsg(CellServer* pCellServer, ClientSocket* pClient, DataHeader* header) {
		EasyTcpServer::OnNetMsg(pCellServer, pClient, header);
		switch (header->cmd) {
		case CMD_LOGIN:
		{

			Login* login = (Login*)header;
			//printf("收到客户端<Socket=%d>请求：CMD_LOGIN,数据长度：%d,userName=%s PassWord=%s\n", cSock, login->dataLength, login->userName, login->PassWord);
			//忽略判断用户密码是否正确的过程
			//LoginResult ret;
			//pClient->SendData(&ret);
			LoginResult* ret = new LoginResult();
			pCellServer->addSendTask(pClient, ret);
		}
		break;
		case CMD_LOGOUT:
		{
			Logout* logout = (Logout*)header;
			//printf("收到客户端<Socket=%d>请求：CMD_LOGOUT,数据长度：%d,userName=%s \n", cSock, logout->dataLength, logout->userName);
			//忽略判断用户密码是否正确的过程
			//LogoutResult ret;
			//SendData(cSock, &ret);
		}
		break;
		default:
		{
			printf("<socket=%d>收到未定义消息,数据长度：%d\n", pClient->sockfd(), header->dataLength);
			//DataHeader ret;
			//SendData(cSock, &ret);
		}
		break;
		}
	}
};

int main() {
	MyServer server;
	server.InitSocket();
	server.Bind(nullptr, 4567);
	server.Listen(5);
	server.Start(4);

	std::thread t1(cmdThread);
	t1.detach();

	while (g_bRun) {
		server.OnRun();
		//printf("空闲时间处理其它业务..\n");
	}
	server.Close();
	printf("已退出。\n");
	getchar();
	return 0;
}
