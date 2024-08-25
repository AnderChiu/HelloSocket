#define WIN32_LEAN_AND_MEAN
#include <WinSock2.h>
#include <Windows.h>
#include <iostream>
#include <thread>

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

int process(SOCKET _cSock) {
    //缓冲区
    char szRecv[1024] = {};
    // 接收客户端数据
    int nLen = recv(_cSock, szRecv, sizeof(DataHeader), 0);
    DataHeader* header = (DataHeader*)szRecv;
    if (nLen <= 0) {
        std::cout << "与服务器断开连接，任务结束!" << std::endl;
        return -1;
    }

    switch (header->cmd) {
        case CMD_LOGIN_RET:
        {
            LoginResult* login = (LoginResult*)szRecv;
            recv(_cSock, szRecv + sizeof(DataHeader), header->dataLength - sizeof(DataHeader), 0);
            std::cout << "收到服务端消息: " << header->cmd << ", data_length: " << login->dataLength << std::endl;
        }
        break;
        case CMD_LOGOUT_RET:
        {
            LogoutResult* logout = (LogoutResult*)szRecv;
            recv(_cSock, szRecv + sizeof(DataHeader), header->dataLength - sizeof(DataHeader), 0);
            std::cout << "收到服务端消息: " << header->cmd << ", data_length: " << logout->dataLength << std::endl;
        }
        break;
        case CMD_NEW_USER_JOIN:
        {
            NewUserJoin* userJoin = (NewUserJoin*)szRecv;
            recv(_cSock, szRecv + sizeof(DataHeader), header->dataLength - sizeof(DataHeader), 0);
            std::cout << "收到服务端消息: " << header->cmd << ", data_length: " << userJoin->dataLength << std::endl;
        }
        break;
    }
}

bool g_bRun = true;
void cmdThread(SOCKET sock) {
    while (true) {
        char cmdBuf[256] = {};
        std::cin >> cmdBuf;
        if (strcmp(cmdBuf, "exit") == 0) {
            g_bRun = false;
            std::cout << "退出cmdThread线程" << std::endl;
            break;
        }
        else if (strcmp(cmdBuf, "login") == 0) {
            Login login;
            strcpy(login.userName, "zyz");
            strcpy(login.passWord, "zyzpass");
            send(sock, (const char*)&login, sizeof(Login), 0);
        }
        else if (strcmp(cmdBuf, "login") == 0) {
            Logout logout;
            strcpy(logout.userName, "zyz");
            send(sock, (const char*)&logout, sizeof(Logout), 0);
        }
        else {
            std::cout << "不支持的命令!" << std::endl;
        }
    }
}

int main()
{
    WORD ver = MAKEWORD(2, 2);
    WSADATA dat;
    WSAStartup(ver, &dat);

    // 1.build a SOCKET
    SOCKET _sock = socket(AF_INET, SOCK_STREAM, 0);
    if (_sock == INVALID_SOCKET) {
        std::cout << "ERROR 绑定端口失败" << std::endl;
    }
    else {
        std::cout << "绑定端口成功" << std::endl;
    }
    // 2.connect
    sockaddr_in _sin = {};
    _sin.sin_family = AF_INET;
    _sin.sin_port = htons(4567);
    _sin.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");
    int ret = connect(_sock, (sockaddr*)&_sin, sizeof(sockaddr_in));
    if (ret == SOCKET_ERROR) {
        std::cout << "ERROR 连接服务器失败!" << std::endl;
    }
    else {
        std::cout << "连接服务器成功" << std::endl;
    }
    // 启动线程函数
    std::thread t1(cmdThread, _sock);
    t1.detach(); // 和主线程分离

    while (g_bRun) {
        fd_set fdReads;
        FD_ZERO(&fdReads);
        FD_SET(_sock, &fdReads);
        timeval t = { 1, 0 };
        int ret = select(_sock, &fdReads, 0, 0, &t);
        if (ret < 0) {
            std::cout << "select任务结束!" << std::endl;
            break;
        }
        if (FD_ISSET(_sock, &fdReads)) {
            FD_CLR(_sock, &fdReads);
            if (process(_sock) == -1) {
                std::cout << "select任务结束[process]!" << std::endl;
                break;
            }
        }
        // 线程thread


        //std::cout << "空闲时间处理其他业务!" << std::endl;

    }
    // 7.close SOCKET
    closesocket(_sock);

    WSACleanup();
    std::cout << "任务结束!!!" << std::endl;
    getchar();
    return 0;
}
