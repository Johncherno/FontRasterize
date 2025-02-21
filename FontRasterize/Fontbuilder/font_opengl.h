#pragma once
#define GLEW_STATIC
#include<GL/glew.h>
#include<GLFW/glfw3.h>
#include"FontStructure.h"
#include"shaderProgram.h"
#include<glm.hpp>
struct DrawVert//���͸�OpenGL�Ķ������ݽṹ
{
	glm::vec2 pos;
	glm::vec2 Tex;
	glm::vec4 Col;//��ͬ���ڵĻ����б��в�ͬ��alphaֵ
};
struct ClipRect
{
	glm::vec2 Min;//�ü��������Ͻ�����
	glm::vec2 Max;//�ü��������Ͻ�����
	float Width;//�ü����εĿ��
	float Height;//�ü����εĸ߶�
	ClipRect(glm::vec2 ClipMin, glm::vec2 ClipMax) :Min(ClipMin), Max(ClipMax) { this->Width = Max.x - Min.x; this->Height = Max.y - Min.y; }
	ClipRect() = default;
	bool Contain(glm::vec2 p) const { return p.x >= Min.x && p.y >= Min.y && p.x < Max.x && p.y < Max.y; }//����Ƿ񺭸����λ��
};
struct DrawList
{
	//Ĭ�Ϲ��캯����ʼ������ṹ�Ĳ���
	DrawList()
	{
		this->VtxCurrentIdx = 0;
		this->VtxWritePtr = nullptr;
		this->_IdxWritePtr = nullptr;
		_VtxBuffer.resize(0);
		_IdxBuffer.resize(0);
		_ArcPath.resize(0);
	}
	//ִ�к�������
	void Clear();
	void ReserveForBuffer(unsigned int vtxcount, unsigned int idxcount);
	void _PathArc(glm::vec2 center, float radius, unsigned int MinSample, unsigned int MaxSample, float scale);
	void PolyConvexFill(glm::vec4 color);
	void AddLine(glm::vec2 p1, glm::vec2 p2, glm::vec4 color, float thickness);//��������
	void AddTriangle(glm::vec2 p1, glm::vec2 p2, glm::vec2 p3, glm::vec4 color, float thickness);//���������α߿�
	void AddRectTriangleBorder(glm::vec2 Min, glm::vec2 Max, glm::vec4 color, float thickness);//���ƾ��������α߿� ���߻������巽���Ľ������ζ���߿�
	void AddRectBorder(glm::vec2 Min, glm::vec2 Max, glm::vec4 color, float thickness);//���Ʊ߿�
	void AddRectFill(glm::vec2 Min, glm::vec2 Max, glm::vec4 color, float rounding, float scale);//����Բ���ı��������ɫ���
	void AddText_VertTriangleBorder(glm::vec2 TextPosition, glm::vec4 LineColor, const char* Text, float scalex, float scaley,float thickness);//�����ַ����ķ��鶥�����Ǳ߿�
	void AddText(glm::vec2 TextPosition, glm::vec4 TextColor, const char* Text, float scalex, float scaley);	
	void AddTextSizeRectBorder(glm::vec2 StartPosition,glm::vec2 TextSize,glm::vec4 Color,float thickness);
	void AddClipText(glm::vec2 TextPosition, glm::vec4 TextColor, const char* Text, ClipRect clipRect, float scalex, float scaley);
	void RendeAllCharactersIntoScreen(glm::vec2 pos, float ConstraintWidth, glm::vec4 Col, float scalex, float scaley);
	//��Ա����
	DynamicArray<glm::vec2>_ArcPath;//�洢Բ���Ͳ���Բ��������
	DynamicArray<DrawVert>_VtxBuffer;
	DynamicArray<unsigned int>_IdxBuffer;
	DrawVert* VtxWritePtr = nullptr;
	unsigned int* _IdxWritePtr = nullptr;
	unsigned int VtxCurrentIdx = 0;


};
enum MenuShowOrderType//�˵��ڷŷ�ʽ
{
	HorizontalShow,//ˮƽ�ڷ�չʾ
	VerticalShow//��ֱ�ڷ�չʾ
};
enum ColStyle
{
	TitleRectCol,//������
	OutterRectCol,//�ⲿ�ü�����
	TextCol,//�ı�������ɫ
	MenuRectCol,//�˵�����ɫ
	HoverRectCol,//��������ɫ
	HoverTextRectCol,//�����������ı�����ɫ
	ScrollBarCol//��������ɫ
};
struct KeyEvent { int key; bool press; };//������Ϣ�ṹ��
struct InputEventParam//������Ϣ����   ���XYλ��  ��갴����Ϣ  �����Ļλ��  �������Ĺ���λ�� KeyEvent{int key bool press} ������Ϣ
{
	glm::vec2 MouseDeltaOffset;//���XYλ��
	bool MouseDown[3] = { false,false,false };//��갴�����²����ж���Ϣ []ʹ��GLFW_MOUSE_BUTTON_LEFTö������0 1 2 ����
	glm::vec2 MousePos;//�����Ļλ�� 
	float Scroll;//�������Ĺ���λ��
	KeyEvent KeyMesage;//������Ϣ
};
struct gl_Content//���UI���gl�Ļ�����Դ���ݼ���
{
	DrawList drawlist;//�����б�
	GLuint Vao, Vbo, Ebo;//glbuffer
	unsigned int VertexBufferSize, IdxBufferSize;//���㻺���Vtx Idx����
};
struct ShowMenuItem
{
	const char* name=nullptr;//����Ŀ������
	ClipRect ItemRect;//����Ŀ��Ӧ���ı���
	bool* ShowOtherContext=nullptr;//�Ƿ�����������������ʾ�жϱ�־
};
struct ShowMenu//Menu{ �˵����ü����Σ��������ü�����(HoverRect)��DynamicArray<MenuItem>MenuItemStack�洢��ǰ����Ŀ����ɫ����������Ŀ��ʾ��־ }
{
	ShowMenu()//���캯��
	{
		this->MenuRect = ClipRect(glm::vec2(0,0), glm::vec2(0, 0));
		this->HoverRect = ClipRect(glm::vec2(0, 0), glm::vec2(0, 0));
		this->ItemScrollbar = ClipRect(glm::vec2(0, 0), glm::vec2(0, 0));
		this->MenuItemOffset = glm::vec2(0,0);
		this->ScrollBarWH = glm::vec2(9, 32);
		this->ScrollbarCol = glm::vec4(0.4, 0.4, 0.4, 1);
		this->MenuCol = glm::vec4(0.5,0,0.4,1);
		this->MenuItemStack.resize(0);
		this->EnableMenuItem = false;
		this->Hoverable = false;
	}
	ClipRect MenuRect;//�˵����ü�����
	ClipRect HoverRect;//�������ü�����
	ClipRect ItemScrollbar;//�˵���������Ŀ�Ĺ�����
	glm::vec2 MenuItemOffset;//����Ŀ�Ĺ���ƫ��λ��
	DynamicArray<ShowMenuItem>MenuItemStack;//�洢��ǰ����Ŀ
	glm::vec4 MenuCol;//�˵�����ɫ
	glm::vec4 ScrollbarCol;//����������ɫ
	glm::vec2 ScrollBarWH;//��ǰ�˵����Ĺ������Ŀ��
	bool EnableMenuItem;//��������Ŀ��ʾ��־ 
	bool Hoverable;//�Ƿ�������������ʾ ������־
};//�˵���
struct ShowWindowContainer
{
	
