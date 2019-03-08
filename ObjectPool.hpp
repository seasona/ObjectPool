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
	//@brief ���첢������нڵ������С
	//@param node_size �ڵ����ݴ�С
	ObjectPool(size_t node_size) :
		chunk_data_head_(0), free_node_head_(0),
		node_size_(node_size),
		free_node_size_(GetFreeNodeSize(node_size)),
		chunk_size_(GetChunkSize()) {}
	
	//@brief ����ʱ�������ͷ������Ѿ����������ڴ�
	~ObjectPool();

	//@brief ����һ���ڴ���Ϊ�ڴ��
	void *allocate() throw(std::bad_alloc);
	
	//@brief ��allocate������ڴ��ͷ�
	//@param ptr ptr��allocate�����ڴ淵�ص�ָ��
	void deallocate(void *ptr);
private:
	static const unsigned char num_nodes_per_chunk = NUM_NODES_PER_CHUNK;
	typedef unsigned char counter_type;

	//��飬Ҳ���������ڴ��
	struct ChunkData {
		counter_type free_node_count;
		ChunkData *next;
	};
	static const size_t chunk_data_size_ = sizeof(ChunkData);
	ChunkData *chunk_data_head_;

	//���нڵ�
	struct FreeNode {
		counter_type bias;
		FreeNode *next;
	};
	static const size_t free_node_offset_ = offsetof(FreeNode, next);
	FreeNode *free_node_head_;

	const size_t node_size_;		//���нڵ����ݴ�С
	const size_t free_node_size_;	//���нڵ��ܴ�С
	const size_t chunk_size_;		//����С

	size_t GetFreeNodeSize(size_t node_size) {
		return free_node_offset_ + std::max(sizeof(FreeNode*), node_size);
	};
	size_t GetChunkSize() { 
		return chunk_data_size_ + free_node_size_*num_nodes_per_chunk;
	};
};

//���Զ����첻ͬ�ߴ��ڴ�ص�������
class ObjectPoolArray {
public:
	//@brief ���캯��������szΪ�����ж���ظ�����step��Ϊ������ؽڵ�ߴ�֮��
	//����sz=4��step=8ʱ������4������أ��ڵ�ߴ�����Ϊ8��16��24��32
	ObjectPoolArray(size_t sz, size_t step) :
		sz_(sz),
		array_(static_cast<ObjectPool*>(operator new(sizeof(ObjectPool)*sz_))) {
		for (size_t i = 0; i < sz; i++) {
			//����ֻ�Ƿ������ڴ�ص����ʹ�С�ڵ㣬ʵ���ڴ���ͨ��ObjectPool�е�allocate�������
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

//ʵ�����������ͷ��ѷ�����ڴ�
ObjectPool::~ObjectPool() {
	while (chunk_data_head_) {
		ChunkData *chunk = chunk_data_head_;
		chunk_data_head_ = chunk_data_head_->next;
		operator delete(chunk);
	}
}

//@brief ���㰴�ֽ���λ��ת��ָ��
template<typename T,typename U>
inline T* ByteShift(U *ptr, size_t b) {
	return reinterpret_cast<T*>(reinterpret_cast<char*>(ptr) + b);
}

void *ObjectPool::allocate() throw(std::bad_alloc) {
	if (!free_node_head_) {
		//�޿��нڵ㣬������һ��chunk��ͬ������chunk list��free node list
		ChunkData *new_chunk = reinterpret_cast<ChunkData*>(operator new(chunk_size_));
		new_chunk->next = chunk_data_head_;
		chunk_data_head_ = new_chunk;

		//���������chunk�ֳɿ��нڵ㣬��һһ��������б�ͬʱ�����ȷ��biasֵ
		free_node_head_ = ByteShift<FreeNode>(new_chunk, chunk_data_size_);//ģ���Զ��Ƶ�
		//free_node_head_->bias = 0;
		free_node_head_->next = NULL;
		for (size_t node_index = 1; node_index < num_nodes_per_chunk; node_index++) {
			FreeNode *f = ByteShift<FreeNode>(free_node_head_, free_node_size_);
			//f->bias = free_node_head_->bias + 1;
			f->next = free_node_head_;
			free_node_head_ = f;
		}
	}

	//������ʹ�õĿ��п���������ʼ��ַ
	FreeNode *return_node = free_node_head_;
	free_node_head_ = free_node_head_->next;
	//����ֵ��ָ��ĵ�ַ����Ϊ�ڵ�Ĵ�С��δ֪�ģ�����ֻ��ͨ��ָ��ĵ�ַ��ȷ���洢���ݵ�λ��
	//nextָ��ĵ�ַ���ǿ�ͷ����free_node_offset_��ĵ�ַ
	return static_cast<void*>(&(return_node->next));
}

void ObjectPool::deallocate(void *ptr) {
	//�������յĿ��������б�ƫ��һ��offset���Ե���һ���п�Ŀ�ͷ��ַ
	FreeNode *f = ByteShift<FreeNode>(ptr, -1 * free_node_offset_);
	f->next = free_node_head_;
	free_node_head_ = f;
}

}
#endif // _OBJECTPOOL_H
