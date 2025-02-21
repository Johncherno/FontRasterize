#pragma once
#define GLEW_STATIC
#include<GL/glew.h>
#include<GLFW/glfw3.h>
#include"FontStructure.h"
#include"shaderProgram.h"
#include<glm.hpp>
struct DrawVert//发送给OpenGL的顶点数据结构
{
	glm::vec2 pos;
	glm::vec2 Tex;
	glm::vec4 Col;//不同窗口的绘制列表有不同的alpha值
};
struct ClipRect
{
	glm::vec2 Min;//裁剪矩形左上角坐标
	glm::vec2 Max;//裁剪矩形右上角坐标
	float Width;//裁剪矩形的宽度
	float Height;//裁剪矩形的高度
	ClipRect(glm::vec2 ClipMin, glm::vec2 ClipMax) :Min(ClipMin), Max(ClipMax) { this->Width = Max.x - Min.x; this->Height = Max.y - Min.y; }
	ClipRect() = default;
	bool Contain(glm::vec2 p) const { return p.x >= Min.x && p.y >= Min.y && p.x < Max.x && p.y < Max.y; }//检查是否涵盖鼠标位置
};
struct DrawList
{
	//默认构造函数初始化这个结构的参数
	DrawList()
	{
		this->VtxCurrentIdx = 0;
		this->VtxWritePtr = nullptr;
		this->_IdxWritePtr = nullptr;
		_VtxBuffer.resize(0);
		_IdxBuffer.resize(0);
		_ArcPath.resize(0);
	}
	//执行函数功能
	void Clear();
	void ReserveForBuffer(unsigned int vtxcount, unsigned int idxcount);
	void _PathArc(glm::vec2 center, float radius, unsigned int MinSample, unsigned int MaxSample, float scale);
	void PolyConvexFill(glm::vec4 color);
	void AddLine(glm::vec2 p1, glm::vec2 p2, glm::vec4 color, float thickness);//绘制线条
	void AddTriangle(glm::vec2 p1, glm::vec2 p2, glm::vec2 p3, glm::vec4 color, float thickness);//绘制三角形边框
	void AddRectTriangleBorder(glm::vec2 Min, glm::vec2 Max, glm::vec4 color, float thickness);//绘制矩形三角形边框 或者绘制字体方块四角三角形顶点边框
	void AddRectBorder(glm::vec2 Min, glm::vec2 Max, glm::vec4 color, float thickness);//绘制边框
	void AddRectFill(glm::vec2 Min, glm::vec2 Max, glm::vec4 color, float rounding, float scale);//绘制圆角文本框进行颜色填充
	void AddText_VertTriangleBorder(glm::vec2 TextPosition, glm::vec4 LineColor, const char* Text, float scalex, float scaley,float thickness);//绘制字符串的方块顶点三角边框
	void AddText(glm::vec2 TextPosition, glm::vec4 TextColor, const char* Text, float scalex, float scaley);	
	void AddTextSizeRectBorder(glm::vec2 StartPosition,glm::vec2 TextSize,glm::vec4 Color,float thickness);
	void AddClipText(glm::vec2 TextPosition, glm::vec4 TextColor, const char* Text, ClipRect clipRect, float scalex, float scaley);
	void RendeAllCharactersIntoScreen(glm::vec2 pos, float ConstraintWidth, glm::vec4 Col, float scalex, float scaley);
	//成员变量
	DynamicArray<glm::vec2>_ArcPath;//存储圆弧和不带圆弧的坐标
	DynamicArray<DrawVert>_VtxBuffer;
	DynamicArray<unsigned int>_IdxBuffer;
	DrawVert* VtxWritePtr = nullptr;
	unsigned int* _IdxWritePtr = nullptr;
	unsigned int VtxCurrentIdx = 0;


};
enum MenuShowOrderType//菜单摆放方式
{
	HorizontalShow,//水平摆放展示
	VerticalShow//垂直摆放展示
};
enum ColStyle
{
	TitleRectCol,//标题栏
	OutterRectCol,//外部裁剪窗口
	TextCol,//文本绘制颜色
	MenuRectCol,//菜单栏颜色
	HoverRectCol,//悬浮窗颜色
	HoverTextRectCol,//悬浮窗单个文本框颜色
	ScrollBarCol//滚动条颜色
};
struct KeyEvent { int key; bool press; };//按键消息结构体
struct InputEventParam//输入消息参数   鼠标XY位移  鼠标按键信息  鼠标屏幕位置  滚轮条的滚动位移 KeyEvent{int key bool press} 按键消息
{
	glm::vec2 MouseDeltaOffset;//鼠标XY位移
	bool MouseDown[3] = { false,false,false };//鼠标按键按下布尔判断信息 []使用GLFW_MOUSE_BUTTON_LEFT枚举名称0 1 2 索引
	glm::vec2 MousePos;//鼠标屏幕位置 
	float Scroll;//滚轮条的滚动位移
	KeyEvent KeyMesage;//按键消息
};
struct gl_Content//这个UI设计gl的绘制资源数据集合
{
	DrawList drawlist;//绘制列表
	GLuint Vao, Vbo, Ebo;//glbuffer
	unsigned int VertexBufferSize, IdxBufferSize;//顶点缓冲的Vtx Idx属性
};
struct ShowMenuItem
{
	const char* name=nullptr;//子条目的名称
	ClipRect ItemRect;//子条目对应的文本框
	bool* ShowOtherContext=nullptr;//是否允许开启其他内容显示判断标志
};
struct ShowMenu//Menu{ 菜单栏裁剪矩形，悬浮窗裁剪矩形(HoverRect)、DynamicArray<MenuItem>MenuItemStack存储当前子条目、颜色、开启子条目显示标志 }
{
	ShowMenu()//构造函数
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
	ClipRect MenuRect;//菜单栏裁剪矩形
	ClipRect HoverRect;//悬浮窗裁剪矩形
	ClipRect ItemScrollbar;//菜单栏的子条目的滚动条
	glm::vec2 MenuItemOffset;//子条目的滚动偏移位置
	DynamicArray<ShowMenuItem>MenuItemStack;//存储当前子条目
	glm::vec4 MenuCol;//菜单栏颜色
	glm::vec4 ScrollbarCol;//滚动条的颜色
	glm::vec2 ScrollBarWH;//当前菜单栏的滚动条的宽高
	bool EnableMenuItem;//开启子条目显示标志 
	bool Hoverable;//是否允许悬浮窗显示 悬浮标志
};//菜单栏
struct ShowWindowContainer
{
	
