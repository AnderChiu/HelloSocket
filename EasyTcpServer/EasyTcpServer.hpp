#ifndef _TcpServer_hpp_
#define _TcpServer_hpp_

#ifdef _WIN32
	#define WIN32_LEAN_AND_MEAN
	#define _WINSOCK_DEPRECATED_NO_WARNINGS
	#define FD_SETSIZE 10240
	#include<windows.h>
	#include<WinSock2.h>
	#pragma comment(lib,"ws2_32.lib")
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

#include<stdio.h>
#include<vector>
#include<thread>
#include<mutex>
#include<atomic>
#include<functional>
#include<iostream>
#include<map>
#include"MessageHeader.hpp"
#include"CELLTimestamp.hpp"
#include"CELLTask.hpp"

//��������С��Ԫ��С
#ifndef RECV_BUFF_SIZE
#define RECV_BUFF_SIZE 10240*5			//���ջ������Ĵ�С
#define SEND_BUFF_SIZE RECV_BUFF_SIZE	//���ͻ������Ĵ�С
#endif // RECV_BUFF_SIZE
//#define _CellServer_THREAD_COUNT 4

class CellServer;

//�ͻ�����������
class ClientSocket {
private:
	SOCKET	_sockfd;						//�ͻ���socket
	char	_recvMsgBuff[RECV_BUFF_SIZE];	//��Ϣ���ջ�����
	int		_lastRecvPos;					//��Ϣ���ջ�����������β��λ��
	char	_sendMsgBuff[RECV_BUFF_SIZE];	//��Ϣ���ͻ�����
	int		_lastSendPos;					//��Ϣ���ͻ�����������β��λ��
public:
	ClientSocket(SOCKET sockfd = INVALID_SOCKET): _sockfd(sockfd), _lastRecvPos(0), _lastSendPos(0) {
		memset(_recvMsgBuff, 0, RECV_BUFF_SIZE);
		memset(_sendMsgBuff, 0, SEND_BUFF_SIZE);
	}

	SOCKET sockfd() { return _sockfd; }
	char* recvMsgBuff() { return _recvMsgBuff; }
	int getRecvLastPos() { return _lastRecvPos; }
	void setRecvLastPos(int pos) { _lastRecvPos = pos; }

	char* sendMsgBuff() { return _sendMsgBuff; }
	int getSendLastPos() { return _lastSendPos; }
	void setSendLastPos(int pos) { _lastSendPos = pos; }

	//��������
	int SendData(DataHeader* header) {
		int ret = SOCKET_ERROR;
		int nSendLen = header->dataLength;				//Ҫ���͵����ݳ���
		const char* pSendData = (const char*)header;	//Ҫ���͵�����
		
		//�������һ��if֮�����nSendLen��Ȼ����SEND_BUFF_SIZE����ô����Ҫ����ִ��if��������������
		while (true) {
			//�����ǰ"Ҫ���͵����ݵĳ���+���������ݽ�βλ��"֮���ܵĻ�������С��˵�����������ˣ���ô�����������������ͳ�ȥ
			if (_lastSendPos + nSendLen >= SEND_BUFF_SIZE) {
				int nCopyLen = SEND_BUFF_SIZE - _lastSendPos;				//����ɿ��������ݳ���
				memcpy(_sendMsgBuff + _lastSendPos, pSendData, nCopyLen);	//��������
				pSendData += nCopyLen;										//����ʣ������λ��
				nSendLen -= nSendLen;										//����ʣ�����ݳ���
				ret = send(_sockfd, _sendMsgBuff, SEND_BUFF_SIZE, 0);		//��������
				_lastSendPos = 0;											//����β��λ������
				//���ʹ���
				if (SOCKET_ERROR == ret) {
					return -1;
				}
			}
			//������ͻ�������û������ô��������Ϣ�ŵ��������У�����ֱ�ӷ���
			else {
				//��Ҫ���͵����� ���������ͻ�����β��
				memcpy(_sendMsgBuff + _lastSendPos, pSendData, nSendLen);
				//��������β��λ��
				_lastSendPos += nSendLen;
				break;
			}
		}
		return ret;
	}
};

