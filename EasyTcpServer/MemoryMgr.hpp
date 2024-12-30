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
*	64�ֽڳ� 128�ֽڳ� 256�ֽڳ�
*		1[**] 1[**] 1[**]
*		2[**] 2[**] 2[**]
*
*/

//��ǰ����MemoryAlloc��
class MemoryAlloc;

//�ڴ�� ��С��Ԫ
class MemoryBlock {
public:
	int nID;				//�ڴ����
	int nRef;				//���ô���
	MemoryAlloc* pAlloc;	//�������ڴ��(��)
	MemoryBlock* pNext;		//��һ���ַ
	bool bPool;				//�Ƿ����ڴ����
private:
	char c1;				//Ԥ�� �ڴ����
	char c2;
	char c3;
};
//32λϵͳ��ָ���Сλ4���ֽڣ�64λΪ8���ֽ�
//windowsϵͳ���ڴ����Ϊ8�����������ֽڣ���������һ���ڴ��Ĵ�СΪ32���ֽ�


//�ڴ��
class MemoryAlloc {
public:
	MemoryAlloc() :_pBuf(nullptr), _pHeader(nullptr), _nSize(0), _nBlockSize(0) {}
	~MemoryAlloc() {
		if (_pBuf)
			free(_pBuf);
	}
	void* mallocMemory(size_t nSize);	//�����ڴ�
	void freeMemory(void* p);			//�ͷ��ڴ�
	void initMemory();				//��ʼ��
protected:
	char* _pBuf;					//�ڴ�ص�ַ
	MemoryBlock* _pHeader;			//ͷ���ڴ浥Ԫ
	size_t _nSize;					//�ڴ浥Ԫ�Ĵ�С e.g.64Byte
	size_t _nBlockSize;				//�ڴ浥Ԫ������
	std::mutex _mutex;				//���߳���
};

//�ڴ�������ڴ�
void* MemoryAlloc::mallocMemory(size_t nSize) {
	std::lock_guard<std::mutex> lg(_mutex);
	//����ڴ��һ��ʼû�б������ַ
	if (!_pBuf) {
		initMemory();
	}
	MemoryBlock* pReturn = nullptr;
	//����ڴ��ʹ�����ˣ���ô_pHeaderӦ��ָ��ĳ���ڴ������nullptrβָ��
	//��ʱ����malloc��ϵͳ�����ڴ棬�������ǵ��ڴ�ؽ����ڴ�����
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
	//��Debugģʽ�����������Ϣ  �����ڴ��ַ+�ڴ����+�ڴ���С
	xPrintf("mallocMem: %llx, id=%d, size=%d\n", pReturn, pReturn->nID, nSize);
	return ((char*)pReturn + sizeof(MemoryBlock));	//����ʵ�ʿ��õ��ڴ� ȥ��block��С
}

//�ڴ���ͷ��ڴ�
void MemoryAlloc::freeMemory(void* pMem) {
	MemoryBlock* pBlock = (MemoryBlock*)((char*)pMem - sizeof(MemoryBlock));	//����ƫ���Ǽ�(������Ϣ��) ����ƫ���Ǽ�
	//xPrintf("freeMem: %llx, id=%d\n", pBlock, pBlock->nID);
	assert(1 == pBlock->nRef);

	//���ڴ�صĲ����ͷ�֮��ָ��ص��ڴ��ͷ��
	if (pBlock->bPool) {
		std::lock_guard<std::mutex> lg(_mutex);
		//ֻ�����ô�����һ ֻ�б����ô���Ϊ0ʱ����Ҫ�ͷ��ڴ�
		if (--pBlock->nRef != 0)
			return;
		//_pHeader�ǵ�һ����е��ڴ� ��_pBlock�ͷų�����pBlock���˵�һ������ڴ�
		pBlock->pNext = _pHeader;
		_pHeader = pBlock;
	}
	else {
		if (--pBlock->nRef != 0)
			return;
		free(pBlock);
	}
}

