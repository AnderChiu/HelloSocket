#define WIN32_LEAN_AND_MEAN
#include <WinSock2.h>
#include <Windows.h>
#include <iostream>

#pragma comment(lib, "ws2_32.lib")
int main()
{
    WORD ver = MAKEWORD(2, 2);
    WSADATA dat;
    WSAStartup(ver, &dat);

    // 1.build a SOCKET
    SOCKET _sock = socket(AF_INET, SOCK_STREAM, 0);
    if (_sock == INVALID_SOCKET) {
        std::cout << "ERROR, build SOCKET fail!" << std::endl;
    }
    else {
        std::cout << "build SOCKET success" << std::endl;
    }
    // 2.connect
    sockaddr_in _sin = {};
    _sin.sin_family = AF_INET;
    _sin.sin_port = htons(4567);
    _sin.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");
    int ret = connect(_sock, (sockaddr*)&_sin, sizeof(sockaddr_in));
    if (ret == SOCKET_ERROR) {
        std::cout << "ERROR, connect PORT fail!" << std::endl;
    }
    else {
        std::cout << "connect PORT success" << std::endl;
    }

    while (true) {
        // 3.input cmd
        char cmdBuf[128] = {};
        std::cin >> cmdBuf;
        // 4.process the cmd
        if (strcmp(cmdBuf, "exit") == 0) {
            std::cout << "[exit] Task Over!!!" << std::endl;
            break;
        }
        else {
            // 5.sned to Server
            send(_sock, cmdBuf, strlen(cmdBuf) + 1, 0);
        }
        // 6.recv msg from server
        char recvBuf[256] = {};
        int nlen = recv(_sock, recvBuf, 256, 0);
        if (nlen > 0) {
            std::cout << "Recv data: " << recvBuf << std::endl;
        }
    }
    // 7.close SOCKET
    closesocket(_sock);

    WSACleanup();
    std::cout << "Task Over!!!" << std::endl;
    getchar();
    return 0;
}
