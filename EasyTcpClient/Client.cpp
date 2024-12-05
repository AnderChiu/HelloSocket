#include "EasyTcpClient.hpp"
#include "CELLTimestamp.hpp"
#include <thread>
#include <atomic>
#include <iostream>

bool g_bRun = true;
//客户端数量
const int cCount = 10000;
//发送线程数量
const int tCount = 4;
EasyTcpClient* client[cCount];
std::atomic_int sendCount = 0;
std::atomic_int readyCount = 0;

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
	//4个线程 ID 1~4
	int c = cCount / tCount;
	int begin = (id - 1) * c;
	int end = id * c;

	for (int n = begin; n < end; n++) {
		client[n] = new EasyTcpClient();
	}
	for (int n = begin; n < end; n++) {
		client[n]->Connect("127.0.0.1", 4567);
	}
	printf("thread<%d>,Connect<begin=%d, end=%d>\n", id, begin, end);

	readyCount++;
	//等待其他线程准备好发送数据
	while (readyCount < tCount) {
		std::chrono::milliseconds t(10);
		std::this_thread::sleep_for(t);
	}

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
			//client[n]->OnRun();
		}
	}

	for (int n = begin; n < end; n++) {
		client[n]->Close();
		delete client[n];
	}

	printf("thread<%d>,exit\n", id);
}

int main() {
	//启动UI线程
	std::thread t1(cmdThread);
	t1.detach();

	//启动发送线程
	for (int n = 0; n < tCount; n++) {
		std::thread t1(sendThread, n + 1);
		t1.detach();
	}

	CELLTimestamp tTime;
	while (g_bRun) {
		auto t = tTime.getElapsedSecond();
		if (t >= 1.0) {
			std::cout << "thread<" << tCount << ">,clients<" << cCount << ">,time<" << t << ">,send<" << sendCount << ">" << std::endl;
			tTime.update();
			sendCount = 0;
		}
		Sleep(1);
	}

	printf("已退出。\n");
	return 0;
}