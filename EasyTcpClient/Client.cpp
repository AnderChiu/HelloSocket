#include "EasyTcpClient.hpp"
#include "CELLTimestamp.hpp"
#include <thread>
#include <atomic>
#include <iostream>

bool g_bRun = true;
const int cCount = 10000;		//客户端数量
const int tCount = 4;			//发送线程数量
EasyTcpClient* client[cCount];	//客户端数组
std::atomic_int sendCount = 0;	//send函数执行的次数
std::atomic_int readyCount = 0;	//已经准备就绪的线程数量

void cmdThread();
void sendThread(int id);

int main() {
	//启动UI线程
	std::thread t1(cmdThread);
	t1.detach();

	//启动发送线程
	for (int n = 0; n < tCount; n++) {
		std::thread t1(sendThread, n + 1);
		t1.detach();
	}

	//每秒打印一次信息 其中包括send()函数的执行次数
	CELLTimestamp tTime;
	while (g_bRun) {
		auto t = tTime.getElapsedSecond();
		if (t >= 1.0) {
			std::cout << "thread<" << tCount << ">,clients<" << cCount << ">,time<" << t << ">,send<" << (int)(sendCount / t) << ">" << std::endl;
			tTime.update();
			sendCount = 0;
		}
		Sleep(1);
	}

	printf("已退出。\n");
	return 0;
}

void cmdThread() {
	while (true) {
		char cmdBuf[256] = {};
		scanf("%s", cmdBuf);
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

void sendThread(int id) {
	printf("thread<%d>,start\n", id);
	//4个线程 ID 1~4 如对10000个客户端均分给4个线程
	int c = cCount / tCount;
	int begin = (id - 1) * c;
	int end = id * c;

	for (int n = begin; n < end; n++) {
		client[n] = new EasyTcpClient(); //创建客户端
	}
	for (int n = begin; n < end; n++) {
		client[n]->ConnectServer("127.0.0.1", 4567); //让每个客户端连接服务器
	}
	printf("thread<%d>,Connect<begin=%d, end=%d>\n", id, begin, end);

	readyCount++;
	//等待其他线程准备好发送数据 如果不是所有线程都就绪就等待都准备好再一起返回发送数据
	while (readyCount < tCount) {
		std::chrono::milliseconds t(10);
		std::this_thread::sleep_for(t);
	}

	//可以根据需求修改客户端单次发送给服务端的数据包数量
	Login login[1];
	for (int n = 0; n < 10; n++) {
		strcpy(login[n].userName, "lyd");
		strcpy(login[n].Password, "lydmm");
	}
	const int nLen = sizeof(login);
	while (g_bRun) {
		for (int n = begin; n < end; n++) {
			if (SOCKET_ERROR != client[n]->SendData(login, nLen)) {
				sendCount++;
			}
			client[n]->OnRun();
		}
	}

	//关闭客户端
	for (int n = begin; n < end; n++) {
		client[n]->CloseSocket();
		delete client[n];
	}

	printf("thread<%d>,exit\n", id);
}
