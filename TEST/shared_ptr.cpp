#include <iostream>
#include <memory>

void fun(std::shared_ptr<int> sp) {
	std::cout << "fun: sp.use_count() == " << sp.use_count() << '\n';
}

void test(int* ptr) {
	std::shared_ptr<int> p1(ptr);
	int n = p1.use_count();
	std::cout << n << std::endl;
}

int main() {
	std::cout << (-1 <= 2 <= 3) << std::endl;

	auto sp1 = std::make_shared<int>(5);

	int* n3 = sp1.get();
	std::cout << *sp1 << '\n';;
	std::cout << "sp1.use_count() == " << sp1.use_count() << '\n';
	std::shared_ptr<int> p2(sp1);
	std::cout << "sp1.use_count() == " << sp1.use_count() << '\n';

	fun(sp1);
	fun(sp1);

	std::cout << "------------------" << std::endl;
	std::cout << std::endl;

	int* p1 = new int[3];
	memset(p1, 0, sizeof(int) * 3);

	*p1 = 11;
	*(p1 + 1) = 22;
	*(p1 + 2) = 33;

	std::shared_ptr<int> sp2(p1);
	int n = sp2.use_count();
	std::cout << n << std::endl;

	std::shared_ptr<int> ptr2(sp2);
	n = ptr2.use_count();
	std::cout << n << std::endl;

	std::shared_ptr<int> ptr3 = sp2;
	n = ptr3.use_count();
	std::cout << n << std::endl;

	//std::cout << sp2 << std::endl;
	std::cout << *(sp2.get()) << std::endl;
	std::cout << *(sp2.get() + 1) << std::endl;
	std::cout << *(sp2.get() + 2) << std::endl;
	return 0;
}
