#ifndef TEST_SUITE_H_
#define TEST_SUITE_H_

#include "vector"
#include <ctime>

template<typename L>
struct InsertToList {
	L &list;
	size_t num_elements;

	InsertToList(L &list, size_t num_elements = 1e5) :
		list(list), num_elements(num_elements) {}

	void operator() () const {
		typedef typename L::value_type value_type;
		for (size_t i = 0; i < num_elements; i++) {
			list.push_back(value_type());
		}
	}
};

template<typename L>
struct RandomDelete {
	typedef typename L::iterator iterator;
	L &list;
	unsigned seed;
	unsigned threshold;
	
	RandomDelete(L &list, unsigned seed = 0, float ratio = 0.2) :
		list(list), seed(seed), threshold(1024 * ratio) {}

	void operator() () const{
		srand(seed);
		for (iterator i = list.begin(); i != list.end();) {
			//删除链表中大于threshold的值
			if (rand() & 0x3ff < threshold) {
				i++;
			} else {
				i = list.erase(i);
			}
		}
	}
};

template<typename F>
float RuntimeOf(F const &func) {
	clock_t start = clock();
	func();
	return float(clock() - start) / CLOCKS_PER_SEC;
}

//存储插入用时和删除用时
typedef std::vector<std::pair<float, float>> test_result;

template<typename L>
test_result RunTestSuit(size_t num_loops = 3) {
	L list;
	test_result ret(num_loops);

	for (size_t i = 0; i < num_loops; i++) {
		//一大波数据插入
		ret[i].first = RuntimeOf(InsertToList<L>(list));
		//随机删除一部分
		ret[i].second = RuntimeOf(RandomDelete<L>(list));
	}

	return ret;
}

#endif