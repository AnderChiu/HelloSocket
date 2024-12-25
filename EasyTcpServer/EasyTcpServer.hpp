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
	//在Unix下没有这些宏，为了兼容，自己定义
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

//缓冲区最小单元大小
#ifndef RECV_BUFF_SIZE
#define RECV_BUFF_SIZE 10240*5			//接收缓冲区的大小
#define SEND_BUFF_SIZE RECV_BUFF_SIZE	//发送缓冲区的大小
#endif // RECV_BUFF_SIZE
//#define _CellServer_THREAD_COUNT 4

class CellServer;

//客户端数据类型
class ClientSocket {
private:
	SOCKET	_sockfd;						//客户端socket
	char	_recvMsgBuff[RECV_BUFF_SIZE];	//消息接收缓冲区
	int		_lastRecvPos;					//消息接收缓冲区的数据尾部位置
	char	_sendMsgBuff[RECV_BUFF_SIZE];	//消息发送缓冲区
	int		_lastSendPos;					//消息发送缓冲区的数据尾部位置
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

	//发送数据
	int SendData(DataHeader* header) {
		int ret = SOCKET_ERROR;
		int nSendLen = header->dataLength;				//要发送的数据长度
		const char* pSendData = (const char*)header;	//要发送的数据
		
		//在下面第一个if之后，如果nSendLen仍然大于SEND_BUFF_SIZE，那么就需要继续执行if，继续发送数据
		while (true) {
			//如果当前"要发送的数据的长度+缓冲区数据结尾位置"之和总的缓冲区大小，说明缓冲区满了，那么将整个缓冲区都发送出去
			if (_lastSendPos + nSendLen >= SEND_BUFF_SIZE) {
				int nCopyLen = SEND_BUFF_SIZE - _lastSendPos;				//计算可拷贝的数据长度
				memcpy(_sendMsgBuff + _lastSendPos, pSendData, nCopyLen);	//拷贝数据
				pSendData += nCopyLen;										//计算剩余数据位置
				nSendLen -= nSendLen;										//计算剩余数据长度
				ret = send(_sockfd, _sendMsgBuff, SEND_BUFF_SIZE, 0);		//发送数据
				_lastSendPos = 0;											//数据尾部位置清零
				//发送错误
				if (SOCKET_ERROR == ret) {
					return -1;
				}
			}
			//如果发送缓冲区还没满，那么将这条消息放到缓冲区中，而不直接发送
			else {
				//将要发送的数据 拷贝到发送缓冲区尾部
				memcpy(_sendMsgBuff + _lastSendPos, pSendData, nSendLen);
				//计算数据尾部位置
				_lastSendPos += nSendLen;
				break;
			}
		}
		return ret;
	}
};

//网络事件接口
class INetEvent {
public:
	//纯虚函数
	virtual void OnClientJoin(ClientSocket* pClient) = 0;					//客户端加入事件
	virtual void OnClientLeave(ClientSocket* pClient) = 0;					//客户端离开事件
	virtual void OnNetMsg(CellServer* pCellServer, ClientSocket* pClient, DataHeader* header) = 0;	//客户端消息事件
	virtual void OnNetRecv(ClientSocket* pClient) = 0;						//recv事件
};

//网络消息发送任务
class CellSendMsg2ClientTask :public CellTask {
private:
	ClientSocket* _pClient;	//发送给哪个客户端
	DataHeader* _pHeader;	//要发送的数据的头指针
public:
	CellSendMsg2ClientTask(ClientSocket* pClient, DataHeader* header) : _pClient(pClient), _pHeader(header) {}
	void doTask();	//执行任务
};

void CellSendMsg2ClientTask::doTask() {
	_pClient->SendData(_pHeader);
	delete _pHeader;
}

//网络消息接收处理服务类
class CellServer {
public:
	CellServer(SOCKET sock = INVALID_SOCKET) :_sock(sock), _maxSock(_sock),_pthread(nullptr), _pNetEvent(nullptr), _clients_change(false) {
		//memset(_recvBuff, 0, sizeof(_recvBuff));
		memset(&_fdRead_bak, 0, sizeof(_fdRead_bak));
	}
	~CellServer() { Close(); }

public:
	bool isRun() { return _sock != INVALID_SOCKET; }	//是否工作中
	bool OnRun();										//处理网络消息
	int RecvData(ClientSocket* pClient);				//接收数据 处理粘包 拆分包	
	void addClient(ClientSocket* pClient);				//将客户端加入到客户端连接缓冲队列中
	void Start();										//启动当前服务线程
	void Close();										//关闭Socket
	void setEventObj(INetEvent* event) { _pNetEvent = event; }					//设置事件对象，此处绑定的是EasyTcpServer
	size_t getClientCount() { return _clients.size() + _clientsBuff.size(); }	//返回当前客户端的数量
	virtual void OnNetMsg(ClientSocket* pClient, DataHeader* header);			//响应网络消息
	void addSendTask(ClientSocket* pClient, DataHeader* header);
private:
	SOCKET							_sock;			//服务端套接字
	SOCKET							_maxSock;		//当前最大的文件描述符值 select的参数1要使用
	fd_set							_fdRead_bak;	//备份客户socket fd_set
	bool							_clients_change;//客户列表是否有变化
	std::mutex						_mutex;			//缓冲队列的互斥锁
	std::thread*					_pthread;		//当前子服务端执行的线程
	INetEvent*						_pNetEvent;		//网络事件对象
	std::map<SOCKET, ClientSocket*>	_clients;		//真正存储客户端
	std::vector<ClientSocket*>		_clientsBuff;	//存储客户端连接缓存队列 之后会被加入到_clients中
	//char							_recvBuff[RECV_BUFF_SIZE] = {}; //接收缓冲区
	CellTaskServer					_taskServer;	//执行任务
};

