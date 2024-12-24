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
	//��Unix��û����Щ�꣬Ϊ�˼��ݣ��Լ�����
	#define SOCKET int
	#define INVALID_SOCKET  (SOCKET)(~0)
	#define SOCKET_ERROR            (-1)
#endif

//���ջ������Ĵ�С
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
	bool	_isConnect;							//��ǰ�Ƿ�����
	//char	_recvBuff[RECV_BUFF_SIZE];			//��һ�����������ջ������������洢�����绺�����н��յ�����
	char	_recvMsgBuff[RECV_BUFF_SIZE];		//�ڶ�����������Ϣ������������һ�������е����ݴ洢���������������������������ж����ݽ��д���ճ���������
	int		_lastPos;							//������ʶ��ǰ��Ϣ�����������ݵĽ�βλ��

public:
	EasyTcpClient() : _sock(INVALID_SOCKET), _isConnect(false), _lastPos(0) {
		//memset(_recvBuff, 0, sizeof(_recvBuff));
		memset(_recvMsgBuff, 0, sizeof(_recvMsgBuff));
	}
	virtual ~EasyTcpClient() { CloseSocket(); }

public:
	void InitSocket();	//��ʼ��SOCKET
	void CloseSocket();	//�ر�SOCKET
	bool OnRun();		//����������Ϣ
	bool isRun();		//�жϵ�ǰ�ͻ����Ƿ���������
	int ConnectServer(const char* ip, unsigned int port); //���ӷ�����
	int RecvData();		//�������� ʹ��RecvData�����κ����͵����ݣ�Ȼ����Ϣ��ͷ���ֶδ��ݸ�OnNetMessage()�����У�������Ӧ��ͬ���͵���Ϣ
	virtual void OnNetMessage(DataHeader* header);		//��Ӧ������Ϣ
	int SendData(DataHeader* header, int nLen);			//��������
};

void EasyTcpClient::InitSocket() {
	//���֮ǰ�������ˣ��رվ����ӣ�����������
	if (INVALID_SOCKET != _sock) {
		printf("<socket=%d>�رվ�����...\n", _sock);
		CloseSocket();
	}

#ifdef _WIN32
	//����Windows socket 2.x����
	WORD ver = MAKEWORD(2, 2);
	WSADATA dat;
	WSAStartup(ver, &dat);
#endif

	_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (INVALID_SOCKET == _sock) {
		printf("���󣬽���Socketʧ��...\n");
	}
	else {
		//printf("����Socket=<%d>�ɹ�...\n", _sock);
	}
}

int EasyTcpClient::ConnectServer(const char* ip, unsigned int port) {
	if (INVALID_SOCKET == _sock) {
		InitSocket();
	}

	//����Ҫ���ӵķ���˵�ַ��ע�⣬��ͬƽ̨�ķ����IP��ַҲ��ͬ��
	sockaddr_in _sin = {};
	_sin.sin_family = AF_INET;
	_sin.sin_port = htons(port);
#ifdef _WIN32
	_sin.sin_addr.S_un.S_addr = inet_addr(ip);
#else
	_sin.sin_addr.s_addr = inet_addr(ip);
#endif

	//���ӷ����
	int ret = connect(_sock, (struct sockaddr*)&_sin, sizeof(_sin));
	if (SOCKET_ERROR == ret) {
		printf("<socket=%d>�������ӷ�����<%s:%d>ʧ��...\n", _sock, ip, port);
	}
	else {
		_isConnect = true;
		//printf("<socket=%d>���ӷ�����<%s:%d>�ɹ�...\n", _sock, ip, port);
	}
	return ret;
}

void EasyTcpClient::CloseSocket() {
	if (_sock != INVALID_SOCKET) {
#ifdef _WIN32
		closesocket(_sock);
		//���Windows socket����
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
			std::cout << "<Socket=" << _sock << ">��select����" << std::endl;
			CloseSocket();
			return false;
		}

		if (FD_ISSET(_sock, &fdRead)) {
			FD_CLR(_sock, &fdRead);
			if (-1 == RecvData()) {
				std::cout << "<Socket=" << _sock << ">�����ݽ���ʧ�ܣ��������ѶϿ���" << std::endl;
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
	//���շ��������
	int nLen = (int)recv(_sock, _recvBuff, RECV_BUFF_SIZE - _lastPos, 0);
	if (nLen < 0) {
		std::cout << "<Socket=" << _sock << ">��recv��������" << std::endl;
		return -1;
	}
	else if (nLen == 0) {
		std::cout << "<Socket=" << _sock << ">����������ʧ�ܣ�������ѹر�!" << std::endl;
		return -1;
	}

	//���յ������ݿ�������Ϣ������
	//memcpy(_recvMsgBuff + _lastPos, _recvBuff, nLen);

	//��Ϣ������������β��λ�ú���
	_lastPos += nLen;

	//�ж���Ϣ�����������ݳ��ȴ�����Ϣͷ����  
	while (_lastPos >= sizeof(DataHeader)) {
		//��ʱ�Ϳ���֪����ǰ��Ϣ����
		DataHeader* header = (DataHeader*)_recvMsgBuff;
		//�ж���Ϣ�����������ݳ��ȴ�����Ϣ����  
		if (_lastPos >= header->dataLength) {
			//ʣ��δ������Ϣ���������ݵĳ���
			int nSize = _lastPos - header->dataLength;
			//����������Ϣ
			OnNetMessage(header);
			//����Ϣ������δ��������ǰ��
			memcpy(_recvMsgBuff, _recvMsgBuff + header->dataLength, nSize);
			//��Ϣ����������ȥβ��λ��ǰ��
			_lastPos = nSize;
		}
		else {
			//��Ϣ������ʣ�����ݲ���һ��������Ϣ
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
		//printf("<socket=%d>�յ��������Ϣ��CMD_LOGIN_RESULT,���ݳ��ȣ�%d\n", _sock, login->dataLength);
	}
	break;
	case CMD_LOGOUT_RESULT:
	{
		LogoutResult* logout = (LogoutResult*)header;
		//printf("<socket=%d>�յ��������Ϣ��CMD_LOGOUT_RESULT,���ݳ��ȣ�%d\n", _sock, logout->dataLength);
	}
	break;
	case CMD_NEW_USER_JOIN:
	{
		NewUserJoin* userJoin = (NewUserJoin*)header;
		//printf("<socket=%d>�յ��������Ϣ��CMD_NEW_USER_JOIN,���ݳ��ȣ�%d\n", _sock, userJoin->dataLength);
	}
	break;
	case CMD_ERROR:
		//printf("<socket=%d>�յ��������Ϣ��CMD_ERROR,���ݳ��ȣ�%d\n", _sock, header->dataLength);
	break;
	default:
	{
		//printf("<socket=%d>�յ�δ������Ϣ,���ݳ��ȣ�%d\n", _sock, header->dataLength);
	}
	}
}

int EasyTcpClient::SendData(DataHeader* header, int nLen) {
	int ret = SOCKET_ERROR;
	if (isRun() && header) {
		ret = send(_sock, (const char*)header, nLen, 0);
		if (SOCKET_ERROR == ret) {
			CloseSocket();
			printf("Client:socket<%d>�������ݴ��󣬹رտͻ�������\n", static_cast<int>(_sock));
		}
	}
	return ret;
}

#endif
