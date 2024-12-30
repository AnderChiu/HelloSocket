#ifndef _ALLOCTOR_H_
#define _ALLOCTOR_H_

//重载函数声明
void* operator new(size_t size);
void operator delete(void* p);
void* operator new[](size_t size);
void operator delete[](void* p);

//给重载后的operator函数换一个函数名
void* mem_alloc(size_t size);
void mem_free(void* p);

#endif // !_ALLOCTOR_H_

