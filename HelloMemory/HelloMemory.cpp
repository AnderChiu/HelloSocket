#include"Alloctor.h"
#include<stdlib.h>
#include<iostream>
#include<thread>
#include<mutex>
#include"CELLTimestamp.hpp"
using namespace std;

mutex m;                            //原子操作
const int tCount = 8;               //线程数
const int mCount = 100000;          //内存申请次数
const int nCount = mCount / tCount; //每个线程申请次数

void workFun(int index) {
    char* data[nCount];
    for (size_t i = 0; i < nCount; i++) {
        data[i] = new char[(rand() % 128) + 1];
    }
    for (size_t i = 0; i < nCount; i++) {
        delete[] data[i];
    }
    /*
    for (int n = 0; n < nCount; n++)
    {
        //自解锁
        //lock_guard<mutex> lg(m);
        //临界区域-开始
        //m.lock();

        //m.unlock();
        //临界区域-结束
    }//线程安全 线程不安全
     //原子操作 计算机处理命令时最小的操作单位
     */
}//抢占式

int main() {
    thread t[tCount];
    for (int n = 0; n < tCount; n++) {
        t[n] = thread(workFun, n);
    }
    CELLTimestamp tTime;
    for (int n = 0; n < tCount; n++) {
        t[n].join();
        //t[n].detach();
    }
    cout << tTime.getElapsedTimeInMilliSec() << endl;
    cout << "Hello,main thread." << endl;
    system("pause");
    return 0;
}