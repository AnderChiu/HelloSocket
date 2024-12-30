#ifndef _MEMORYMGR_HPP_
#define _MEMORYMGR_HPP_
#include <stdlib.h>
#include <assert.h>
#include <mutex>

#ifdef _DEBUG
#include <stdio.h>
#define xPrintf(...) printf(__VA_ARGS__)
#else
#define xPrintf(...)
#endif // _DEBUG

#define MAX_MEMORY_SIZE 1024
/*
*			operator new
*				|
*			MemoryMgr
*				|
*			MemoryAlloc
*			  / | \
*			 /  |  \
*		    /   |   \
*	64字节池 128字节池 256字节池
*		1[**] 1[**] 1[**]
*		2[**] 2[**] 2[**]
*
*/

//提前声明MemoryAlloc类
class MemoryAlloc;

//内存块 最小单元
class MemoryBlock {
public:
	int nID;				//内存块编号
	int nRef;				//引用次数
	MemoryAlloc* pAlloc;	//所属大内存块(池)
	MemoryBlock* pNext;		//下一块地址
	bool bPool;				//是否在内存池中
private:
	char c1;				//预留 内存对齐
	char c2;
	char c3;
};
//32位系统中指针大小位4个字节，64位为8个字节
//windows系统中内存分配为8的整数倍个字节，所有这里一个内存块的大小为32个字节


//内存池
class MemoryAlloc {
public:
	MemoryAlloc() :_pBuf(nullptr), _pHeader(nullptr), _nSize(0), _nBlockSize(0) {}
	~MemoryAlloc() {
		if (_pBuf)
			free(_pBuf);
	}
	void* mallocMemory(size_t nSize);	//申请内存
	void freeMemory(void* p);			//释放内存
	void initMemory();				//初始化
protected:
	char* _pBuf;					//内存池地址
	MemoryBlock* _pHeader;			//头部内存单元
	size_t _nSize;					//内存单元的大小 e.g.64Byte
	size_t _nBlockSize;				//内存单元的数量
	std::mutex _mutex;				//多线程锁
};

//内存池申请内存
void* MemoryAlloc::mallocMemory(size_t nSize) {
	std::lock_guard<std::mutex> lg(_mutex);
	//如果内存池一开始没有被分配地址
	if (!_pBuf) {
		initMemory();
	}
	MemoryBlock* pReturn = nullptr;
	//如果内存池使用完了，那么_pHeader应该指向某个内存块的最后nullptr尾指针
	//此时调用malloc向系统申请内存，不向我们的内存池进行内存申请
	if (nullptr == _pHeader) {
		pReturn = (MemoryBlock*)malloc(nSize + sizeof(MemoryBlock));
		pReturn->nID = -1;
		pReturn->nRef = 1;
		pReturn->bPool = false;
		pReturn->pAlloc = nullptr;
		pReturn->pNext = nullptr;
	}
	else {
		pReturn = _pHeader;
		_pHeader = _pHeader->pNext;
		assert(0 == pReturn->nRef);
		pReturn->nRef = 1;
	}
	//在Debug模式下输出调试信息  申请内存地址+内存块编号+内存块大小
	xPrintf("mallocMem: %llx, id=%d, size=%d\n", pReturn, pReturn->nID, nSize);
	return ((char*)pReturn + sizeof(MemoryBlock));	//返回实际可用的内存 去除block大小
}

//内存池释放内存
void MemoryAlloc::freeMemory(void* pMem) {
	MemoryBlock* pBlock = (MemoryBlock*)((char*)pMem - sizeof(MemoryBlock));	//向左偏移是减(加上信息块) 向右偏移是加
	//xPrintf("freeMem: %llx, id=%d\n", pBlock, pBlock->nID);
	assert(1 == pBlock->nRef);

	//在内存池的部分释放之后，指针回到内存池头部
	if (pBlock->bPool) {
		std::lock_guard<std::mutex> lg(_mutex);
		//只被引用次数减一 只有被引用次数为0时才需要释放内存
		if (--pBlock->nRef != 0)
			return;
		//_pHeader是第一块空闲的内存 当_pBlock释放出来后pBlock成了第一块可用内存
		pBlock->pNext = _pHeader;
		_pHeader = pBlock;
	}
	else {
		if (--pBlock->nRef != 0)
			return;
		free(pBlock);
	}
}

