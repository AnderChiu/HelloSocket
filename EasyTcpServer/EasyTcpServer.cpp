#define WIN32_LEAN_AND_MEAN
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <WinSock2.h>
#include <Windows.h>
#include <iostream>

#pragma comment(lib, "ws2_32.lib")

enum CMD {
    CMD_LOGIN, CMD_LOGOUT,CMD_ERROR
};

struct DataHeader {
    short cmd;
    short dataLength;
};

// DataPackage
struct Login {
    char userName[32];
    char passWord[32];
};

struct LoginResult {
    int result;
};

struct Logout {
    char userName[32];
};

struct LogoutResult {
    int result;
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
    std::cout << "new Client join: socket=" << _cSock << ", IP = " << inet_ntoa(clientAddr.sin_addr) << std::endl;

    while (true) {
        DataHeader header = {};
        // 接收客户端数据
        int nLen = recv(_cSock, (char*)&header, sizeof(header), 0);
        if (nLen <= 0) {
            std::cout << "Client over!" << std::endl;
            break;
        }
        std::cout << "recv cmd: " << header.cmd << ", data_length: " << header.dataLength << std::endl;
        // 5.send
        switch (header.cmd) {
            case CMD_LOGIN:
            {
                Login login = {};
                recv(_cSock, (char*)&login, sizeof(Login), 0);
                LoginResult ret = { 1 };
                send(_cSock, (char*)&header, sizeof(DataHeader), 0);
                send(_cSock, (char*)&ret, sizeof(LoginResult), 0);
            }
            break;
            case CMD_LOGOUT:
            {
                Logout logout = {};
                recv(_cSock, (char*)&logout, sizeof(Logout), 0);
                LogoutResult ret = { 1 };
                send(_cSock, (char*)&header, sizeof(DataHeader), 0);
                send(_cSock, (char*)&ret, sizeof(LogoutResult), 0);
            }
            break;
            default:
            {
                header.cmd = CMD_ERROR;
                header.dataLength = 0;
                send(_cSock, (char*)&header, sizeof(header), 0);
            }
            break;
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
