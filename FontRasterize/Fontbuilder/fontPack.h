#pragma once

#define PACKMAX 0x7fffffff
struct PackedRect
{
	float x=0, y=0, w=0, h=0;
	bool was_PackedValid=true;
};//��װ���ַ���߼�λ��
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
	PackedNode* allocatedNodes;   // ���� malloc �������ʼ��ַ
	PackedNode extraNode[2];
	unsigned int width;
	unsigned int height;
	unsigned int Num_Nodes;
};
void PackedRectBegin(PackedContext*C,unsigned int width,unsigned int height)//��������ܿ�Ⱥ͸߶�
{
	if (C == nullptr) return;
	unsigned int NumNodes = width - 1;
	// ������Ƿ�Ϸ�
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
	// ��ʼ������
	for (unsigned int i = 0; i < NumNodes - 1; i++) {
		allocateNode[i].next = &allocateNode[i + 1];
		allocateNode[i].x = 0;  // ��ʼ�� x
		allocateNode[i].y = 0;  // ��ʼ�� y
	}
	allocateNode[NumNodes - 1].next = nullptr;

	// ���������Ľṹ��
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
	float x1 = x0 + width;//��Զ���ٽ��ҳ���ԱȽϸߵĸ߶�
	float min_y = 0.0f;//ȷ������һ��������
	while (tempNode != NULL && tempNode->x < x1)//һֱ����ֱ���ҵ����������������޴����������������ȿ��Էŵ��½���ѭ��
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
	float best_y = (1 << 30);//ȷ���տ�ʼ��С����

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
	//��ͷ�ڵ㿪ʼ����
	while (TempNode->x + width <= C->width)
	{
		float y;
		y = Find_Min_y(TempNode, width);
		if (y < best_y)//����֮ǰ�Ѿ��ҵ���С�ɹ��ĸ߶� ֮�����һ����������ֹ�˷ѿռ�
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
	insertNode->y = res.y + height;//���뷽���߶Ȼ����
	C->freehead = insertNode->next;
	cur = *res.prevNode;//������һ������ �൱�ڱ�������һ�εĸ߶�
	*res.prevNode = insertNode;//���ȥ
	while (cur->next && cur->next->x <= res.x + width)//node->next��λ�ø�ֵ 
	{//��cur����һ��λ�õĺ������С�����ķ�����������ֱ���ҵ��������������ķ�����ֹͣ  ֮ǰ�Ľڵ�������������C->freehead����
		PackedNode* next = cur->next;
		cur->next = C->freehead;
		C->freehead = cur;
		cur = next;
	}
	insertNode->next = cur;//����һ�ε������ŵ���ǰ�ڵ�ĺ���
	if (cur->x < res.x + width)
		cur->x = res.x + width;//��ǰ�ƶ�ͬʱԭ�ȸ߶�Ҫ���ֲ���
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

	// �ͷ� allocatedNodes ָ����ڴ�  �൱���ͷ�֮ǰ��̬������ڴ� PackedNode* allocateNode = (PackedNode*)malloc(sizeof(PackedNode) * NumNodes);
	if (C->allocatedNodes != nullptr) {
		free(C->allocatedNodes);
		C->allocatedNodes = nullptr; // ��������ָ��
	}

	// ���������ֶ�
	C->freehead = nullptr;
	C->activehead = nullptr;
	C->width = 0;
	C->height = 0;
	C->Num_Nodes = 0;

	printf("PackedRectEnd: Memory has been released successfully.\n");
}