//�����¼��ӿ�
class INetEvent {
public:
	//���麯��
	virtual void OnClientJoin(ClientSocket* pClient) = 0;					//�ͻ��˼����¼�
	virtual void OnClientLeave(ClientSocket* pClient) = 0;					//�ͻ����뿪�¼�
	virtual void OnNetMsg(CellServer* pCellServer, ClientSocket* pClient, DataHeader* header) = 0;	//�ͻ�����Ϣ�¼�
	virtual void OnNetRecv(ClientSocket* pClient) = 0;						//recv�¼�
};

//������Ϣ��������
class CellSendMsg2ClientTask :public CellTask {
private:
	ClientSocket* _pClient;	//���͸��ĸ��ͻ���
	DataHeader* _pHeader;	//Ҫ���͵����ݵ�ͷָ��
public:
	CellSendMsg2ClientTask(ClientSocket* pClient, DataHeader* header) : _pClient(pClient), _pHeader(header) {}
	void doTask();	//ִ������
};

void CellSendMsg2ClientTask::doTask() {
	_pClient->SendData(_pHeader);
	delete _pHeader;
}

//������Ϣ���մ��������
class CellServer {
public:
	CellServer(SOCKET sock = INVALID_SOCKET) :_sock(sock), _maxSock(_sock),_pthread(nullptr), _pNetEvent(nullptr), _clients_change(false) {
		//memset(_recvBuff, 0, sizeof(_recvBuff));
		memset(&_fdRead_bak, 0, sizeof(_fdRead_bak));
	}
	~CellServer() { Close(); }

public:
	bool isRun() { return _sock != INVALID_SOCKET; }	//�Ƿ�����
	bool OnRun();										//����������Ϣ
	int RecvData(ClientSocket* pClient);				//�������� ����ճ�� ��ְ�	
	void addClient(ClientSocket* pClient);				//���ͻ��˼��뵽�ͻ������ӻ��������
	void Start();										//������ǰ�����߳�
	void Close();										//�ر�Socket
	void setEventObj(INetEvent* event) { _pNetEvent = event; }					//�����¼����󣬴˴��󶨵���EasyTcpServer
	size_t getClientCount() { return _clients.size() + _clientsBuff.size(); }	//���ص�ǰ�ͻ��˵�����
	virtual void OnNetMsg(ClientSocket* pClient, DataHeader* header);			//��Ӧ������Ϣ
	void addSendTask(ClientSocket* pClient, DataHeader* header);
private:
	SOCKET							_sock;			//������׽���
	SOCKET							_maxSock;		//��ǰ�����ļ�������ֵ select�Ĳ���1Ҫʹ��
	fd_set							_fdRead_bak;	//���ݿͻ�socket fd_set
	bool							_clients_change;//�ͻ��б��Ƿ��б仯
	std::mutex						_mutex;			//������еĻ�����
	std::thread*					_pthread;		//��ǰ�ӷ����ִ�е��߳�
	INetEvent*						_pNetEvent;		//�����¼�����
	std::map<SOCKET, ClientSocket*>	_clients;		//�����洢�ͻ���
	std::vector<ClientSocket*>		_clientsBuff;	//�洢�ͻ������ӻ������ ֮��ᱻ���뵽_clients��
	//char							_recvBuff[RECV_BUFF_SIZE] = {}; //���ջ�����
	CellTaskServer					_taskServer;	//ִ������
};

//�ر�Socket
void CellServer::Close() {
	if (_sock != INVALID_SOCKET) {
#ifdef _WIN32
		for (int n = (int)_clients.size() - 1; n >= 0; n--) {
			closesocket(_clients[n]->sockfd());
			delete _clients[n];
		}
		//�ر��׽���closesocket
		closesocket(_sock);
#else
		for (int n = (int)_clients.size() - 1; n >= 0; n--) {
			close(_clients[n]->sockfd());
			delete _clients[n];
		}
		//�ر��׽���closesocket
		close(_sock);
#endif
		_clients.clear();
		_sock = INVALID_SOCKET;
		delete _pthread;
	}
}

