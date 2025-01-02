#include"Alloctor.h"
#include<stdlib.h>
#include<iostream>
#include<thread>
#include<mutex>
#include <memory>
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
}//抢占式

class ClassA {
public:
    ClassA() {
        cout << "ClassA构造函数" << endl;
    }
    ~ClassA() {
        cout << "ClassA析构函数" << endl;
    }

private:

};

int main() {
    //thread t[tCount];
    //for (int n = 0; n < tCount; n++) {
    //    t[n] = thread(workFun, n);
    //}
    //CELLTimestamp tTime;
    //for (int n = 0; n < tCount; n++) {
    //    t[n].join();
    //    //t[n].detach();
    //}
    //cout << tTime.getElapsedTimeInMilliSec() << endl;
    //cout << "Hello,main thread." << endl;

    //共享指针
    //std::shared_ptr<int> b = std::make_shared<int>();
    //*b = 100;
    //cout << "b=" << *b << endl;

    ClassA* a1 = new ClassA;
    //delete a1;

    system("pause");
    return 0;
}