	ShowWindowContainer()//初始化显示窗口的各个参数
	{
		this->Pos= glm::vec2(0, 0);//显示窗口绘制位置
		this->content.drawlist = DrawList();
		this->cursorPos = glm::vec2(0,0);
		
		this->TitleRect = ClipRect(this->Pos, this->Pos + glm::vec2(460, 70));
		this->OutterRect = ClipRect(this->Pos, this->Pos + glm::vec2(560, 590));//初始化自己窗口的外部裁剪矩形

		this->COLOR[TitleRectCol] = glm::vec4(1,0,1,1);
		this->COLOR[OutterRectCol] = glm::vec4(0.4, 0, 0.2, 1);
		this->COLOR[TextCol] = glm::vec4(1.0, 1.0, 1.0, 1);

		this->content.VertexBufferSize = 0;//自己窗口存储的顶点个数
		this->content.IdxBufferSize = 0;//自己窗口存储的索引绘制个数
		this->ScaleX = 1.0f;//初始化自己窗口的缩放大小
		this->ScaleY = 1.0f;
		this->spacing = 2.0f;
		//设置自己窗口的各种判断事件的判断标志
		this->Flag.AllowResponseInputEvent = false;
		this->Flag.Movable = true;
		this->Flag.Bring_Front = false;
		this->Flag.IsWindowVisible = false;
		//默认菜单栏显示方式为水平方式
		this->MenuShowOrder = HorizontalShow;
		this->MenuArray.resize(0);
	}
	ShowMenu* CreateMenu(const char* name);
	ShowMenu* FindMenuByName(const char*name);//从菜单映射表找出菜单指针
	struct ShowWindowflag
	{
		bool AllowResponseInputEvent = false;//是否允许当前窗口响应事件 防止叠加的窗口都在响应输入事件 我们只允许叠加的窗口中置顶窗口响应事件
		bool Movable = true;//是否窗口可以允许移动
		bool Bring_Front = false;//是否自身窗口置顶显示
		bool IsWindowVisible = true;//是否允许窗口显示即是否将本身窗口绘制资源顶点发送给GPU
	};
	RBTreeMap<const char*, ShowMenu*>MenuMap;//当前窗口在菜单映射表
	DynamicArray<ShowMenu*>MenuArray;//当前窗口的存储各个菜单栏数组
	DynamicArray<ShowMenu*>MenuStack;//当前窗口在菜单栏在栈 只能存储一次再弹出
	glm::vec2 Pos ;//显示窗口绘制位置
	glm::vec2 cursorPos;//每一个绘制内容的跟踪位置
	
