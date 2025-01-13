#pragma once
template<typename T>
struct DynamicArray//stl vector数组实现方法
{
    int                 Size=0;//数组长度
    int                 Capacity=0;//数组容量
    T* Data;//数组地址

    // 迭代器 数据类型
    typedef T                   value_type;
    typedef value_type* iterator;
    typedef const value_type* const_iterator;

    // 构造函数 析构函数
    inline DynamicArray() {
        Size = Capacity = 0; 
        Data = NULL;
    }
    inline DynamicArray(const DynamicArray<T>& src) {
        Size = Capacity = 0; 
        Data = NULL; 
        operator=(src); 
    }//分配大小
    inline DynamicArray<T>& operator=(const DynamicArray<T>& src) {
        clear(); 
        resize(src.Size); 
        if (src.Data) 
            memcpy(Data, src.Data, (size_t)Size * sizeof(T)); 
        return *this;
    }
    inline ~DynamicArray() { if (Data) free(Data); }

    inline void         clear() { 
        if (Data) 
        { 
            Size = Capacity = 0; 
            free(Data); 
            Data = NULL; 
        } 
    }// 清除
    inline void         clear_destruct() { 
        for (int n = 0; n < Size; n++) 
            Data[n].~T(); 
            clear(); 
    }
    //以下是判断数组是否为空 数组大小 容量 字节数 数组索引
    inline bool         empty() const { return Size == 0; }
    inline int          size() const { return Size; }
    inline int          size_in_bytes() const { return Size * (int)sizeof(T); }
    inline int          max_size() const { return 0x7FFFFFFF / (int)sizeof(T); }
    inline int          capacity() const { return Capacity; }
    inline T& operator[](int i) { 
        _ASSERT(i >= 0 && i < Size); 
        return Data[i]; 
    }//相当于数组的索引
    inline const T& operator[](int i) const { 
        _ASSERT(i >= 0 && i < Size); 
        return Data[i];
    }
    //数组首地址 尾部地址 交换数组
    inline T* begin() { return Data; }
    inline const T* begin() const { return Data; }
    inline T* end() { return Data + Size; }
    inline const T* end() const { return Data + Size; }
    inline T& front() { 
        _ASSERT(Size > 0); 
        return Data[0]; 
    }
    inline const T& front() const {
        _ASSERT(Size > 0); 
        return Data[0]; 
    }
    inline T& back() { 
        _ASSERT(Size > 0); 
        return Data[Size - 1];
    }
    inline const T& back() const { 
        _ASSERT(Size > 0); 
        return Data[Size - 1];
    }
    inline void         swap(DynamicArray<T>& rhs) { 
        int rhs_size = rhs.Size; 
        rhs.Size = Size; 
        Size = rhs_size; 
        int rhs_cap = rhs.Capacity; 
        rhs.Capacity = Capacity; 
        Capacity = rhs_cap; 
        T* rhs_data = rhs.Data; 
        rhs.Data = Data; 
        Data = rhs_data;
    }
    //变化数组内存的容量大小 分配地址
    inline int          _grow_capacity(int sz) const { 
        int new_capacity = Capacity ? (Capacity + Capacity / 2) : 8;
        return new_capacity > sz ? new_capacity : sz; 
    }
    inline void         resize(unsigned int new_size) { 
        if (new_size > Capacity) 
            reserve(_grow_capacity(new_size)); 
        Size = new_size; 
    }
    inline void         resize(unsigned int new_size, const T& v) {
        if (new_size > Capacity) 
            reserve(_grow_capacity(new_size)); 
        if (new_size > Size) 
            for (int n = Size; n < new_size; n++) 
                memcpy(&Data[n], &v, sizeof(v)); 
        Size = new_size; 
    }
    inline void         reserve(unsigned int new_capacity) { //容量分配和内存地址分配
        if (new_capacity <= Capacity) 
            return;
        T* new_data = (T*)malloc((size_t)new_capacity * sizeof(T));//有可能新增加的容量 内存的元素数值是随机值
        memset(new_data,0, new_capacity * sizeof(T));//需要进行初始化内存
        if (Data) { 
            memcpy(new_data, Data, Size * sizeof(T)); 
            free(Data);
        } 
        Data = new_data; 
        Capacity = new_capacity; 
    }
    inline void         reserve_discard(int new_capacity) { 
        if (new_capacity <= Capacity) // 如果新容量小于或等于当前容量，直接返回
            return; 
        if (Data) 
            free(Data); // 释放当前分配的内存
        Data = (T*)malloc((size_t)new_capacity * sizeof(T)); // 分配新的内存块 有可能内存的数值是随机值
        memset(Data, 0, new_capacity * sizeof(T));//需要对新生成的地址进行初始化内存
        Capacity = new_capacity;// 更新容量
    }

