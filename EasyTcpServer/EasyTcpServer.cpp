#define WIN32_LEAN_AND_MEAN
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <WinSock2.h>
#include <Windows.h>
#include <iostream>

#pragma comment(lib, "ws2_32.lib")

struct DataPackage {
    int age;
    char name[32];
};

int main()
{
    WORD ver = MAKEWORD(2, 2);
    WSADATA dat;
    WSAStartup(ver, &dat);

    // 1.build a socket
    SOCKET _sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    // 2.bind PORT
    sockaddr_in _sin = {};
    _sin.sin_family = AF_INET;
    _sin.sin_port = htons(4567); // host to net unsigned short
    _sin.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");
    if (bind(_sock, (sockaddr*)&_sin, sizeof(_sin)) == SOCKET_ERROR) {
        std::cout << "ERROR, bind PORT fail!" << std::endl;
    }
    else {
        std::cout << "bind PORT success" << std::endl;
    }
    // 3.listen to SOCKET
    if (listen(_sock, 5) == SOCKET_ERROR) {
        std::cout << "ERROR, listen PORT fail!" << std::endl;
    }
    else {
        std::cout << "listen PORT success" << std::endl;
    }
    // 4.accept
    sockaddr_in clientAddr = {};
    int nAddrLen = sizeof(sockaddr_in);
    SOCKET _cSock = INVALID_SOCKET;
    _cSock = accept(_sock, (sockaddr*)&clientAddr, &nAddrLen);
    if (_cSock == INVALID_SOCKET) {
        std::cout << "ERROR, INVALID Client SOCKET." << std::endl;
    }
    std::cout << "new Client join: socket=" << _cSock << "IP = " << inet_ntoa(clientAddr.sin_addr) << std::endl;
    char _recvBuf[128] = {};
    while (true) {
        int nLen = recv(_cSock, _recvBuf, 128, 0);
        if (nLen <= 0) {
            std::cout << "Client over!" << std::endl;
            break;
        }
        std::cout << "recv cmd: " << _recvBuf << std::endl;
        if (strcmp(_recvBuf, "getInfo") == 0) {
            DataPackage dp = { 24, "Ander" };
            send(_cSock, (const char*) & dp, sizeof(DataPackage), 0);
        }
        else {
            // 5.send
            char msgBuf[] = "???";
            send(_cSock, msgBuf, strlen(msgBuf) + 1, 0);
        }
    }
    // 6.close SOCKET
    closesocket(_sock);

    // Clean Windows Socket ENV
    WSACleanup();
    std::cout << "Task Over!!!" << std::endl;
    getchar();
    return 0;
}