	ClipRect TitleRect;//标题框裁剪矩形
	ClipRect OutterRect;//自家窗口的外部裁剪矩形
	glm::vec4 COLOR[6];//不同在内容对应的颜色

	gl_Content content; // 自家窗口的绘制资源 自家窗口的绘制列表 每个窗口都有自己的glbuffer 自己窗口的顶点缓冲的Vtx Idx属性
	ShowWindowflag Flag;//自己窗口的一些事件判定标志
	MenuShowOrderType MenuShowOrder;//菜单栏的显示方式 水平还是垂直方式
	float spacing;//显示内容相互之间的距离
	float ScaleX, ScaleY;//自己窗口的绘制内容的缩放比例
};
struct DrawData
{
	glm::vec2 DisplayPos;//视口显示位置
	glm::vec2 DisplaySize;//视口屏幕大小 也是正交投影的视口
	glm::vec2 framebufferscale;//帧缓冲视口缩放大小
	DynamicArray<ShowWindowContainer*>DrawDataLayerBuffer;//存储每个窗口图层的绘制资源 和一些判断状态  改成这样不用像之前那样再访问遍历窗口容器再进行拷贝
};

class UI
{
public:
	void StartShowWindow(const char* Text, bool Popen);//Popen控制窗口是否允许显示
	void BgText(const char* Text);//绘制背景的文本
	void StartMenu(MenuShowOrderType ShowOrder);//Horizontal  Vertical菜单栏是竖直摆放 还是水平摆放 枚举类型
	void Menu(const char*MenuName,bool enableMenuItem);//显示菜单栏
	void MenuItem(const char*ItemName,bool*ShowotherContextFlag);//为菜单栏添加子条目
	void EndMenu();//计算好所有子条目的参数 总的判断事件标志 绘制子条目的文本框和文本 最后再弹出更新的内存
	void EndShowWindow();//从当前窗口弹开方便下一个窗口的子条目绘制因为要访问目前窗口的位置
private:
	ShowWindowContainer* CreateNewShowWindow(const char* name);//只允许调用一次
	ShowWindowContainer* FindShowWindowByName(const char* name);
protected:
    RBTreeMap<const char*, ShowWindowContainer*>NameToShowWindowMap;//标题名映射到这个标题显示窗口的地址
	ShowWindowContainer* LastShowWindow = nullptr;//只有在第一次窗口位置的赋值时才需要访问
	DynamicArray<ShowWindowContainer*>CurrentShowWindowStack;//方便窗口的子条目绘制
};
class BuiltIO
{
public:
	void CreateFontTexture();
	void CreateShowWindow_glBuffer(ShowWindowContainer* Show_Window);
	void UpdateDrawDataBuffer(UI* DrawUI);//更新绘制数据顶点
	void RenderDrawList();
	void DeleteResource();//释放内存
	DrawData drawdata;
	FontAtlas atlas;
	InputEventParam InputMessage;//输入响应消息
	glm::vec2 ArcFastLookUpTable[48];//存储cos sin值
	glm::vec4 Colors[8];//一些绘制内容在默认颜色
	bool Has_Bring_front_opera=false;//是否有置顶窗口的操作
protected:
	shaderProgram* shader = nullptr;
	glm::mat4 orthoProjection;
	unsigned int TexId;
private:
	void initializeArcTable();
	void SetUpBufferState(GLuint vertex_object_array, GLuint vertex_Buffer_object, GLuint element_object_object);
	void Bring_TopDrawDataLayer_ToFrontDisplay();//更新置顶层绘制窗口绘制资源 窗口进行置顶操作 本质是将窗口的存放索引改变了 GPU顶点绘制顺序改变了
	void AllowDrawDataLayerResponseEvent();//是否绘制图层响应事件输入 是否允许当前窗口响应事件 防止叠加的窗口都在响应输入事件 我们只允许叠加的窗口中置顶窗口响应事件
};

//C函数代码
void SetUpContextIO();// io设置
void BuildBackendData();//建立所需要的字体纹理和顶点资源
BuiltIO* GetIo();//返回IO访问地址
void UpdateEvent(GLFWwindow* window);
void ClearEvent();
void RenderDrawData(UI* DrawUI);//绘制添加进来的绘制数据
void DestructResource();//释放内存