//����������Ϣ
bool CellServer::OnRun() {
	_clients_change = true;
	while (isRun()) {
		//�ӻ��������ȡ���¿ͻ�����
		if (_clientsBuff.size() > 0) {
			//�Խ���lock_guard��������������Զ��ͷ��������ifִ�н�����_mutex���ͷ���
			std::lock_guard<std::mutex> lock(_mutex);
			for (auto pClient : _clientsBuff) {
				_clients[pClient->sockfd()] = pClient;
			}
			_clientsBuff.clear();
			_clients_change = true;
		}

		//���û����Ҫ����Ŀͻ��ˣ�������
		if (_clients.empty()) {
			std::chrono::milliseconds t(1);
			std::this_thread::sleep_for(t);
			continue;
		}

		//�������׽��� BSD socket
		fd_set fdRead;		//��������socket�� ����
		FD_ZERO(&fdRead);	//������
		//����_clients_change�ж��Ƿ����¿ͻ��˼��룬�������ô�����µ�FD_SET
		if (_clients_change) {
			_clients_change = false;
			for (auto iter : _clients) {
				FD_SET(iter.second->sockfd(), &fdRead);
				if (_maxSock < iter.second->sockfd())
					_maxSock = iter.second->sockfd();
			}
			//�����º��fd_set���浽_fdRead_bak��
			memcpy(&_fdRead_bak, &fdRead, sizeof(fd_set));
		}
		else {
			//ֱ��ʹ�ñ���
			memcpy(&fdRead, &_fdRead_bak, sizeof(fd_set));
		}

		///nfds ��һ������ֵ ��ָfd_set����������������(socket)�ķ�Χ������������
		///���������ļ����������ֵ+1 ��Windows�������������д0
		int ret = select(_maxSock + 1, &fdRead, nullptr, nullptr, nullptr); //�ӷ�����һ��ֻ�����������ݣ�����������Ϊ����Ҳ����
		if (ret < 0) {
			printf("Server: select���������\n");
			Close();
			return false;
		}

#ifdef _WIN32
		//�����windows���� fd_setӵ��fd_count��fd_array��Ա
		//���ǿ��Ա���fd_set Ȼ����л�ȡ���� ����Ҫʹ��FD_ISSET
		for (int n = 0; n < fdRead.fd_count; n++) {
			auto iter = _clients.find(fdRead.fd_array[n]);
			//���RecvData���� ��ô�ͽ��ÿͻ��˴�_client���Ƴ�
			if (-1 == RecvData(iter->second)) {
				if (_pNetEvent)
					_pNetEvent->OnClientLeave(iter->second); //֪ͨ���������пͻ����˳�
				delete iter->second;
				_clients.erase(iter->first);
				_clients_change = true; //�пͻ����˳����½���FD_SET
			}
		}
#else
		//�����unix�� fd_set��fd_count��fd_array��Ա ����ֻ�ܱ���_clients����
		for (auto iter : _clients) {
			//��Ϊ_clients��map ���ÿ��iter����һ��pair ��first��ԱΪSOCKET second��ԱΪClientSocket
			if (FD_ISSET(iter.second->sockfd(), &fdRead)) {
				if (-1 == RecvData(iter.second)) {
					if (_pNetEvent)
						_pNetEvent->OnClientLeave(iter.second); //֪ͨ���������пͻ����˳�
					delete iter.second;
					_clients.erase(iter.first);
					_clients_change = true;
				}
			}
		}
#endif // _WIN32
	}
	return false;
}

