#pragma once
template<typename T>
struct DynamicArray//stl vector����ʵ�ַ���
{
    int                 Size=0;//���鳤��
    int                 Capacity=0;//��������
    T* Data;//�����ַ

    // ������ ��������
    typedef T                   value_type;
    typedef value_type* iterator;
    typedef const value_type* const_iterator;

    // ���캯�� ��������
    inline DynamicArray() {
        Size = Capacity = 0; 
        Data = NULL;
    }
    inline DynamicArray(const DynamicArray<T>& src) {
        Size = Capacity = 0; 
        Data = NULL; 
        operator=(src); 
    }//�����С
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
    }// ���
    inline void         clear_destruct() { 
        for (int n = 0; n < Size; n++) 
            Data[n].~T(); 
            clear(); 
    }
    //�������ж������Ƿ�Ϊ�� �����С ���� �ֽ��� ��������
    inline bool         empty() const { return Size == 0; }
    inline int          size() const { return Size; }
    inline int          size_in_bytes() const { return Size * (int)sizeof(T); }
    inline int          max_size() const { return 0x7FFFFFFF / (int)sizeof(T); }
    inline int          capacity() const { return Capacity; }
    inline T& operator[](int i) { 
        _ASSERT(i >= 0 && i < Size); 
        return Data[i]; 
    }//�൱�����������
    inline const T& operator[](int i) const { 
        _ASSERT(i >= 0 && i < Size); 
        return Data[i];
    }
    //�����׵�ַ β����ַ ��������
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
    //�仯�����ڴ��������С �����ַ
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
    inline void         reserve(unsigned int new_capacity) { //����������ڴ��ַ����
        if (new_capacity <= Capacity) 
            return;
        T* new_data = (T*)malloc((size_t)new_capacity * sizeof(T));//�п��������ӵ����� �ڴ��Ԫ����ֵ�����ֵ
        memset(new_data,0, new_capacity * sizeof(T));//��Ҫ���г�ʼ���ڴ�
        if (Data) { 
            memcpy(new_data, Data, Size * sizeof(T)); 
            free(Data);
        } 
        Data = new_data; 
        Capacity = new_capacity; 
    }
    inline void         reserve_discard(int new_capacity) { 
        if (new_capacity <= Capacity) // ���������С�ڻ���ڵ�ǰ������ֱ�ӷ���
            return; 
        if (Data) 
            free(Data); // �ͷŵ�ǰ������ڴ�
        Data = (T*)malloc((size_t)new_capacity * sizeof(T)); // �����µ��ڴ�� �п����ڴ����ֵ�����ֵ
        memset(Data, 0, new_capacity * sizeof(T));//��Ҫ�������ɵĵ�ַ���г�ʼ���ڴ�
        Capacity = new_capacity;// ��������
    }

    // �������ݵĲ��� ���� erase���
    inline void         push_back(const T& v) {
        if (Size == Capacity) 
            reserve(_grow_capacity(Size + 1)); //���ڴ����·�������
        memcpy(&Data[Size], &v, sizeof(v)); //�ڱ�֮ǰ�������Ԫ�ؽ����ڴ渳ֵ
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
    inline T* erase(const T* it, const T* it_last) { //�����ڴ�ĳһ������(��������ʼ��ַ-������β����ַ)
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
    inline T* insert(const T* it, const T& v) {//�����Ԫ�ز��ȥ
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
    inline bool         contains(const T& v) const { //����Ƿ�������ڴ���
        const T* data = Data;  
        const T* data_end = Data + Size; 
        while (data < data_end) 
            if (*data++ == v) 
                return true; 
        return false; 
    }
    inline T* find(const T& v) { //Ѱ�����Ԫ��������ڴ��λ��
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
		Storage.resize((size + 31) >> 5);//�൱����С���� ��32Ϊ��λ�����ڴ泤�ȷ���
		memset(Storage.Data, 0, (size_t)Storage.size() * sizeof(Storage[0]));
	}
	void SetBit(unsigned int SetValue) {
		unsigned int Var = 1 << (SetValue & 31);//1�����൱�ڳ�����Щ��  
		Storage[SetValue >> 5] |= Var;//ѭ�����������Щ�����뱻ѹ����������  Storage[0]û������ ʣ�µ�Storage[1] Storage[2]  Storage[3].....������λ�������ۻ����ֵ
	}
	void  Clear() { Storage.clear(); }
};
// ������ɫö�٣����ڱ�ʾ������ڵ����ɫ
enum class Color {
    RED,
    BLACK
};

// ����ģ���������������Ϊconst char*
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
template<typename Key, typename Value>//��������ݽṹ
class RBTreeMap {
public:
    using Node = RBTreeNode<Key, Value>;

private:
    Node* root;
    Node* nil; // ���ڱ�ʾҶ�ӽڵ��NIL�ڵ�

    // ��������
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

    // ��������
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

    // �����ĵ���
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

    // �������������ҽڵ�
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
        // �ͷ��ڴ�
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
            if (key < x->key) x = x->left;//�����ٱ���������
            else x = x->right;//����������
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
                // ���Ѵ��ڣ���������ֵ
                return x->value;
            }
        }
        // �������ڣ������½ڵ�
        Node* z = new Node(key, Value(), Color::RED, y, nil, nil);
        if (y == nil) {
            // ��Ϊ�գ������½ڵ�
            root = z;
            z->color = Color::BLACK;
        }
        else if (key < y->key) {
            y->left = z;
        }
        else {
            y->right = z;
        }
        insertFixup(z);//ƽ����������������
        return z->value;
    }
};