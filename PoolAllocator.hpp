#ifndef POOL_ALLOCATOR_H_
#define POOL_ALLOCATOR_H_

#include "ObjectPool.hpp"
#include <new>
#include <memory>

#ifndef POOL_ALLOCATOR_SIZE_STEP
#define POOL_ALLOCATOR_SIZE_STEP 8	
#endif

#ifndef POOL_ALLOCATOR_POOL_SIZE
#define POOL_ALLOCATOR_POOL_SIZE 64	
#endif

namespace pool {

//池分配器基类，确保不同类型的分配器使用同一个池
class PoolAllocatorBase {
public:
	static const size_t align = POOL_ALLOCATOR_SIZE_STEP;
	static const size_t pool_size = POOL_ALLOCATOR_POOL_SIZE;
	static const size_t max_size = align*pool_size;
	static ObjectPoolArray pool;

	//@brief 根据节点大小选择合适节点大小的池
	//@param n 是需要分配的节点的大小，按照字节
	void *BaseAllocate(size_t n) throw (std::bad_alloc) {
		if (n == 0) {
			throw std::bad_alloc();
		} else if (n > max_size) {
			return static_cast<void*>(operator new (n));
		} else {
			//内存池数组中只有合适节点大小的池分配内存
			return pool[(n + align - 1) / align - 1].allocate();
		}
	}

	void BaseDeallocate(void *ptr, size_t n) {
		if (n > max_size) {
			operator delete(ptr);
		} else {
			pool[(n + align - 1) / align - 1].deallocate(ptr);
		}
	}

};

template<typename T>
class PoolAllocator:private PoolAllocatorBase {
public:
	typedef size_t size_type;
	typedef ptrdiff_t difference_type;
	typedef T* pointer;
	typedef const T* const_pointer;
	typedef T& reference;
	typedef const T& const_reference;
	typedef T value_type;

	template<typename U>
	struct rebind {
		typedef PoolAllocator<U> other;
	};

	PoolAllocator() throw() {}
	PoolAllocator(PoolAllocator const &) throw() {}
	template<typename U>
	PoolAllocator(PoolAllocator<U> const &) throw() {}
	~PoolAllocator() throw() {}

	pointer address(reference r) const { return &r; }
	const_pointer address(const_reference r) const { return &r; }

	size_type max_size() const throw() { return size_t(-1) / sizeof(T); }

	//@brief 构建函数，用于实例化已经分配的内存
	void construct(pointer ptr, const_reference v) {
		new ((void*)ptr) T(v);
	}

	//@brief 只是析构存储的元素，如int或一些类，不释放内存
	void destroy(pointer ptr) {
		ptr->~T();
	}

	//@brief 调用base类中的分配函数来分配内存
	//@param n 分配的元素个数
	pointer allocate(size_type n, const void * = 0) throw(std::bad_alloc) {
		return static_cast<pointer>(BaseAllocate(n * sizeof(T)));
	}

	//@brief 调用base类中的释放函数来释放内存
	//@param ptr allocate分配返回的指针
	//@param n 需释放的元素个数
	void deallocate(pointer ptr, size_type n) {
		return BaseDeallocate(ptr, n * sizeof(T));
	}
};

template<typename T>
inline bool operator==(const PoolAllocator<T> &, const PoolAllocator<T> &) {
	return true;
}

template<typename T>
inline bool operator!=(const PoolAllocator<T> &, const PoolAllocator<T> &) {
	return false;
}

//这里进行static变量初始化，并调用ObjectPoolArray的构造函数
ObjectPoolArray PoolAllocatorBase::pool(PoolAllocatorBase::pool_size, PoolAllocatorBase::align);

}

#endif //POOL_ALLOCATOR_H_