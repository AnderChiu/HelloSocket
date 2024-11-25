#include <iostream>
#include <thread>
using namespace std;

void workFun() {
    for (int i = 0; i < 4; i++) {
        cout << "Hello, other thread." << endl;
    }
}

int main()
{
    thread t(workFun);
    t.detach();
    for (int i = 0; i < 4; i++) {
        std::cout << "Hello World!" << endl;
    }
    return 0;
}
