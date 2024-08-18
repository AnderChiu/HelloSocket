#define WIN32_LEAN_AND_MEAN
#include <WinSock2.h>
#include <Windows.h>
#include <iostream>

#pragma comment(lib, "ws2_32.lib")

enum CMD {
    CMD_LOGIN, CMD_LOGIN_RET, CMD_LOGOUT, CMD_LOGOUT_RET, CMD_ERROR
};

struct DataHeader {
    short cmd;
    short dataLength;
};

// DataPackage
struct Login : public DataHeader {
    Login() {
        cmd = CMD_LOGIN;
        dataLength = sizeof(Login);
    }
    char userName[32];
    char passWord[32];
};

struct LoginResult : public DataHeader {
    LoginResult() {
        cmd = CMD_LOGIN_RET;
        dataLength = sizeof(LoginResult);
        result = 0;
    }
    int result;
};

struct Logout : public DataHeader {
    Logout() {
        cmd = CMD_LOGOUT;
        dataLength = sizeof(Logout);
    }
    char userName[32];
};

struct LogoutResult : public DataHeader {
    LogoutResult() {
        cmd = CMD_LOGOUT_RET;
        dataLength = sizeof(LogoutResult);
        result = 0;
    }
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
            Login login;
            strcpy(login.userName, "zyz");
            strcpy(login.passWord, "zyzpass");
            // 5.sned to Server
            send(_sock, (const char*)&login, sizeof(login), 0);
            // 接收服务器返回数据
            LoginResult loginRet = {};
            recv(_sock, (char*)&loginRet, sizeof(loginRet), 0);
            std::cout << "LoginResult: " << loginRet.result << std::endl;
        }
        else if (strcmp(cmdBuf, "logout") == 0) {
            Logout logout;
            strcpy(logout.userName, "zyz");
            // 5.sned to Server
            send(_sock, (const char*)&logout, sizeof(logout), 0);
            // 接收服务器返回数据
            LogoutResult logoutRet = {};
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
