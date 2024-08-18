#define WIN32_LEAN_AND_MEAN
#include <WinSock2.h>
#include <Windows.h>
#include <iostream>

#pragma comment(lib, "ws2_32.lib")

enum CMD {
    CMD_LOGIN, CMD_LOGOUT, CMD_ERROR
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
        else if (strcmp(cmdBuf, "login") == 0) {
            Login login = { "zyz", "zyzpass" };
            DataHeader dh = { CMD_LOGIN,sizeof(login) };
            // 5.sned to Server
            send(_sock, (const char*)&dh, sizeof(dh), 0);
            send(_sock, (const char*)&login, sizeof(login), 0);
            // 接收服务器返回数据
            DataHeader retHeader = {};
            LoginResult loginRet = {};
            recv(_sock, (char*)&retHeader, sizeof(retHeader), 0);
            recv(_sock, (char*)&loginRet, sizeof(loginRet), 0);
            std::cout << "LoginResult: " << loginRet.result << std::endl;
        }
        else if (strcmp(cmdBuf, "logout") == 0) {
            Logout logout = { "zyz"};
            DataHeader dh = { CMD_LOGOUT,sizeof(logout) };
            // 5.sned to Server
            send(_sock, (const char*)&dh, sizeof(dh), 0);
            send(_sock, (const char*)&logout, sizeof(logout), 0);
            // 接收服务器返回数据
            DataHeader retHeader = {};
            LogoutResult logoutRet = {};
            recv(_sock, (char*)&retHeader, sizeof(retHeader), 0);
            recv(_sock, (char*)&logoutRet, sizeof(logoutRet), 0);
            std::cout << "LogoutResult: " << logoutRet.result << std::endl;
        }
        else {
            std::cout << "不支持的命令，重写输入！" << std::endl;
        }
    }
    // 7.close SOCKET
    closesocket(_sock);

    WSACleanup();
    std::cout << "Task Over!!!" << std::endl;
    getchar();
    return 0;
}
