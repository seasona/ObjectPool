[TOC]

最近一直在搞内存池，这一块之前一直没有注意过，最近看书看到这一块，发现挺重要的，琢磨了挺长时间，也在github上找了很多代码，感觉都不太好，最后是看《深入实践C++模板编程》书中的实现，感觉还不错，就自己敲了一下并分析理解了原理，感觉受益匪浅。

[源码：](https://github.com/seasona/ObjectPool)

##### 简介

什么是内存池？为什么要使用内存池？

简单的来说就是，如果我们要给很多元素分配内存，如果我们一个个用malloc分配，因为malloc本身调用非常慢，所以数据量大时耗费在malloc上的时间将会很多。内存池就是先分配一块比较大的内存，再对这块已经分配的内存进行管理，对元素进行分配，这样可以节约比较多的时间。

##### 内存池和allocator的关系

allocator是分配器，STL中大量使用它，比如vector等，我们平时使用的vector<int\>实际上是隐含了分配器，实际真正的调用应该是vector<int, std::allocator<int\>>，也就是说真正的内存管理是通过allocator来进行的。实际上std::allocator的实现就是通过内存池，而且性能还不错。

下面的表格是一个分配器所需实现的内存处理成员函数：

|  成员函数  |           简介           |
| :--------: | :----------------------: |
|  allocate  |    分配未初始化的存储    |
| deallocate |        解分配存储        |
| construct  |   在分配的存储构造对象   |
|  destroy   | 析构在已分配存储中的对象 |

这里要注意deallocate和destroy的差别，deallocate的作用是将不用的块重新放回空闲列表，而destroy是析构已分配的存储对象

```c++
//@brief 只是析构存储的元素，如int或一些类，不释放内存
void destroy(pointer ptr) {
	ptr->~T();
}
```

我一开始很疑惑，那最后是在哪里释放内存？其实释放内存是在内存池类的析构函数中，是一个静态的对象，应该是在allocator的作用域结束后通过调用析构函数释放内存

```c++
//实际是在这里释放已分配的内存
ObjectPool::~ObjectPool() {
	while (chunk_data_head_) {
		ChunkData *chunk = chunk_data_head_;
		chunk_data_head_ = chunk_data_head_->next;
		operator delete(chunk);
	}
}
```

想要实现一个可以被vector使用的allocator，必须实现上述四个成员函数以及一些变量

##### 内存池的具体实现

###### 内存池数组

我实现的这个内存池是不定长的，虽然说是不定长，但实际上只是根据你选择的元素来选择最相近的节点大小的内存池，比如你的元素大小是15，那可能就会选择节点大小为16的内存池

这是通过内存池数组来实现的：

```c++
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
```

内存池数组是个单例，通过静态初始化

```c++
//这里进行static变量初始化，并调用ObjectPoolArray的构造函数
ObjectPoolArray PoolAllocatorBase::pool(PoolAllocatorBase::pool_size, PoolAllocatorBase::align);
```

###### 内存池中的类型

实际上内存池中有两种类型：

```c++
//组块，也就是整个内存池
struct ChunkData {
	counter_type free_node_count;
	ChunkData *next;
};
```

这是配合内存池数组使用的，实际上真正分配的是内部节点类型：

```c++
//空闲节点
struct FreeNode {
	counter_type bias;
	FreeNode *next;
};
size_t GetFreeNodeSize(size_t node_size) {
	return free_node_offset_ + std::max(sizeof(FreeNode*), node_size);
};
```

注意这里的节点，我一开始很疑惑，为什么节点需要指针？不是分配一块连续的内存吗？但是其实这个指针可以帮助我们确定位置，实际节点的大小是offset（字节对齐）加上指针和你确定的元素大小的最大值。实际上那些已经分配元素的节点用到的是元素大小这一块内存；而那些还没有插入元素的，也就是空闲节点，使用的是指针，这一块概念很重要，关系到后面的很多东西。

###### allocate和deallocate

allocate中有一个地方我想了很长时间

```c++
//返回能使用的空闲块中数据起始地址
FreeNode *return_node = free_node_head_;
free_node_head_ = free_node_head_->next;
//返回值是指针的地址，因为节点的大小是未知的，所以只能通过指针的地址，确定存储数据的位置
//next指针的地址就是块头加上free_node_offset_后的地址
return static_cast<void*>(&(return_node->next));
```

return_node->next是一个指针，为什么要返回一个指针的地址？

当插入一个元素时，free_node_head\_将会移动到(free_node_head_->next)这个位置，返回&next也就是数据该开始存储的地址，

而deallocate也是这样：

```c++
void ObjectPool::deallocate(void *ptr) {
	//将所回收的块插入空闲列表，偏移一个offset可以到下一空闲块的块头地址
	FreeNode *f = ByteShift<FreeNode>(ptr, -1 * free_node_offset_);
	f->next = free_node_head_;
	free_node_head_ = f;
}
```

要想回收块，只能通过&next的地址减去offset来得到，free_node_head_此时已经无法使用。

##### 测试结果

```c++
Default Allocator Time: 3.33

MemoryPool Allocator Time: 1.683

BoostPool Allocator Time: 5.611

============my pool list=============
Insertion time: 0.453   0.455   0.459
Deletion time:  0.453   0.455   0.459
============boost pool list=============
Insertion time: 0.49    0.49    0.489
Deletion time:  0.49    0.49    0.489
============normal pool list=============
Insertion time: 0.452   0.461   0.466
Deletion time:  0.452   0.461   0.466
```

可以看出，我自己写的分配器在重写的stack中比std::allocator快不少，std::list中改进不大，而boost库中的pool则慢的多，可能是因为它是线程安全的。

如果是使用STL原本自带的容器类，原本STL的分配器比较高效，要是自己写的容器类，可能内存池会有一些用处，还是要结合实际情况来看。
