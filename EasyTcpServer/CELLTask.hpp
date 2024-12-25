#ifndef _CELL_TASK_H_
#define _CELL_TASK_H_

#include<mutex>
#include<thread>
#include<list>
#include <functional>

//任务类型-基类
class CellTask {
public:
	virtual void doTask() = 0;
};

//执行任务的服务类型
class CellTaskServer
{
public:
	void addTask(CellTask* task);	//添加任务
	void Start();					//启动工作线程
protected:
	void OnRun();					//工作函数
private:
	std::list<CellTask*> _tasks;	//任务数据链表
	std::list<CellTask*> _tasksBuff;	//任务数据缓冲区链表
	std::mutex _mutex;				//改变数据缓冲区时需要加锁
};

void  CellTaskServer::addTask(CellTask* task) {
	std::lock_guard<std::mutex> lock(_mutex);
	_tasksBuff.push_back(task);
}

void CellTaskServer::Start() {
	std::thread t(std::mem_fn(&CellTaskServer::OnRun), this);
	t.detach();
}

void CellTaskServer::OnRun() {
	while (true) {
		//从缓冲区取出数据
		if (!_tasksBuff.empty()) {
			std::lock_guard<std::mutex> lock(_mutex);
			for (auto pTask : _tasksBuff) {
				_tasks.push_back(pTask);
			}
			_tasksBuff.clear();
		}
		
		//如果没有任务休眠1ms
		if (_tasks.empty()) {
			std::chrono::microseconds t(1);
			std::this_thread::sleep_for(t);
			continue;
		}

		//处理任务
		for (auto pTask : _tasksBuff) {
			pTask->doTask();
			delete pTask;
		}
		_tasks.clear();
	}
}
#endif // !_CELL_TASK_H_
