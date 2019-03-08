#ifndef OBJECT_POOL_H_
#define OBJECT_POOL_H_

#include <cstddef>
#include <algorithm>
#include <new>

#ifndef NUM_NODES_PER_CHUNK
#define NUM_NODES_PER_CHUNK 128
#endif 

namespace pool {

class ObjectPool {
public:
	//@brief 构造并计算空闲节点和组块大小
	//@param node_size 节点数据大小
	ObjectPool(size_t node_size) :
		chunk_data_head_(0), free_node_head_(0),
		node_size_(node_size),
		free_node_size_(GetFreeNodeSize(node_size)),
		chunk_size_(GetChunkSize()) {}
	
	//@brief 析构时按链表释放所有已经申请的组块内存
	~ObjectPool();

	//@brief 分配一块内存作为内存池
	void *allocate() throw(std::bad_alloc);
	
	//@brief 将allocate分配的内存释放
	//@param ptr ptr是allocate分配内存返回的指针
	void deallocate(void *ptr);
private:
	static const unsigned char num_nodes_per_chunk = NUM_NODES_PER_CHUNK;
	typedef unsigned char counter_type;

	//组块，也就是整个内存池
	struct ChunkData {
		counter_type free_node_count;
		ChunkData *next;
	};
	static const size_t chunk_data_size_ = sizeof(ChunkData);
	ChunkData *chunk_data_head_;

	//空闲节点
	struct FreeNode {
		counter_type bias;
		FreeNode *next;
	};
	static const size_t free_node_offset_ = offsetof(FreeNode, next);
	FreeNode *free_node_head_;

	const size_t node_size_;		//空闲节点数据大小
	const size_t free_node_size_;	//空闲节点总大小
	const size_t chunk_size_;		//组块大小

	size_t GetFreeNodeSize(size_t node_size) {
		return free_node_offset_ + std::max(sizeof(FreeNode*), node_size);
	};
	size_t GetChunkSize() { 
		return chunk_data_size_ + free_node_size_*num_nodes_per_chunk;
	};
};

//可自动构造不同尺寸内存池的数组类
class ObjectPoolArray {
public:
	//@brief 构造函数，参数sz为数组中对象池个数，step则为各对象池节点尺寸之差
	//例如sz=4，step=8时将构造4个对象池，节点尺寸依次为8，16，24，32
	ObjectPoolArray(size_t sz, size_t step) :
		sz_(sz),
		array_(static_cast<ObjectPool*>(operator new(sizeof(ObjectPool)*sz_))) {
		for (size_t i = 0; i < sz; i++) {
			//这里只是分配了内存池的类型大小节点，实际内存是通过ObjectPool中的allocate来分配的
			new (array_ + i) ObjectPool(i*step + step);
		}
	}
	~ObjectPoolArray() {
		for (size_t i = 0; i < sz_; i++) {
			(array_ + i)->~ObjectPool();
		}
		operator delete (array_);
	}
	size_t size() const { return sz_; }

	ObjectPool& operator[](size_t n) { return array_[n]; }
private:
	size_t sz_;
	ObjectPool *array_;
};

//实际是在这里释放已分配的内存
ObjectPool::~ObjectPool() {
	while (chunk_data_head_) {
		ChunkData *chunk = chunk_data_head_;
		chunk_data_head_ = chunk_data_head_->next;
		operator delete(chunk);
	}
}

//@brief 方便按字节移位并转义指针
template<typename T,typename U>
inline T* ByteShift(U *ptr, size_t b) {
	return reinterpret_cast<T*>(reinterpret_cast<char*>(ptr) + b);
}

void *ObjectPool::allocate() throw(std::bad_alloc) {
	if (!free_node_head_) {
		//无空闲节点，新申请一块chunk并同步更新chunk list和free node list
		ChunkData *new_chunk = reinterpret_cast<ChunkData*>(operator new(chunk_size_));
		new_chunk->next = chunk_data_head_;
		chunk_data_head_ = new_chunk;

		//将所申请的chunk分成空闲节点，并一一插入空闲列表，同时标记正确的bias值
		free_node_head_ = ByteShift<FreeNode>(new_chunk, chunk_data_size_);//模板自动推导
		//free_node_head_->bias = 0;
		free_node_head_->next = NULL;
		for (size_t node_index = 1; node_index < num_nodes_per_chunk; node_index++) {
			FreeNode *f = ByteShift<FreeNode>(free_node_head_, free_node_size_);
			//f->bias = free_node_head_->bias + 1;
			f->next = free_node_head_;
			free_node_head_ = f;
		}
	}

	//返回能使用的空闲块中数据起始地址
	FreeNode *return_node = free_node_head_;
	free_node_head_ = free_node_head_->next;
	//返回值是指针的地址，因为节点的大小是未知的，所以只能通过指针的地址，确定存储数据的位置
	//next指针的地址就是块头加上free_node_offset_后的地址
	return static_cast<void*>(&(return_node->next));
}

void ObjectPool::deallocate(void *ptr) {
	//将所回收的块插入空闲列表，偏移一个offset可以到下一空闲块的块头地址
	FreeNode *f = ByteShift<FreeNode>(ptr, -1 * free_node_offset_);
	f->next = free_node_head_;
	free_node_head_ = f;
}

}
#endif // _OBJECTPOOL_H