//�������� ����ճ�� ��ְ�
int CellServer::RecvData(ClientSocket* pClient) {
	char* _recvBuff = pClient->recvMsgBuff() + pClient->getRecvLastPos();
	int nLen = (int)recv(pClient->sockfd(), _recvBuff, RECV_BUFF_SIZE - pClient->getRecvLastPos(), 0);	// ���տͻ�������
	_pNetEvent->OnNetRecv(pClient);

	if (nLen <= 0) {
		printf("�ͻ���<Socket=%d>���˳������������\n", pClient->sockfd());
		return -1;
	}

	//����ȡ�������ݿ�������Ϣ������
	//memcpy(pClient->_recvBuff() + pClient->getLastPos(), _recvBuff, nLen);

	//��Ϣ������������β��λ�ú���
	pClient->setRecvLastPos(pClient->getRecvLastPos() + nLen);

	//�ж���Ϣ�����������ݳ��ȴ�����ϢͷDataHeader����
	while (pClient->getRecvLastPos() >= sizeof(DataHeader)) {
		//��ʱ�Ϳ���֪����ǰ��Ϣ�ĳ���
		DataHeader* header = (DataHeader*)pClient->recvMsgBuff();
		//�ж���Ϣ�����������ݳ��ȴ�����Ϣ����
		if (pClient->getRecvLastPos() >= header->dataLength) {
			//��Ϣ������ʣ��δ�������ݵĳ���
			int nSize = pClient->getRecvLastPos() - header->dataLength;
			//����������Ϣ
			OnNetMsg(pClient, header);
			//����Ϣ������ʣ��δ��������ǰ��
			memcpy(pClient->recvMsgBuff(), pClient->recvMsgBuff() + header->dataLength, nSize);
			//��Ϣ������������β��λ��ǰ��
			pClient->setRecvLastPos(nSize);
		}
		else {
			//��Ϣ������ʣ�����ݲ���һ��������Ϣ
			break;
		}
	}
	return 0;
}

//��Ӧ������Ϣ
void CellServer::OnNetMsg(ClientSocket* pClient, DataHeader* header) {
	_pNetEvent->OnNetMsg(this, pClient, header);
}

//���ͻ��˼��뵽�ͻ������ӻ��������
void CellServer::addClient(ClientSocket* pClient) {
	//ʹ���Խ���
	std::lock_guard<std::mutex> lock(_mutex);
	//_mutex.lock(); ʹ�û�����
	_clientsBuff.push_back(pClient);
	//_mutex.unlock();
}
	
//������ǰ�����߳�
void CellServer::Start() {
	//����һ���̣߳��߳�ִ�к���ΪOnrun()����ʵ���Բ�����this������Ϊ�˸���ȫ�����Դ���this��Onrun()
	_pthread = new std::thread(std::mem_fn(&CellServer::OnRun), this);
	//�����������
	_taskServer.Start();
}

//
void CellServer::addSendTask(ClientSocket* pClient, DataHeader* header) {
	CellSendMsg2ClientTask* task = new CellSendMsg2ClientTask(pClient, header);
	_taskServer.addTask(task);
}

