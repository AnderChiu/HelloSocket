#ifndef _TcpClient_hpp_
#define _TcpClient_hpp_


#ifdef _WIN32
	#define WIN32_LEAN_AND_MEAN
	#define _WINSOCK_DEPRECATED_NO_WARNINGS //for inet_pton()
	#define _CRT_SECURE_NO_WARNINGS
	#include <windows.h>
	#include <WinSock2.h>
	#pragma comment(lib, "ws2_32.lib")
#else
	#include <unistd.h>
	#include <sys/socket.h>
	#include <sys/types.h>
	#include <arpa/inet.h>
	#include <netinet/in.h>
	#include <sys/select.h>
	//在Unix下没有这些宏，为了兼容，自己定义
	#define SOCKET int
	#define INVALID_SOCKET  (SOCKET)(~0)
	#define SOCKET_ERROR            (-1)
#endif

//接收缓冲区的大小
#ifndef RECV_BUFF_SIZE
#define RECV_BUFF_SIZE 10240
#endif // !RECV_BUFF_SIZE

#include <iostream>
#include <string.h>
#include <stdio.h>
#include "MessageHeader.hpp"

class EasyTcpClient {
private:
	SOCKET	_sock;
	bool	_isConnect;							//当前是否连接
	//char	_recvBuff[RECV_BUFF_SIZE];			//第一缓冲区（接收缓冲区）用来存储从网络缓冲区中接收的数据
	char	_recvMsgBuff[RECV_BUFF_SIZE];		//第二缓冲区（消息缓冲区）将第一缓冲区中的数据存储在这个缓冲区，并在这个缓冲区中对数据进行处理（粘包拆包处理）
	int		_lastPos;							//用来标识当前消息缓冲区中数据的结尾位置

public:
	EasyTcpClient() : _sock(INVALID_SOCKET), _isConnect(false), _lastPos(0) {
		//memset(_recvBuff, 0, sizeof(_recvBuff));
		memset(_recvMsgBuff, 0, sizeof(_recvMsgBuff));
	}
	virtual ~EasyTcpClient() { CloseSocket(); }

public:
	void InitSocket();	//初始化SOCKET
	void CloseSocket();	//关闭SOCKET
	bool OnRun();		//处理网络消息
	bool isRun();		//判断当前客户端是否正在运行
	int ConnectServer(const char* ip, unsigned int port); //连接服务器
	int RecvData();		//接收数据 使用RecvData接收任何类型的数据，然后将消息的头部字段传递给OnNetMessage()函数中，让其响应不同类型的消息
	virtual void OnNetMessage(DataHeader* header);		//响应网络消息
	int SendData(DataHeader* header, int nLen);			//发送数据
};

void EasyTcpClient::InitSocket() {
	//如果之前有连接了，关闭旧连接，开启新连接
	if (INVALID_SOCKET != _sock) {
		printf("<socket=%d>关闭旧连接...\n", _sock);
		CloseSocket();
	}

#ifdef _WIN32
	//启动Windows socket 2.x环境
	WORD ver = MAKEWORD(2, 2);
	WSADATA dat;
	WSAStartup(ver, &dat);
#endif

	_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (INVALID_SOCKET == _sock) {
		printf("错误，建立Socket失败...\n");
	}
	else {
		//printf("建立Socket=<%d>成功...\n", _sock);
	}
}

int EasyTcpClient::ConnectServer(const char* ip, unsigned int port) {
	if (INVALID_SOCKET == _sock) {
		InitSocket();
	}

	//声明要连接的服务端地址（注意，不同平台的服务端IP地址也不同）
	sockaddr_in _sin = {};
	_sin.sin_family = AF_INET;
	_sin.sin_port = htons(port);
#ifdef _WIN32
	_sin.sin_addr.S_un.S_addr = inet_addr(ip);
#else
	_sin.sin_addr.s_addr = inet_addr(ip);
#endif

	//连接服务端
	int ret = connect(_sock, (struct sockaddr*)&_sin, sizeof(_sin));
	if (SOCKET_ERROR == ret) {
		printf("<socket=%d>错误，连接服务器<%s:%d>失败...\n", _sock, ip, port);
	}
	else {
		_isConnect = true;
		//printf("<socket=%d>连接服务器<%s:%d>成功...\n", _sock, ip, port);
	}
	return ret;
}