	ShowWindowContainer()//��ʼ����ʾ���ڵĸ�������
	{
		this->Pos= glm::vec2(0, 0);//��ʾ���ڻ���λ��
		this->content.drawlist = DrawList();
		this->cursorPos = glm::vec2(0,0);
		
		this->TitleRect = ClipRect(this->Pos, this->Pos + glm::vec2(460, 70));
		this->OutterRect = ClipRect(this->Pos, this->Pos + glm::vec2(560, 590));//��ʼ���Լ����ڵ��ⲿ�ü�����

		this->COLOR[TitleRectCol] = glm::vec4(1,0,1,1);
		this->COLOR[OutterRectCol] = glm::vec4(0.4, 0, 0.2, 1);
		this->COLOR[TextCol] = glm::vec4(1.0, 1.0, 1.0, 1);

		this->content.VertexBufferSize = 0;//�Լ����ڴ洢�Ķ������
		this->content.IdxBufferSize = 0;//�Լ����ڴ洢���������Ƹ���
		this->ScaleX = 1.0f;//��ʼ���Լ����ڵ����Ŵ�С
		this->ScaleY = 1.0f;
		this->spacing = 2.0f;
		//�����Լ����ڵĸ����ж��¼����жϱ�־
		this->Flag.AllowResponseInputEvent = false;
		this->Flag.Movable = true;
		this->Flag.Bring_Front = false;
		this->Flag.IsWindowVisible = false;
		//Ĭ�ϲ˵�����ʾ��ʽΪˮƽ��ʽ
		this->MenuShowOrder = HorizontalShow;
		this->MenuArray.resize(0);
	}
	ShowMenu* CreateMenu(const char* name);
	ShowMenu* FindMenuByName(const char*name);//�Ӳ˵�ӳ����ҳ��˵�ָ��
	struct ShowWindowflag
	{
		bool AllowResponseInputEvent = false;//�Ƿ�����ǰ������Ӧ�¼� ��ֹ���ӵĴ��ڶ�����Ӧ�����¼� ����ֻ������ӵĴ������ö�������Ӧ�¼�
		bool Movable = true;//�Ƿ񴰿ڿ��������ƶ�
		bool Bring_Front = false;//�Ƿ��������ö���ʾ
		bool IsWindowVisible = true;//�Ƿ���������ʾ���Ƿ񽫱����ڻ�����Դ���㷢�͸�GPU
	};
	RBTreeMap<const char*, ShowMenu*>MenuMap;//��ǰ�����ڲ˵�ӳ���
	DynamicArray<ShowMenu*>MenuArray;//��ǰ���ڵĴ洢�����˵�������
	DynamicArray<ShowMenu*>MenuStack;//��ǰ�����ڲ˵�����ջ ֻ�ܴ洢һ���ٵ���
	glm::vec2 Pos ;//��ʾ���ڻ���λ��
	glm::vec2 cursorPos;//ÿһ���������ݵĸ���λ��
	
