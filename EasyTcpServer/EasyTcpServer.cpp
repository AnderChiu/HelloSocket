#define WIN32_LEAN_AND_MEAN
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <WinSock2.h>
#include <Windows.h>
#include <iostream>
#include <vector>
#pragma comment(lib, "ws2_32.lib")

enum CMD {
    CMD_LOGIN, CMD_LOGIN_RET, CMD_LOGOUT, CMD_LOGOUT_RET, CMD_ERROR, CMD_NEW_USER_JOIN
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

struct NewUserJoin : public DataHeader {
    NewUserJoin() {
        cmd = CMD_NEW_USER_JOIN;
        dataLength = sizeof(NewUserJoin);
        sock = 0;
    }
    int sock;
};


std::vector<SOCKET> g_clients;

int process(SOCKET _cSock) {
    //缓冲区
    char szRecv[1024] = {};
    // 接收客户端数据
    int nLen = recv(_cSock, szRecv, sizeof(DataHeader), 0);
    DataHeader* header = (DataHeader*)szRecv;
    if (nLen <= 0) {
        std::cout << "客户端结束!" << std::endl;
        return -1;
    }

    // 5.send
    switch (header->cmd) {
        case CMD_LOGIN:
        {
            Login* login = (Login*)szRecv;
            recv(_cSock, szRecv + sizeof(DataHeader), header->dataLength - sizeof(DataHeader), 0);
            std::cout << "收到CMD命令: " << header->cmd << ", data_length: " << header->dataLength << ", user=" << login->userName << ", passwd=" << login->passWord << std::endl;
            LoginResult ret;
            send(_cSock, (char*)&ret, sizeof(LoginResult), 0);
        }
        break;
        case CMD_LOGOUT:
        {
            Logout* logout = (Logout*)szRecv;
            recv(_cSock, szRecv + sizeof(DataHeader), header->dataLength - sizeof(DataHeader), 0);
            std::cout << "收到CMD命令: " << header->cmd << ", data_length: " << header->dataLength << ", user=" << logout->userName << std::endl;
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
        std::cout << "ERROR 绑定端口失败" << std::endl;
    }
    else {
        std::cout << "绑定端口成功" << std::endl;
    }
    // 3.listen to SOCKET
    if (listen(_sock, 5) == SOCKET_ERROR) {
        std::cout << "ERROR 监听端口失败!" << std::endl;
    }
    else {
        std::cout << "监听端口成功" << std::endl;
    }

    while (true) {
        // 伯克利套接字BSD socket
        fd_set fdRead;
        fd_set fdWrite;
        fd_set fdExp;
        // 清理集合
        FD_ZERO(&fdRead);
        FD_ZERO(&fdWrite);
        FD_ZERO(&fdExp);
        // 将描述符(socket)加入集合
        FD_SET(_sock, &fdRead);
        FD_SET(_sock, &fdWrite);
        FD_SET(_sock, &fdExp);

        for (int n = g_clients.size() - 1; n >= 0; n--) {
            FD_SET(g_clients[n], &fdRead);
        }

        // nfds是一个整数表示fd_set集合中所有描述符的范围而不是数量
        // 既是所有文件描述符的最大值+1 在windows中可写0
        timeval t = { 1, 0 };
        int ret = select(_sock + 1, &fdRead, &fdWrite, &fdExp, &t);
        if (ret < 0) {
            std::cout << "select任务结束!\n" << std::endl;
            break;
        }
        // 判断描述符是否在集合中
        if (FD_ISSET(_sock, &fdRead)) {
            FD_CLR(_sock, &fdRead);
            // 4.accept
            sockaddr_in clientAddr = {};
            int nAddrLen = sizeof(sockaddr_in);
            SOCKET _cSock = INVALID_SOCKET;
            _cSock = accept(_sock, (sockaddr*)&clientAddr, &nAddrLen);
            if (_cSock == INVALID_SOCKET) {
                std::cout << "ERROR 无效客户端SOCKET." << std::endl;
            }
            else {
                for (int n = g_clients.size() - 1; n >= 0; n--) {
                    NewUserJoin userJoin;
                    send(g_clients[n], (const char*)&userJoin, sizeof(NewUserJoin), 0);
                }
                g_clients.push_back(_cSock);
                std::cout << "新客户端加入: socket=" << _cSock << ", IP = " << inet_ntoa(clientAddr.sin_addr) << std::endl;
            }
        }

        for (int n = 0; n < fdRead.fd_count; n++) {
            if (process(fdRead.fd_array[n]) == -1) {
                auto iter = find(g_clients.begin(), g_clients.end(), fdRead.fd_array[n]);
                if (iter != g_clients.end())
                    g_clients.erase(iter);
            }
        }

        //std::cout << "空闲时间处理其他任务!" << std::endl;
    }
    // 6.close SOCKET
    for (int n = g_clients.size() - 1; n >= 0; n--) {
        closesocket(g_clients[n]);
    }
    
    // Clean Windows Socket ENV
    WSACleanup();
    std::cout << "任务结束!!!" << std::endl;
    getchar();
    return 0;
}