void EasyTcpClient::CloseSocket() {
	if (_sock != INVALID_SOCKET) {
#ifdef _WIN32
		closesocket(_sock);
		//清除Windows socket环境
		WSACleanup();
#else
		close(_sock);
#endif
		_sock = INVALID_SOCKET;
	}
	_isConnect = false;
}

bool EasyTcpClient::OnRun() {
	if (isRun()) {
		fd_set fdRead;
		FD_ZERO(&fdRead);
		FD_SET(_sock, &fdRead);

		timeval t = { 0,0 };
		int ret = select(_sock + 1, &fdRead, 0, 0, &t);
		if (ret < 0) {
			std::cout << "<Socket=" << _sock << ">：select出错！" << std::endl;
			CloseSocket();
			return false;
		}

		if (FD_ISSET(_sock, &fdRead)) {
			FD_CLR(_sock, &fdRead);
			if (-1 == RecvData()) {
				std::cout << "<Socket=" << _sock << ">：数据接收失败，或服务端已断开！" << std::endl;
				CloseSocket();
				return false;
			}
		}
		return true;
	}
	return false;
}

bool EasyTcpClient::isRun() {
	return ((_sock != INVALID_SOCKET) && _isConnect);
}

int EasyTcpClient::RecvData() {
	char* _recvBuff = _recvMsgBuff + _lastPos;
	//接收服务端数据
	int nLen = (int)recv(_sock, _recvBuff, RECV_BUFF_SIZE - _lastPos, 0);
	if (nLen < 0) {
		std::cout << "<Socket=" << _sock << ">：recv函数出错！" << std::endl;
		return -1;
	}
	else if (nLen == 0) {
		std::cout << "<Socket=" << _sock << ">：接收数据失败，服务端已关闭!" << std::endl;
		return -1;
	}

	//将收到的数据拷贝到消息缓冲区
	//memcpy(_recvMsgBuff + _lastPos, _recvBuff, nLen);

	//消息缓冲区的数据尾部位置后移
	_lastPos += nLen;

	//判断消息缓冲区的数据长度大于消息头长度  
	while (_lastPos >= sizeof(DataHeader)) {
		//这时就可以知道当前消息长度
		DataHeader* header = (DataHeader*)_recvMsgBuff;
		//判断消息缓冲区的数据长度大于消息长度  
		if (_lastPos >= header->dataLength) {
			//剩余未处理消息缓冲区数据的长度
			int nSize = _lastPos - header->dataLength;
			//处理网络消息
			OnNetMessage(header);
			//将消息缓冲区未处理数据前移
			memcpy(_recvMsgBuff, _recvMsgBuff + header->dataLength, nSize);
			//消息缓冲区的数去尾部位置前移
			_lastPos = nSize;
		}
		else {
			//消息缓冲区剩余数据不够一条完整消息
			break;
		}
	}
	return 0;
}

void EasyTcpClient::OnNetMessage(DataHeader* header) {
	switch (header->cmd) {
	case CMD_LOGIN_RESULT:
	{
		LoginResult* login = (LoginResult*)header;
		//printf("<socket=%d>收到服务端消息：CMD_LOGIN_RESULT,数据长度：%d\n", _sock, login->dataLength);
	}
	break;
	case CMD_LOGOUT_RESULT:
	{
		LogoutResult* logout = (LogoutResult*)header;
		//printf("<socket=%d>收到服务端消息：CMD_LOGOUT_RESULT,数据长度：%d\n", _sock, logout->dataLength);
	}
	break;
	case CMD_NEW_USER_JOIN:
	{
		NewUserJoin* userJoin = (NewUserJoin*)header;
		//printf("<socket=%d>收到服务端消息：CMD_NEW_USER_JOIN,数据长度：%d\n", _sock, userJoin->dataLength);
	}
	break;
	case CMD_ERROR:
		//printf("<socket=%d>收到服务端消息：CMD_ERROR,数据长度：%d\n", _sock, header->dataLength);
	break;
	default:
	{
		//printf("<socket=%d>收到未定义消息,数据长度：%d\n", _sock, header->dataLength);
	}
	}
}

int EasyTcpClient::SendData(DataHeader* header, int nLen) {
	int ret = SOCKET_ERROR;
	if (isRun() && header) {
		ret = send(_sock, (const char*)header, nLen, 0);
		if (SOCKET_ERROR == ret) {
			CloseSocket();
			printf("Client:socket<%d>发送数据错误，关闭客户端连接\n", static_cast<int>(_sock));
		}
	}
	return ret;
}

#endif