	ClipRect TitleRect;//�����ü�����
	ClipRect OutterRect;//�ԼҴ��ڵ��ⲿ�ü�����
	glm::vec4 COLOR[6];//��ͬ�����ݶ�Ӧ����ɫ

	gl_Content content; // �ԼҴ��ڵĻ�����Դ �ԼҴ��ڵĻ����б� ÿ�����ڶ����Լ���glbuffer �Լ����ڵĶ��㻺���Vtx Idx����
	ShowWindowflag Flag;//�Լ����ڵ�һЩ�¼��ж���־
	MenuShowOrderType MenuShowOrder;//�˵�������ʾ��ʽ ˮƽ���Ǵ�ֱ��ʽ
	float spacing;//��ʾ�����໥֮��ľ���
	float ScaleX, ScaleY;//�Լ����ڵĻ������ݵ����ű���
};
struct DrawData
{
	glm::vec2 DisplayPos;//�ӿ���ʾλ��
	glm::vec2 DisplaySize;//�ӿ���Ļ��С Ҳ������ͶӰ���ӿ�
	glm::vec2 framebufferscale;//֡�����ӿ����Ŵ�С
	DynamicArray<ShowWindowContainer*>DrawDataLayerBuffer;//�洢ÿ������ͼ��Ļ�����Դ ��һЩ�ж�״̬  �ĳ�����������֮ǰ�����ٷ��ʱ������������ٽ��п���
};

class UI
{
public:
	void StartShowWindow(const char* Text, bool Popen);//Popen���ƴ����Ƿ�������ʾ
	void BgText(const char* Text);//���Ʊ������ı�
	void StartMenu(MenuShowOrderType ShowOrder);//Horizontal  Vertical�˵�������ֱ�ڷ� ����ˮƽ�ڷ� ö������
	void Menu(const char*MenuName,bool enableMenuItem);//��ʾ�˵���
	void MenuItem(const char*ItemName,bool*ShowotherContextFlag);//Ϊ�˵����������Ŀ
	void EndMenu();//�������������Ŀ�Ĳ��� �ܵ��ж��¼���־ ��������Ŀ���ı�����ı� ����ٵ������µ��ڴ�
	void EndShowWindow();//�ӵ�ǰ���ڵ���������һ�����ڵ�����Ŀ������ΪҪ����Ŀǰ���ڵ�λ��
private:
	ShowWindowContainer* CreateNewShowWindow(const char* name);//ֻ�������һ��
	ShowWindowContainer* FindShowWindowByName(const char* name);
protected:
    RBTreeMap<const char*, ShowWindowContainer*>NameToShowWindowMap;//������ӳ�䵽���������ʾ���ڵĵ�ַ
	ShowWindowContainer* LastShowWindow = nullptr;//ֻ���ڵ�һ�δ���λ�õĸ�ֵʱ����Ҫ����
	DynamicArray<ShowWindowContainer*>CurrentShowWindowStack;//���㴰�ڵ�����Ŀ����
};
class BuiltIO
{
public:
	void CreateFontTexture();
	void CreateShowWindow_glBuffer(ShowWindowContainer* Show_Window);
	void UpdateDrawDataBuffer(UI* DrawUI);//���»������ݶ���
	void RenderDrawList();
	void DeleteResource();//�ͷ��ڴ�
	DrawData drawdata;
	FontAtlas atlas;
	InputEventParam InputMessage;//������Ӧ��Ϣ
	glm::vec2 ArcFastLookUpTable[48];//�洢cos sinֵ
	glm::vec4 Colors[8];//һЩ����������Ĭ����ɫ
	bool Has_Bring_front_opera=false;//�Ƿ����ö����ڵĲ���
protected:
	shaderProgram* shader = nullptr;
	glm::mat4 orthoProjection;
	unsigned int TexId;
private:
	void initializeArcTable();
	void SetUpBufferState(GLuint vertex_object_array, GLuint vertex_Buffer_object, GLuint element_object_object);
	void Bring_TopDrawDataLayer_ToFrontDisplay();//�����ö�����ƴ��ڻ�����Դ ���ڽ����ö����� �����ǽ����ڵĴ�������ı��� GPU�������˳��ı���
	void AllowDrawDataLayerResponseEvent();//�Ƿ����ͼ����Ӧ�¼����� �Ƿ�����ǰ������Ӧ�¼� ��ֹ���ӵĴ��ڶ�����Ӧ�����¼� ����ֻ������ӵĴ������ö�������Ӧ�¼�
};

//C��������
void SetUpContextIO();// io����
void BuildBackendData();//��������Ҫ����������Ͷ�����Դ
BuiltIO* GetIo();//����IO���ʵ�ַ
void UpdateEvent(GLFWwindow* window);
void ClearEvent();
void RenderDrawData(UI* DrawUI);//������ӽ����Ļ�������
void DestructResource();//�ͷ��ڴ