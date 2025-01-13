#pragma once

#define PACKMAX 0x7fffffff
struct PackedRect
{
	float x=0, y=0, w=0, h=0;
	bool was_PackedValid=true;
};//包装的字符宽高及位置
struct PackedNode
{
	float x,y;
	PackedNode* next;
};
typedef struct
{
	float x,y;
	PackedNode** prevNode;
}FindResult;
struct PackedContext
{
	PackedNode* freehead;
	PackedNode* activehead;
	PackedNode* allocatedNodes;   // 保存 malloc 分配的起始地址
	PackedNode extraNode[2];
	unsigned int width;
	unsigned int height;
	unsigned int Num_Nodes;
};
void PackedRectBegin(PackedContext*C,unsigned int width,unsigned int height)//输入的是总宽度和高度
{
	if (C == nullptr) return;
	unsigned int NumNodes = width - 1;
	// 检查宽度是否合法
	if (NumNodes < 1) {
		printf("Error: Invalid width (%u), no nodes to allocate.\n", width);
		return;
	}
	PackedNode* allocateNode = (PackedNode*)malloc(sizeof(PackedNode) * NumNodes);
	if (allocateNode == nullptr) {
		printf("Error: Failed to allocate memory for PackedNode.\n");
		return;
	}
	C->allocatedNodes = allocateNode;
	// 初始化链表
	for (unsigned int i = 0; i < NumNodes - 1; i++) {
		allocateNode[i].next = &allocateNode[i + 1];
		allocateNode[i].x = 0;  // 初始化 x
		allocateNode[i].y = 0;  // 初始化 y
	}
	allocateNode[NumNodes - 1].next = nullptr;

	// 更新上下文结构体
	C->freehead = &allocateNode[0];
	C->activehead = &C->extraNode[0];
	C->extraNode[0].x = 0;
	C->extraNode[0].y = 0;
	C->extraNode[0].next = &C->extraNode[1];
	C->extraNode[1].x = width;
	C->extraNode[1].y = 0;
	C->extraNode[1].next = nullptr;
	C->width = width;
	C->height = height;
	C->Num_Nodes = NumNodes;

	printf("PackedRectBegin: Initialized successfully with %u nodes.\n", NumNodes);
}
static float Find_Min_y(PackedNode* first, float width)
{
	PackedNode* tempNode = first;
	float x0 = tempNode->x;
	float x1 = x0 + width;//永远从临近找出相对比较高的高度
	float min_y = 0.0f;//确保所有一定大于它
	while (tempNode != NULL && tempNode->x < x1)//一直查找直到找到横坐标大于这个宽限代表我们现在这个宽度可以放得下结束循环
	{
		if (tempNode->y > min_y)
		{
			min_y = tempNode->y;
		}
		tempNode = tempNode->next;
	}
	return min_y;
}
static FindResult FindBestPos(PackedContext* C, float width)
{
	float best_y = (1 << 30);//确保刚开始会小于它

	FindResult fr;
	PackedNode** previous = NULL, ** Best = NULL;
	PackedNode* TempNode = C->activehead;
	previous = &C->activehead;

	if (width > C->width)
	{
		fr.x = 0;
		fr.y = 0;
		fr.prevNode = NULL;
		return fr;
	}
	//从头节点开始遍历
	while (TempNode->x + width <= C->width)
	{
		float y;
		y = Find_Min_y(TempNode, width);
		if (y < best_y)//在这之前已经找到稍小可够的高度 之后如果一样就舍弃防止浪费空间
		{

			best_y = y;
			Best = previous;
		}
		previous = &TempNode->next;
		TempNode = TempNode->next;
	}
	fr.x = (Best == NULL) ? 0 : (*Best)->x;
	fr.y = best_y;
	fr.prevNode = Best;
	return fr;
}
static FindResult PackRectangleResult(PackedContext* C, float width, float height)
{
	PackedNode* insertNode, * cur;
	FindResult res = FindBestPos(C, width);
	insertNode = C->freehead;
	insertNode->x = res.x;
	insertNode->y = res.y + height;//放入方块后高度会提高
	C->freehead = insertNode->next;
	cur = *res.prevNode;//保存上一次链条 相当于保存了上一次的高度
	*res.prevNode = insertNode;//插进去
	while (cur->next && cur->next->x <= res.x + width)//node->next的位置赋值 
	{//当cur的下一个位置的横坐标宽小于来的方块宽继续遍历直到找到横坐标宽大于来的方块宽就停止  之前的节点链条就舍弃由C->freehead回收
		PackedNode* next = cur->next;
		cur->next = C->freehead;
		C->freehead = cur;
		cur = next;
	}
	insertNode->next = cur;//将上一次的链条放到当前节点的后面
	if (cur->x < res.x + width)
		cur->x = res.x + width;//往前移动同时原先高度要保持不变
	return res;
}

void PackRect(PackedContext* C, PackedRect*Rect,unsigned int num_rect)
{
	for (unsigned int i=0;i<num_rect;i++)
	{
		FindResult result=PackRectangleResult(C, Rect[i].w, Rect[i].h);
		if (result.prevNode)
		{
			Rect[i].x = result.x;
			Rect[i].y = result.y;
		}
	}
	for (unsigned int i = 0; i < num_rect; i++)
	{
		Rect[i].was_PackedValid = !(Rect[i].x==PACKMAX&& Rect[i].y == PACKMAX);
	}
}
void PackedRectEnd(PackedContext* C)
{
	if (C == nullptr) return;

	// 释放 allocatedNodes 指向的内存  相当于释放之前动态分配的内存 PackedNode* allocateNode = (PackedNode*)malloc(sizeof(PackedNode) * NumNodes);
	if (C->allocatedNodes != nullptr) {
		free(C->allocatedNodes);
		C->allocatedNodes = nullptr; // 避免悬空指针
	}

	// 清理其他字段
	C->freehead = nullptr;
	C->activehead = nullptr;
	C->width = 0;
	C->height = 0;
	C->Num_Nodes = 0;

	printf("PackedRectEnd: Memory has been released successfully.\n");
}