    // 进行数据的插入 弹出 erase清除
    inline void         push_back(const T& v) {
        if (Size == Capacity) 
            reserve(_grow_capacity(Size + 1)); //对内存重新分配容量
        memcpy(&Data[Size], &v, sizeof(v)); //在比之前多出来的元素进行内存赋值
        Size++;
    }
    inline void         pop_back() { _ASSERT(Size > 0); Size--; }
    inline void         push_front(const T& v) { if (Size == 0) push_back(v); else insert(Data, v); }
    inline T* erase(const T* it) { 
        _ASSERT(it >= Data && it < Data + Size); 
        const ptrdiff_t off = it - Data; 
        memmove(Data + off, Data + off + 1, ((size_t)Size - (size_t)off - 1) * sizeof(T));
        Size--; 
        return Data + off; }
    inline T* erase(const T* it, const T* it_last) { //擦除内存某一段内容(擦除的起始地址-擦除的尾部地址)
        _ASSERT(it >= Data && it < Data + Size && it_last >= it && it_last <= Data + Size); 
        const ptrdiff_t count = it_last - it; 
        const ptrdiff_t off = it - Data;
        memmove(Data + off, Data + off + count, ((size_t)Size - (size_t)off - (size_t)count) * sizeof(T));
        Size -= (int)count; 
        return Data + off; 
    }
    inline T* erase_unsorted(const T* it) { 
        _ASSERT(it >= Data && it < Data + Size); 
        const ptrdiff_t off = it - Data; 
        if (it < Data + Size - 1) 
            memcpy(Data + off, Data + Size - 1, sizeof(T)); 
        Size--; 
        return Data + off;
    }
    inline T* insert(const T* it, const T& v) {//将这个元素插进去
        _ASSERT(it >= Data && it <= Data + Size); 
        const ptrdiff_t off = it - Data; 
        if (Size == Capacity) 
            reserve(_grow_capacity(Size + 1)); 
        if (off < (int)Size) 
            memmove(Data + off + 1, Data + off, ((size_t)Size - (size_t)off) * sizeof(T));
        memcpy(&Data[off], &v, sizeof(v)); 
        Size++; 
        return Data + off;
    }
    inline bool         contains(const T& v) const { //检查是否在这个内存中
        const T* data = Data;  
        const T* data_end = Data + Size; 
        while (data < data_end) 
            if (*data++ == v) 
                return true; 
        return false; 
    }
    inline T* find(const T& v) { //寻找这个元素在这个内存的位置
        T* data = Data;  
        const T* data_end = Data + Size; 
        while (data < data_end) 
            if (*data == v) 
                break; 
            else ++data; 
        return data; 
    }
    inline const T* find(const T& v) const { 
        const T* data = Data;  
        const T* data_end = Data + Size; 
        while (data < data_end) 
            if (*data == v) 
                break; 
            else ++data; 
        return data; 
    }
    inline bool         find_erase(const T& v) { 
        const T* it = find(v); 
        if (it < Data + Size) { 
            erase(it); 
            return true;
        } 
        return false; 
    }
    inline bool         find_erase_unsorted(const T& v) { 
        const T* it = find(v); 
        if (it < Data + Size) {
            erase_unsorted(it); 
            return true; 
        } 
        return false; 
    }
    inline int          index_from_ptr(const T* it) const { 
        _ASSERT(it >= Data && it < Data + Size); 
        const ptrdiff_t off = it - Data; 
        return (int)off;
    }
};
struct BitVector
{
    DynamicArray<unsigned int>Storage;
	void Create(unsigned int size) {
		Storage.resize((size + 31) >> 5);//相当于缩小容量 以32为单位进行内存长度分配
		memset(Storage.Data, 0, (size_t)Storage.size() * sizeof(Storage[0]));
	}
	void SetBit(unsigned int SetValue) {
		unsigned int Var = 1 << (SetValue & 31);//1左移相当于乘上这些数  
		Storage[SetValue >> 5] |= Var;//循环或操作将这些数放入被压缩的数组里  Storage[0]没有内容 剩下的Storage[1] Storage[2]  Storage[3].....都是移位操作的累积或的值
	}
	void  Clear() { Storage.clear(); }
};
// 定义颜色枚举，用于表示红黑树节点的颜色
enum class Color {
    RED,
    BLACK
};

// 定义模板参数，键的类型为const char*
template<typename Key, typename Value>
struct RBTreeNode {
    Key key;
    Value value;
    Color color;
    RBTreeNode* left;
    RBTreeNode* right;
    RBTreeNode* parent;

