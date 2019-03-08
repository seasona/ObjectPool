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

//�ط��������࣬ȷ����ͬ���͵ķ�����ʹ��ͬһ����
class PoolAllocatorBase {
public:
	static const size_t align = POOL_ALLOCATOR_SIZE_STEP;
	static const size_t pool_size = POOL_ALLOCATOR_POOL_SIZE;
	static const size_t max_size = align*pool_size;
	static ObjectPoolArray pool;

	//@brief ���ݽڵ��Сѡ����ʽڵ��С�ĳ�
	//@param n ����Ҫ����Ľڵ�Ĵ�С�������ֽ�
	void *BaseAllocate(size_t n) throw (std::bad_alloc) {
		if (n == 0) {
			throw std::bad_alloc();
		} else if (n > max_size) {
			return static_cast<void*>(operator new (n));
		} else {
			//�ڴ��������ֻ�к��ʽڵ��С�ĳط����ڴ�
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

	//@brief ��������������ʵ�����Ѿ�������ڴ�
	void construct(pointer ptr, const_reference v) {
		new ((void*)ptr) T(v);
	}

	//@brief ֻ�������洢��Ԫ�أ���int��һЩ�࣬���ͷ��ڴ�
	void destroy(pointer ptr) {
		ptr->~T();
	}

	//@brief ����base���еķ��亯���������ڴ�
	//@param n �����Ԫ�ظ���
	pointer allocate(size_type n, const void * = 0) throw(std::bad_alloc) {
		return static_cast<pointer>(BaseAllocate(n * sizeof(T)));
	}

	//@brief ����base���е��ͷź������ͷ��ڴ�
	//@param ptr allocate���䷵�ص�ָ��
	//@param n ���ͷŵ�Ԫ�ظ���
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

//�������static������ʼ����������ObjectPoolArray�Ĺ��캯��
ObjectPoolArray PoolAllocatorBase::pool(PoolAllocatorBase::pool_size, PoolAllocatorBase::align);

}

#endif //POOL_ALLOCATOR_H_