//�ڴ�س�ʼ��
void MemoryAlloc::initMemory() {
	xPrintf("initMemory:_nSize=%d,_nBlock=%d\n", _nSize, _nBlockSize);
	assert(nullptr == _pBuf);	//����

	//�ڴ��Ϊ�գ�����ʼ��ֱ�ӷ���
	if (_pBuf)
		return;

	//�����ڴ�ش�С=�ڴ������*���ڴ���С+�ڴ�������Ϣ�ڴ��С��
	size_t realSize = _nSize + sizeof(MemoryBlock);
	size_t bufSize = realSize * _nBlockSize;	//�����ڴ�ش�С
	_pBuf = (char*)malloc(bufSize);			//��ϵͳ����ص��ڴ�

	//��ʼ���ڴ��ͷָ��
	_pHeader = (MemoryBlock*)_pBuf;
	_pHeader->bPool = true;
	_pHeader->nID = 0;
	_pHeader->nRef = 0;
	_pHeader->pAlloc = this;
	_pHeader->pNext = nullptr;

	//�����ڴ�鲢��ʼ��
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


//������������Ա����ʱ��ʼ��memoryalloc�ĳ�Ա����
template<size_t nSize, size_t nBlockSize>
class MemoryAlloctor :public MemoryAlloc {
public:
	MemoryAlloctor() {
		const size_t n = sizeof(void*);
		_nSize = (nSize / n) * n + (nSize % n ? n : 0);	//���û�����Ĳ����ʵ��ֽ������ж��� 61->64
		_nBlockSize = nBlockSize;
	}
private:

};


//�ڴ������
class MemoryMgr {
private:
	MemoryMgr() {
		//��ʼ���ڴ��ӳ������_szAlloc��������һ���ڴ�ʱ������ֱ���ҵ��ڴ��С���ʵ��ڴ��
		//init_szAlloc�Ĺ�������ӳ�䣬��������ڴ�Ĵ�С��[0��64]��Χ�ڣ����ڿ��СΪ64���ڴ��ȥ����
		init_szAlloc(0, 64, &_mem64);
		init_szAlloc(65, 128, &_mem128);
		init_szAlloc(129, 256, &_mem256);
		init_szAlloc(257, 512, &_mem512);
		init_szAlloc(513, 1024, &_mem1024);
	}
public:
	static MemoryMgr& Instance();	//����ģʽ ���ؾ�̬���������
	void* mallocMem(size_t nSize);	//�����ڴ�
	void freeMem(void* p);			//�ͷ��ڴ�
	void addRef(void* pMem);		//��������
private:
	void init_szAlloc(int nBegin, int nEnd, MemoryAlloc* pMemA);	//�ڴ��ӳ��
private:
	MemoryAlloctor<64, 100000> _mem64;
	MemoryAlloctor<128, 100000> _mem128;
	MemoryAlloctor<256, 100000> _mem256;
	MemoryAlloctor<512, 100000> _mem512;
	MemoryAlloctor<1024, 100000> _mem1024;
	MemoryAlloc* _szAlloc[MAX_MEMORY_SIZE + 1];
};

//����ģʽ ���ؾ�̬���������
MemoryMgr& MemoryMgr::Instance() {
	//����ģʽ
	static MemoryMgr mgr;
	return mgr;
}

//�����������ڴ�
void* MemoryMgr::mallocMem(size_t nSize) {
	if (nSize <= MAX_MEMORY_SIZE) {
		return _szAlloc[nSize]->mallocMemory(nSize);
	}
	else {
		//û���㹻��С���ڴ�أ�ȥ���������ڴ�
		MemoryBlock* pReturn = (MemoryBlock*)malloc(nSize + sizeof(MemoryBlock));
		pReturn->bPool = false;
		pReturn->nID = -1;
		pReturn->nRef = 1;
		pReturn->pAlloc = nullptr;
		pReturn->pNext = nullptr;
		xPrintf("mallocMem: %llx, id=%d, size=%d\n", pReturn, pReturn->nID, nSize);
		return ((char*)pReturn + sizeof(MemoryBlock));	//����ʵ�ʿ��õ��ڴ� ȥ��block��С
	}
}

//�������ͷ��ڴ�
void MemoryMgr::freeMem(void* pMem) {
	MemoryBlock* pBlock = (MemoryBlock*)((char*)pMem - sizeof(MemoryBlock));
	xPrintf("freeMem: %llx, id=%d\n", pBlock, pBlock->nID);
	//�ͷ��ڴ��
	if (pBlock->bPool) {
		pBlock->pAlloc->freeMemory(pMem);
	}
	//�ͷŶ����ڴ�
	else {
		//ֻ������һ�ξ�ֱ���ͷ��ڴ棬�������������ʱ���ͷţ���Ϊ���������ط����ã�
		if (--pBlock->nRef == 0) {
			free(pBlock);
		}
	}
}

//�������ڴ��ӳ��
void MemoryMgr::init_szAlloc(int nBegin, int nEnd, MemoryAlloc* pMemA) {
	for (int i = nBegin; i <= nEnd; i++) {
		_szAlloc[i] = pMemA;
	}
}

//��������������
void MemoryMgr::addRef(void* pMem) {
	MemoryBlock* pBlock = (MemoryBlock*)((char*)pMem - sizeof(MemoryBlock));
	++pBlock->nRef;
}

#endif // !_MEMORYMGR_HPP_