    RBTreeNode(Key k, Value v, Color c, RBTreeNode* p = nullptr, RBTreeNode* l = nullptr, RBTreeNode* r = nullptr)
        : key(k), value(v), color(c), parent(p), left(l), right(r) {}
};
template<typename Key, typename Value>//红黑树数据结构
class RBTreeMap {
public:
    using Node = RBTreeNode<Key, Value>;

private:
    Node* root;
    Node* nil; // 用于表示叶子节点的NIL节点

    // 左旋操作
    void leftRotate(Node* x) {
        Node* y = x->right;
        x->right = y->left;
        if (y->left != nil) y->left->parent = x;
        y->parent = x->parent;
        if (x->parent == nil) root = y;
        else if (x == x->parent->left) x->parent->left = y;
        else x->parent->right = y;
        y->left = x;
        x->parent = y;
    }

    // 右旋操作
    void rightRotate(Node* x) {
        Node* y = x->left;
        x->left = y->right;
        if (y->right != nil) y->right->parent = x;
        y->parent = x->parent;
        if (x->parent == nil) root = y;
        else if (x == x->parent->right) x->parent->right = y;
        else x->parent->left = y;
        y->right = x;
        x->parent = y;
    }

    // 插入后的调整
    void insertFixup(Node* z) {
        while (z->parent->color == Color::RED) {
            if (z->parent == z->parent->parent->left) {
                Node* y = z->parent->parent->right;
                if (y->color == Color::RED) {
                    z->parent->color = Color::BLACK;
                    y->color = Color::BLACK;
                    z->parent->parent->color = Color::RED;
                    z = z->parent->parent;
                }
                else {
                    if (z == z->parent->right) {
                        z = z->parent;
                        leftRotate(z);
                    }
                    z->parent->color = Color::BLACK;
                    z->parent->parent->color = Color::RED;
                    rightRotate(z->parent->parent);
                }
            }
            else {
                Node* y = z->parent->parent->left;
                if (y->color == Color::RED) {
                    z->parent->color = Color::BLACK;
                    y->color = Color::BLACK;
                    z->parent->parent->color = Color::RED;
                    z = z->parent->parent;
                }
                else {
                    if (z == z->parent->left) {
                        z = z->parent;
                        rightRotate(z);
                    }
                    z->parent->color = Color::BLACK;
                    z->parent->parent->color = Color::RED;
                    leftRotate(z->parent->parent);
                }
            }
        }
        root->color = Color::BLACK;
    }

    // 辅助函数，查找节点
    Node* findNode(const Key& key) {
        Node* x = root;
        while (x != nil) {
            if (key == x->key) {
                return x;
            }
            if (key < x->key) x = x->left;
            else x = x->right;
        }
        return nil;
    }

public:
    RBTreeMap() {
        nil = new Node(Key(), Value(), Color::BLACK);
        root = nil;
    }

    ~RBTreeMap() {
        // 释放内存
        clear(root);
        delete nil;
    }

    void clear(Node* node) {
        if (node == nil) return;
        clear(node->left);
        clear(node->right);
        delete node;
    }

    void insert(const Key& key, const Value& value) {
        Node* z = new Node(key, value, Color::RED, nil, nil, nil);
        Node* y = nil;
        Node* x = root;

        while (x != nil) {
            y = x;
            if (z->key < x->key) x = x->left;
            else x = x->right;
        }
        z->parent = y;
        if (y == nil) root = z;
        else if (z->key < y->key) y->left = z;
        else y->right = z;
        z->left = nil;
        z->right = nil;
        insertFixup(z);
    }

    bool find(const Key& key) {
        Node* x = root;
        while (x != nil) {
            if (key == x->key) {
                return true;
            }
            if (key < x->key) x = x->left;//依次再遍历左子树
            else x = x->right;//遍历右子树
        }
        return false;
    }

    Value& operator[](const Key& key) {
        Node* x = root;
        Node* y = nil;
        while (x != nil) {
            y = x;
            if (key < x->key) x = x->left;
            else if (key > x->key) x = x->right;
            else {
                // 键已存在，返回现有值
                return x->value;
            }
        }
        // 键不存在，插入新节点
        Node* z = new Node(key, Value(), Color::RED, y, nil, nil);
        if (y == nil) {
            // 树为空，插入新节点
            root = z;
            z->color = Color::BLACK;
        }
        else if (key < y->key) {
            y->left = z;
        }
        else {
            y->right = z;
        }
        insertFixup(z);//平衡红黑树的数量性质
        return z->value;
    }
};