//关闭Socket
void CellServer::Close() {
	if (_sock != INVALID_SOCKET) {
#ifdef _WIN32
		for (int n = (int)_clients.size() - 1; n >= 0; n--) {
			closesocket(_clients[n]->sockfd());
			delete _clients[n];
		}
		//关闭套节字closesocket
		closesocket(_sock);
#else
		for (int n = (int)_clients.size() - 1; n >= 0; n--) {
			close(_clients[n]->sockfd());
			delete _clients[n];
		}
		//关闭套节字closesocket
		close(_sock);
#endif
		_clients.clear();
		_sock = INVALID_SOCKET;
		delete _pthread;
	}
}

//处理网络消息
bool CellServer::OnRun() {
	_clients_change = true;
	while (isRun()) {
		//从缓冲队列里取出新客户数据
		if (_clientsBuff.size() > 0) {
			//自解锁lock_guard，作用域结束后自动释放锁，因此if执行结束后，_mutex就释放了
			std::lock_guard<std::mutex> lock(_mutex);
			for (auto pClient : _clientsBuff) {
				_clients[pClient->sockfd()] = pClient;
			}
			_clientsBuff.clear();
			_clients_change = true;
		}

		//如果没有需要处理的客户端，就跳过
		if (_clients.empty()) {
			std::chrono::milliseconds t(1);
			std::this_thread::sleep_for(t);
			continue;
		}

		//伯克利套接字 BSD socket
		fd_set fdRead;		//描述符（socket） 集合
		FD_ZERO(&fdRead);	//清理集合
		//根据_clients_change判断是否有新客户端加入，如果有那么进行新的FD_SET
		if (_clients_change) {
			_clients_change = false;
			for (auto iter : _clients) {
				FD_SET(iter.second->sockfd(), &fdRead);
				if (_maxSock < iter.second->sockfd())
					_maxSock = iter.second->sockfd();
			}
			//将更新后的fd_set保存到_fdRead_bak中
			memcpy(&_fdRead_bak, &fdRead, sizeof(fd_set));
		}
		else {
			//直接使用备份
			memcpy(&fdRead, &_fdRead_bak, sizeof(fd_set));
		}

		///nfds 是一个整数值 是指fd_set集合中所有描述符(socket)的范围，而不是数量
		///既是所有文件描述符最大值+1 在Windows中这个参数可以写0
		int ret = select(_maxSock + 1, &fdRead, nullptr, nullptr, nullptr); //从服务器一般只用来接收数据，故这里设置为阻塞也可以
		if (ret < 0) {
			printf("Server: select任务结束。\n");
			Close();
			return false;
		}

#ifdef _WIN32
		//如果是windows运行 fd_set拥有fd_count与fd_array成员
		//我们可以遍历fd_set 然后从中获取数据 不需要使用FD_ISSET
		for (int n = 0; n < fdRead.fd_count; n++) {
			auto iter = _clients.find(fdRead.fd_array[n]);
			//如果RecvData出错 那么就将该客户端从_client中移除
			if (-1 == RecvData(iter->second)) {
				if (_pNetEvent)
					_pNetEvent->OnClientLeave(iter->second); //通知主服务器有客户端退出
				delete iter->second;
				_clients.erase(iter->first);
				_clients_change = true; //有客户端退出重新进行FD_SET
			}
		}
#else
		//如果在unix下 fd_set无fd_count与fd_array成员 我们只能遍历_clients数组
		for (auto iter : _clients) {
			//因为_clients是map 因此每次iter返回一个pair 其first成员为SOCKET second成员为ClientSocket
			if (FD_ISSET(iter.second->sockfd(), &fdRead)) {
				if (-1 == RecvData(iter.second)) {
					if (_pNetEvent)
						_pNetEvent->OnClientLeave(iter.second); //通知主服务器有客户端退出
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

//接收数据 处理粘包 拆分包
int CellServer::RecvData(ClientSocket* pClient) {
	char* _recvBuff = pClient->recvMsgBuff() + pClient->getRecvLastPos();
	int nLen = (int)recv(pClient->sockfd(), _recvBuff, RECV_BUFF_SIZE - pClient->getRecvLastPos(), 0);	// 接收客户端数据
	_pNetEvent->OnNetRecv(pClient);

	if (nLen <= 0) {
		printf("客户端<Socket=%d>已退出，任务结束。\n", pClient->sockfd());
		return -1;
	}

	//将收取到的数据拷贝到消息缓冲区
	//memcpy(pClient->_recvBuff() + pClient->getLastPos(), _recvBuff, nLen);

	//消息缓冲区的数据尾部位置后移
	pClient->setRecvLastPos(pClient->getRecvLastPos() + nLen);

	//判断消息缓冲区的数据长度大于消息头DataHeader长度
	while (pClient->getRecvLastPos() >= sizeof(DataHeader)) {
		//这时就可以知道当前消息的长度
		DataHeader* header = (DataHeader*)pClient->recvMsgBuff();
		//判断消息缓冲区的数据长度大于消息长度
		if (pClient->getRecvLastPos() >= header->dataLength) {
			//消息缓冲区剩余未处理数据的长度
			int nSize = pClient->getRecvLastPos() - header->dataLength;
			//处理网络消息
			OnNetMsg(pClient, header);
			//将消息缓冲区剩余未处理数据前移
			memcpy(pClient->recvMsgBuff(), pClient->recvMsgBuff() + header->dataLength, nSize);
			//消息缓冲区的数据尾部位置前移
			pClient->setRecvLastPos(nSize);
		}
		else {
			//消息缓冲区剩余数据不够一条完整消息
			break;
		}
	}
	return 0;
}

//响应网络消息
void CellServer::OnNetMsg(ClientSocket* pClient, DataHeader* header) {
	_pNetEvent->OnNetMsg(this, pClient, header);
}

//将客户端加入到客户端连接缓冲队列中
void CellServer::addClient(ClientSocket* pClient) {
	//使用自解锁
	std::lock_guard<std::mutex> lock(_mutex);
	//_mutex.lock(); 使用互斥锁
	_clientsBuff.push_back(pClient);
	//_mutex.unlock();
}
	
//启动当前服务线程
void CellServer::Start() {
	//创建一个线程，线程执行函数为Onrun()，其实可以不传递this，但是为了更安全，可以传递this给Onrun()
	_pthread = new std::thread(std::mem_fn(&CellServer::OnRun), this);
	//启动任务管理
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
	SOCKET InitSocket();								//初始化Socket
	int Bind(const char* ip, unsigned short port);		//绑定ip和端口号
	int Listen(int n);									//监听端口号
	SOCKET Accept();									//接受客户端连接
	void addClientToCellServer(ClientSocket* pClient);	//将新客户加入到CellServer的客户端连接缓冲队列中
	void Start(int nCellServer);						//创建从服务器，并运行所有的从服务器。(参数为从服务器的数量)
	void Close();										//关闭Socket
	bool isRun() { return _sock != INVALID_SOCKET; }	//是否工作中
	bool OnRun();										//处理网络消息
	void time4msg();									//计算并输出每秒收到的网络消息
	//客户端加入事件(这个是线程安全的，因为其只会被主服务器(自己)调用)
	virtual void OnClientJoin(ClientSocket* pClient) {
		_clientCount++;
		//std::cout << "client<" << pClient->sockfd() << "> join" << std::endl;
	}
	//cellserver 4 多个线程触发 不安全 如果只开启1个cellServer就是安全的
	virtual void OnClientLeave(ClientSocket* pClient) {
		_clientCount--;
		//std::cout << "client<" << pClient->sockfd() << "> leave" << std::endl;
	}
	//cellserver 4 多个线程触发 不安全 如果只开启1个cellServer就是安全的
	virtual void OnNetMsg(CellServer* pCellServer, ClientSocket* pClient, DataHeader* header) { _msgCount++; }
	//recv事件
	virtual void OnNetRecv(ClientSocket* pClient) { _recvCount++; }
private:
	SOCKET					 _sock;			//服务端套接字
	std::vector<CellServer*> _cellServers;	//消息处理对象，内部会创建线程
	CELLTimestamp			 _tTime;		//每秒消息计时
	std::atomic_int			_recvCount;		//收到消息计数recv()执行次数
	std::atomic_int			_clientCount;	//客户端计数
	std::atomic_int			_msgCount;		//表示服务端接收到客户端数据包的数量
};

//初始化Socket
SOCKET EasyTcpServer::InitSocket() {
#ifdef _WIN32
	//启动Windows socket 2.x环境
	WORD ver = MAKEWORD(2, 2);
	WSADATA dat;
	WSAStartup(ver, &dat);
#endif

	if (INVALID_SOCKET != _sock) {
		printf("<socket=%d>关闭旧连接...\n", (int)_sock);
		Close();
	}

	_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (INVALID_SOCKET == _sock) {
		printf("错误，建立socket失败...\n");
	}
	else {
		printf("建立socket=<%d>成功...\n", (int)_sock);
	}
	return _sock;
}

//绑定ip和端口号
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
		_sin.sin_addr.S_un.S_addr = INADDR_ANY; //INADDR_ANY表示任意本机的任意ip地址
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
			std::cout << "Server：绑定地址(" << ip << "," << port << ")失败!" << std::endl;
		else
			std::cout << "Server：绑定地址(INADDR_ANY," << port << ")失败!" << std::endl;
	}
	else {
		if (ip)
			std::cout << "Server：绑定地址(" << ip << "," << port << ")成功!" << std::endl;
		else
			std::cout << "Server：绑定地址(INADDR_ANY," << port << ")成功!" << std::endl;
	}
	return ret;
}

//监听端口号
int EasyTcpServer::Listen(int n) {
	//listen 监听网络端口
	int ret = listen(_sock, n);
	if (SOCKET_ERROR == ret) {
		printf("socket=<%d>错误,监听网络端口失败...\n", _sock);
	}
	else {
		printf("socket=<%d>监听网络端口成功...\n", _sock);
	}
	return ret;
}

//接受客户端连接
SOCKET EasyTcpServer::Accept() {
	//accept 等待接受客户端连接
	sockaddr_in clientAddr = {};
	int nAddrLen = sizeof(sockaddr_in);
	SOCKET _cSock = INVALID_SOCKET;
#ifdef _WIN32
	_cSock = accept(_sock, (sockaddr*)&clientAddr, &nAddrLen);
#else
	_cSock = accept(_sock, (sockaddr*)&clientAddr, (socklen_t*)&nAddrLen);
#endif
	if (INVALID_SOCKET == _cSock) {
		printf("socket=<%d>错误,接受到无效客户端SOCKET...\n", (int)_sock);
	}
	else {
		//将新客户端分配给客户数量最少的cellserver
		addClientToCellServer(new ClientSocket(_cSock));
		//获取IP地址 inet_ntoa(clientAddr.sin_addr)
	}
	return _cSock;
}

//将新客户加入到CellServer的客户端连接缓冲队列中
void EasyTcpServer::addClientToCellServer(ClientSocket* pClient) {
	//查找客户数量最少的CellServer消息处理对象
	auto pMinServer = _cellServers[0];
	for (auto pCellServer : _cellServers) {
		if (pMinServer->getClientCount() > pCellServer->getClientCount()) {
			pMinServer = pCellServer;
		}
	}
	pMinServer->addClient(pClient);
	OnClientJoin(pClient);
}

//创建从服务器，并运行所有的从服务器。
void EasyTcpServer::Start(int nCellServer) {
	for (int n = 0; n < nCellServer; n++) {
		auto ser = new CellServer(_sock);
		_cellServers.push_back(ser);
		ser->setEventObj(this);		//注册网络事件接受对象
		ser->Start();				//启动消息处理线程
	}
}

//关闭Socket
void EasyTcpServer::Close() {
	if (_sock != INVALID_SOCKET) {
#ifdef _WIN32
		//关闭套节字closesocket
		closesocket(_sock);
		//------------
		//清除Windows socket环境
		WSACleanup();
#else
		//关闭套节字closesocket
		close(_sock);
#endif
	}
	_sock = INVALID_SOCKET;
}

//处理网络消息
bool EasyTcpServer::OnRun() {
	if (isRun()) {
		time4msg();	//统计当前接收到的数据包的数量
		//伯克利套接字 BSD socket
		fd_set fdRead;			//描述符（socket） 集合
		FD_ZERO(&fdRead);		//清理集合
		FD_SET(_sock, &fdRead);	//将描述符（socket）加入集合
		///nfds 是一个整数值 是指fd_set集合中所有描述符(socket)的范围，而不是数量
		///既是所有文件描述符最大值+1 在Windows中这个参数可以写0
		timeval t = { 0,0 };
		int ret = select(_sock + 1, &fdRead, nullptr, nullptr, &t);
		if (ret < 0) {
			printf("Accept Select任务结束。\n");
			Close();
			return false;
		}
		//判断描述符（socket）是否在集合中
		if (FD_ISSET(_sock, &fdRead)) {
			FD_CLR(_sock, &fdRead);
			Accept();
			return true;
		}
		return true;
	}
	return false;
}

//计算并输出每秒收到的网络消息
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
