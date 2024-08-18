#define WIN32_LEAN_AND_MEAN
#define _WINSOCK_DEPRECATED_NO_WARNINGS

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
        //缓冲区
        char szRecv[1024] = {};
        // 接收客户端数据
        int nLen = recv(_cSock, szRecv, sizeof(DataHeader), 0);
        DataHeader* header = (DataHeader*)szRecv;
        if (nLen <= 0) {
            std::cout << "Client over!" << std::endl;
            break;
        }
        
        // 5.send
        switch (header->cmd) {
            case CMD_LOGIN:
            {
                Login* login = (Login*)szRecv;
                recv(_cSock, szRecv + sizeof(DataHeader), header->dataLength - sizeof(DataHeader), 0);
                std::cout << "recv cmd: " << header->cmd << ", data_length: " << header->dataLength << ", user=" << login->userName << ", passwd=" << login->passWord << std::endl;
                LoginResult ret;
                send(_cSock, (char*)&ret, sizeof(LoginResult), 0);
            }
            break;
            case CMD_LOGOUT:
            {
                Logout* logout = (Logout*)szRecv;
                recv(_cSock, szRecv + sizeof(DataHeader), header->dataLength - sizeof(DataHeader), 0);
                std::cout << "recv cmd: " << header->cmd << ", data_length: " << header->dataLength << ", user=" << logout->userName << std::endl;
                LogoutResult ret;
                send(_cSock, (char*)&ret, sizeof(LogoutResult), 0);
            }
            break;
            default:
            {
                DataHeader header = { CMD_ERROR , 0 };
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