class EasyTcpServer : public INetEvent {
public:
	EasyTcpServer() :_sock(INVALID_SOCKET), _msgCount(0), _recvCount(0), _clientCount(0) {}
	virtual ~EasyTcpServer() { Close(); }
public:
	SOCKET InitSocket();								//��ʼ��Socket
	int Bind(const char* ip, unsigned short port);		//��ip�Ͷ˿ں�
	int Listen(int n);									//�����˿ں�
	SOCKET Accept();									//���ܿͻ�������
	void addClientToCellServer(ClientSocket* pClient);	//���¿ͻ����뵽CellServer�Ŀͻ������ӻ��������
	void Start(int nCellServer);						//�����ӷ����������������еĴӷ�������(����Ϊ�ӷ�����������)
	void Close();										//�ر�Socket
	bool isRun() { return _sock != INVALID_SOCKET; }	//�Ƿ�����
	bool OnRun();										//����������Ϣ
	void time4msg();									//���㲢���ÿ���յ���������Ϣ
	//�ͻ��˼����¼�(������̰߳�ȫ�ģ���Ϊ��ֻ�ᱻ��������(�Լ�)����)
	virtual void OnClientJoin(ClientSocket* pClient) {
		_clientCount++;
		//std::cout << "client<" << pClient->sockfd() << "> join" << std::endl;
	}
	//cellserver 4 ����̴߳��� ����ȫ ���ֻ����1��cellServer���ǰ�ȫ��
	virtual void OnClientLeave(ClientSocket* pClient) {
		_clientCount--;
		//std::cout << "client<" << pClient->sockfd() << "> leave" << std::endl;
	}
	//cellserver 4 ����̴߳��� ����ȫ ���ֻ����1��cellServer���ǰ�ȫ��
	virtual void OnNetMsg(CellServer* pCellServer, ClientSocket* pClient, DataHeader* header) { _msgCount++; }
	//recv�¼�
	virtual void OnNetRecv(ClientSocket* pClient) { _recvCount++; }
private:
	SOCKET					 _sock;			//������׽���
	std::vector<CellServer*> _cellServers;	//��Ϣ��������ڲ��ᴴ���߳�
	CELLTimestamp			 _tTime;		//ÿ����Ϣ��ʱ
	std::atomic_int			_recvCount;		//�յ���Ϣ����recv()ִ�д���
	std::atomic_int			_clientCount;	//�ͻ��˼���
	std::atomic_int			_msgCount;		//��ʾ����˽��յ��ͻ������ݰ�������
};

//��ʼ��Socket
SOCKET EasyTcpServer::InitSocket() {
#ifdef _WIN32
	//����Windows socket 2.x����
	WORD ver = MAKEWORD(2, 2);
	WSADATA dat;
	WSAStartup(ver, &dat);
#endif

	if (INVALID_SOCKET != _sock) {
		printf("<socket=%d>�رվ�����...\n", (int)_sock);
		Close();
	}

	_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (INVALID_SOCKET == _sock) {
		printf("���󣬽���socketʧ��...\n");
	}
	else {
		printf("����socket=<%d>�ɹ�...\n", (int)_sock);
	}
	return _sock;
}

//��ip�Ͷ˿ں�
int EasyTcpServer::Bind(const char* ip, unsigned short port) {
	if (INVALID_SOCKET == _sock) {
		InitSocket();
	};

	sockaddr_in _sin = {};
	_sin.sin_family = AF_INET;
	_sin.sin_port = htons(port); //host to a net unsigned short
#ifdef _WIN32
	if (ip) {
		_sin.sin_addr.S_un.S_addr = inet_addr(ip);
	}
	else {
		_sin.sin_addr.S_un.S_addr = INADDR_ANY; //INADDR_ANY��ʾ���Ȿ��������ip��ַ
	}
#else
	if (ip) {
		_sin.sin_addr.s_addr = inet_addr(ip);
	}
	else {
		_sin.sin_addr.s_addr = INADDR_ANY;
	}
#endif

	int ret = bind(_sock, (sockaddr*)&_sin, sizeof(_sin));
	if (SOCKET_ERROR == ret) {
		if (ip)
			std::cout << "Server���󶨵�ַ(" << ip << "," << port << ")ʧ��!" << std::endl;
		else
			std::cout << "Server���󶨵�ַ(INADDR_ANY," << port << ")ʧ��!" << std::endl;
	}
	else {
		if (ip)
			std::cout << "Server���󶨵�ַ(" << ip << "," << port << ")�ɹ�!" << std::endl;
		else
			std::cout << "Server���󶨵�ַ(INADDR_ANY," << port << ")�ɹ�!" << std::endl;
	}
	return ret;
}

//�����˿ں�
int EasyTcpServer::Listen(int n) {
	//listen ��������˿�
	int ret = listen(_sock, n);
	if (SOCKET_ERROR == ret) {
		printf("socket=<%d>����,��������˿�ʧ��...\n", _sock);
	}
	else {
		printf("socket=<%d>��������˿ڳɹ�...\n", _sock);
	}
	return ret;
}