//内存池初始化
void MemoryAlloc::initMemory() {
	xPrintf("initMemory:_nSize=%d,_nBlock=%d\n", _nSize, _nBlockSize);
	assert(nullptr == _pBuf);	//断言

	//内存池为空，不初始化直接返回
	if (_pBuf)
		return;

	//计算内存池大小=内存块数量*（内存块大小+内存描述信息内存大小）
	size_t realSize = _nSize + sizeof(MemoryBlock);
	size_t bufSize = realSize * _nBlockSize;	//计算内存池大小
	_pBuf = (char*)malloc(bufSize);			//向系统申请池的内存

	//初始化内存块头指针
	_pHeader = (MemoryBlock*)_pBuf;
	_pHeader->bPool = true;
	_pHeader->nID = 0;
	_pHeader->nRef = 0;
	_pHeader->pAlloc = this;
	_pHeader->pNext = nullptr;

	//遍历内存块并初始化
	MemoryBlock* pTempPrev = _pHeader;
	for (size_t i = 1; i < _nBlockSize; i++) {
		MemoryBlock* pTemp = (MemoryBlock*)(_pBuf + i * realSize);
		pTemp->nID = i;
		pTemp->nRef = 0;
		pTemp->pAlloc = this;
		pTemp->bPool = true;
		pTemp->pNext = nullptr;
		pTempPrev->pNext = pTemp;
		pTempPrev = pTemp;
	}
}


//便于在声明成员变量时初始化memoryalloc的成员变量
template<size_t nSize, size_t nBlockSize>
class MemoryAlloctor :public MemoryAlloc {
public:
	MemoryAlloctor() {
		const size_t n = sizeof(void*);
		_nSize = (nSize / n) * n + (nSize % n ? n : 0);	//对用户输入的不合适的字节数进行对齐 61->64
		_nBlockSize = nBlockSize;
	}
private:

};


//内存管理工具
class MemoryMgr {
private:
	MemoryMgr() {
		//初始化内存池映射数组_szAlloc，当申请一个内存时，可以直接找到内存大小合适的内存池
		//init_szAlloc的功能类似映射，如果申请内存的大小在[0，64]范围内，就在块大小为64的内存池去申请
		init_szAlloc(0, 64, &_mem64);
		init_szAlloc(65, 128, &_mem128);
		init_szAlloc(129, 256, &_mem256);
		init_szAlloc(257, 512, &_mem512);
		init_szAlloc(513, 1024, &_mem1024);
	}
public:
	static MemoryMgr& Instance();	//单例模式 返回静态对象的引用
	void* mallocMem(size_t nSize);	//申请内存
	void freeMem(void* p);			//释放内存
	void addRef(void* pMem);		//增加引用
private:
	void init_szAlloc(int nBegin, int nEnd, MemoryAlloc* pMemA);	//内存池映射
private:
	MemoryAlloctor<64, 100000> _mem64;
	MemoryAlloctor<128, 100000> _mem128;
	MemoryAlloctor<256, 100000> _mem256;
	MemoryAlloctor<512, 100000> _mem512;
	MemoryAlloctor<1024, 100000> _mem1024;
	MemoryAlloc* _szAlloc[MAX_MEMORY_SIZE + 1];
};

//单例模式 返回静态对象的引用
MemoryMgr& MemoryMgr::Instance() {
	//单例模式
	static MemoryMgr mgr;
	return mgr;
}

//管理工具申请内存
void* MemoryMgr::mallocMem(size_t nSize) {
	if (nSize <= MAX_MEMORY_SIZE) {
		return _szAlloc[nSize]->mallocMemory(nSize);
	}
	else {
		//没有足够大小的内存池，去额外申请内存
		MemoryBlock* pReturn = (MemoryBlock*)malloc(nSize + sizeof(MemoryBlock));
		pReturn->bPool = false;
		pReturn->nID = -1;
		pReturn->nRef = 1;
		pReturn->pAlloc = nullptr;
		pReturn->pNext = nullptr;
		xPrintf("mallocMem: %llx, id=%d, size=%d\n", pReturn, pReturn->nID, nSize);
		return ((char*)pReturn + sizeof(MemoryBlock));	//返回实际可用的内存 去除block大小
	}
}

//管理工具释放内存
void MemoryMgr::freeMem(void* pMem) {
	MemoryBlock* pBlock = (MemoryBlock*)((char*)pMem - sizeof(MemoryBlock));
	xPrintf("freeMem: %llx, id=%d\n", pBlock, pBlock->nID);
	//释放内存池
	if (pBlock->bPool) {
		pBlock->pAlloc->freeMemory(pMem);
	}
	//释放额外内存
	else {
		//只被引用一次就直接释放内存，若被多个引用暂时不释放（因为还被其它地方引用）
		if (--pBlock->nRef == 0) {
			free(pBlock);
		}
	}
}

//管理工具内存池映射
void MemoryMgr::init_szAlloc(int nBegin, int nEnd, MemoryAlloc* pMemA) {
	for (int i = nBegin; i <= nEnd; i++) {
		_szAlloc[i] = pMemA;
	}
}

//管理工具增加引用
void MemoryMgr::addRef(void* pMem) {
	MemoryBlock* pBlock = (MemoryBlock*)((char*)pMem - sizeof(MemoryBlock));
	++pBlock->nRef;
}

#endif // !_MEMORYMGR_HPP_
