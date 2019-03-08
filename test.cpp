#include "PoolAllocator.hpp"
#include "TestSuite.hpp"
#include "StackAlloc.h"
#include <list>
#include <iostream>
#include <sstream>
#include "boost/pool/pool_alloc.hpp"

typedef std::list<size_t, pool::PoolAllocator<size_t>> pool_list;
typedef std::list<size_t, boost::fast_pool_allocator<size_t>> boost_list;
typedef std::list<size_t> normal_list;

#define ELEMS 100000
#define REPS 50

void PrintTestResult(test_result const &result) {
	std::ostringstream oss_insertion, oss_deletion;

	for (auto i = result.begin(); i != result.end(); i++) {
		oss_insertion << '\t' << i->first;
		oss_deletion << '\t' << i->second;
	}
	std::cout << "Insertion time:" << oss_deletion.str() << std::endl;
	std::cout << "Deletion time:" << oss_deletion.str() << std::endl;
}

void TestStack() {
	clock_t start;

	/* Use the default allocator */
	StackAlloc<int, std::allocator<int> > stackDefault;
	//StackAlloc<int, pool::PoolAllocator<int> > stackDefault;
	start = clock();
	for (int j = 0; j < REPS; j++)
	{
		assert(stackDefault.empty());
		for (int i = 0; i < ELEMS / 4; i++) {
			// Unroll to time the actual code and not the loop
			stackDefault.push(i);
			stackDefault.push(i);
			stackDefault.push(i);
			stackDefault.push(i);
		}
		for (int i = 0; i < ELEMS / 4; i++) {
			// Unroll to time the actual code and not the loop
			stackDefault.pop();
			stackDefault.pop();
			stackDefault.pop();
			stackDefault.pop();
		}
	}
	std::cout << "Default Allocator Time: ";
	std::cout << (((double)clock() - start) / CLOCKS_PER_SEC) << "\n\n";

	StackAlloc<int, pool::PoolAllocator<int> > stackPool;
	start = clock();
	for (int j = 0; j < REPS; j++)
	{
		assert(stackPool.empty());
		for (int i = 0; i < ELEMS / 4; i++) {
			// Unroll to time the actual code and not the loop
			stackPool.push(i);
			stackPool.push(i);
			stackPool.push(i);
			stackPool.push(i);
		}
		for (int i = 0; i < ELEMS / 4; i++) {
			// Unroll to time the actual code and not the loop
			stackPool.pop();
			stackPool.pop();
			stackPool.pop();
			stackPool.pop();
		}
	}
	std::cout << "MemoryPool Allocator Time: ";
	std::cout << (((double)clock() - start) / CLOCKS_PER_SEC) << "\n\n";

	StackAlloc<int, boost::fast_pool_allocator<int>> stackBoostPool;
	start = clock();
	for (int j = 0; j < REPS; j++)
	{
		assert(stackBoostPool.empty());
		for (int i = 0; i < ELEMS / 4; i++) {
			// Unroll to time the actual code and not the loop
			stackBoostPool.push(i);
			stackBoostPool.push(i);
			stackBoostPool.push(i);
			stackBoostPool.push(i);
		}
		for (int i = 0; i < ELEMS / 4; i++) {
			// Unroll to time the actual code and not the loop
			stackBoostPool.pop();
			stackBoostPool.pop();
			stackBoostPool.pop();
			stackBoostPool.pop();
		}
	}
	std::cout << "BoostPool Allocator Time: ";
	std::cout << (((double)clock() - start) / CLOCKS_PER_SEC) << "\n\n";
}



void TestList()
{
	std::cout << "============my pool list=============" << std::endl;
	PrintTestResult(RunTestSuit<pool_list>());

	std::cout << "============boost pool list=============" << std::endl;
	PrintTestResult(RunTestSuit<boost_list>());

	std::cout << "============normal pool list=============" << std::endl;
	PrintTestResult(RunTestSuit<normal_list>());
}

int main() {
	TestStack();
	TestList();

	return 0;
}