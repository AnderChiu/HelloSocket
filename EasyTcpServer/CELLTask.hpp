#ifndef _CELL_TASK_H_
#define _CELL_TASK_H_

#include<mutex>
#include<thread>
#include<list>
#include <functional>

//��������-����
class CellTask {
public:
	virtual void doTask() = 0;
};

//ִ������ķ�������
class CellTaskServer
{
public:
	void addTask(CellTask* task);	//�������
	void Start();					//���������߳�
protected:
	void OnRun();					//��������
private:
	std::list<CellTask*> _tasks;	//������������
	std::list<CellTask*> _tasksBuff;	//�������ݻ���������
	std::mutex _mutex;				//�ı����ݻ�����ʱ��Ҫ����
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
		//�ӻ�����ȡ������
		if (!_tasksBuff.empty()) {
			std::lock_guard<std::mutex> lock(_mutex);
			for (auto pTask : _tasksBuff) {
				_tasks.push_back(pTask);
			}
			_tasksBuff.clear();
		}
		
		//���û����������1ms
		if (_tasks.empty()) {
			std::chrono::microseconds t(1);
			std::this_thread::sleep_for(t);
			continue;
		}

		//��������
		for (auto pTask : _tasksBuff) {
			pTask->doTask();
			delete pTask;
		}
		_tasks.clear();
	}
}
#endif // !_CELL_TASK_H_