//���ܿͻ�������
SOCKET EasyTcpServer::Accept() {
	//accept �ȴ����ܿͻ�������
	sockaddr_in clientAddr = {};
	int nAddrLen = sizeof(sockaddr_in);
	SOCKET _cSock = INVALID_SOCKET;
#ifdef _WIN32
	_cSock = accept(_sock, (sockaddr*)&clientAddr, &nAddrLen);
#else
	_cSock = accept(_sock, (sockaddr*)&clientAddr, (socklen_t*)&nAddrLen);
#endif
	if (INVALID_SOCKET == _cSock) {
		printf("socket=<%d>����,���ܵ���Ч�ͻ���SOCKET...\n", (int)_sock);
	}
	else {
		//���¿ͻ��˷�����ͻ��������ٵ�cellserver
		addClientToCellServer(new ClientSocket(_cSock));
		//��ȡIP��ַ inet_ntoa(clientAddr.sin_addr)
	}
	return _cSock;
}

//���¿ͻ����뵽CellServer�Ŀͻ������ӻ��������
void EasyTcpServer::addClientToCellServer(ClientSocket* pClient) {
	//���ҿͻ��������ٵ�CellServer��Ϣ�������
	auto pMinServer = _cellServers[0];
	for (auto pCellServer : _cellServers) {
		if (pMinServer->getClientCount() > pCellServer->getClientCount()) {
			pMinServer = pCellServer;
		}
	}
	pMinServer->addClient(pClient);
	OnClientJoin(pClient);
}

//�����ӷ����������������еĴӷ�������
void EasyTcpServer::Start(int nCellServer) {
	for (int n = 0; n < nCellServer; n++) {
		auto ser = new CellServer(_sock);
		_cellServers.push_back(ser);
		ser->setEventObj(this);		//ע�������¼����ܶ���
		ser->Start();				//������Ϣ�����߳�
	}
}

//�ر�Socket
void EasyTcpServer::Close() {
	if (_sock != INVALID_SOCKET) {
#ifdef _WIN32
		//�ر��׽���closesocket
		closesocket(_sock);
		//------------
		//���Windows socket����
		WSACleanup();
#else
		//�ر��׽���closesocket
		close(_sock);
#endif
	}
	_sock = INVALID_SOCKET;
}

//����������Ϣ
bool EasyTcpServer::OnRun() {
	if (isRun()) {
		time4msg();	//ͳ�Ƶ�ǰ���յ������ݰ�������
		//�������׽��� BSD socket
		fd_set fdRead;			//��������socket�� ����
		FD_ZERO(&fdRead);		//������
		FD_SET(_sock, &fdRead);	//����������socket�����뼯��
		///nfds ��һ������ֵ ��ָfd_set����������������(socket)�ķ�Χ������������
		///���������ļ����������ֵ+1 ��Windows�������������д0
		timeval t = { 0,0 };
		int ret = select(_sock + 1, &fdRead, nullptr, nullptr, &t);
		if (ret < 0) {
			printf("Accept Select���������\n");
			Close();
			return false;
		}
		//�ж���������socket���Ƿ��ڼ�����
		if (FD_ISSET(_sock, &fdRead)) {
			FD_CLR(_sock, &fdRead);
			Accept();
			return true;
		}
		return true;
	}
	return false;
}

//���㲢���ÿ���յ���������Ϣ
void EasyTcpServer::time4msg() {
	auto t1 = _tTime.getElapsedSecond();
	if (t1 >= 1.0) {
		printf("thread<%d>,time<%lf>,socket<%d>,clients<%d>,recv<%d>,msg<%d>\n", _cellServers.size(), t1, _sock, (int)_clientCount, (int)(_recvCount / t1), (int)(_msgCount / t1));
		_recvCount = 0;
		_msgCount = 0;
		_tTime.update();
	}
}

#endif // !_EasyTcpServer_hpp_
