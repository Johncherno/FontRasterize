#include"TruetypeFont.h"
#include"internal.h"
#include"fontPack.h"
#include"FontStructure.h"
enum
{
	vStartNextCountour,
	vCurve,
	vline
};
enum flagBit
{
	Point_On_glyphCurveBit,
	X_Position_BytesBit,
	Y_Position_BytesBit,
	RepeatCountBit,
	X_Position_SignBit,
	Y_Position_SignBit
};
struct Glyph_Vertex
{
	truetype_int16 x, y, Lastx, Lasty;//signed short占两个字节
	unsigned char VertType;
};
struct GlyphShape_Point
{
	float x, y;
};
struct GlyphShape_edge
{
	float x0;
	float y0;//永远是边的最低点
	float x1;
	float y1;
	int Dir;
};//光栅化边
struct GlyphShape_active_edge
{
	GlyphShape_active_edge* next;
	float direction;
	float slope_dxFordy;//x对y斜率
	float slope_dyFordx;//y对x斜率
	float fx;//top扫描线与这条边的交点的x
	float Miny;
	float Maxy;
};//活化边
struct PackedChar
{
	float x0InTexMap, y0InTexMap, x1InTexMap, y1InTexMap;
	float CharBox_xoff1, CharBox_yoff1, CharBox_xoff2, CharBox_yoff2;
	float advanceX;
	unsigned int CodePoint : 30;//ASCll索引
};
struct Align_quad
{
	float x0, y0, s0, t0;
	float x1, y1, s1, t1;
};
//PackedCharRange存储一堆字符色块的结构体信息
//FontGlyph:U0=x0InTexMaP/TexWidth  V0=y0InTexMaP/TexHeight .....同理
//FontGlyph:X0=CharBox_xoff1 Y0=CharBox_yoff1.....同理后面加上单个字符色块的宽高
struct PackedCharRange
{
	float FontSize = 0;//字体大小
	unsigned int num_chars = 0;//多少个字符
	unsigned int* Array_of_UnicodePoint = nullptr;//方便通过访问它对FontInfo字体字形索引的检索
	PackedChar* PackedCharCoordData_for_Range = nullptr;//存储一系列字符色块的非0-1空间下的纹理采样坐标及Box的偏移坐标
};
struct PackFontTexture_Context
{
	unsigned char* pixel;//图像像素地址
	//unsigned int Width;//图像宽度
	unsigned int Height;//图像高度
	unsigned int stride_in_bytes;//涵盖这个小图块总宽度
	float padding;//纹理字符之间的间距
};//整个图像的结构体变量
struct PackRectTexture_Context
{
	unsigned char* Rectpixel;//单个字符色块的渲染地址
	unsigned int CharRectWidth;//单个字符色块宽度
	unsigned int CharRectHight;//单个字符色块高度
	unsigned int stride_in_bytes;//偏移量
};

struct BuildFontSrcData//字体创建结构信息
{
	ttfBuildFontInfo FontInfo;//字体信息
	PackedRect* Rect = nullptr;//存储一系列字符方块Rect
	PackedCharRange packedChar_for_range;//存储字符的codepoint索引 还存储多少个字符 存储着每个字符的非0-1的采样坐标及Box的偏移位置
	Char16* SrcCodePointRange = nullptr;//字符索引的前后范围 [0]为首 [1]为尾
	DynamicArray<unsigned int>glyphList_of_CodePoint;
	unsigned int GlyphCount = 0;//多少个字符
	unsigned  int GlyphLastCodePoint = 0;//最后一个字符codepoint索引
	BitVector GlyphListCodePointBitArray;//首先create 再SetBit
};
#define Col(R,G,B,A) (((unsigned int)(A)<<24) | ((unsigned int)(B)<<16) | ((unsigned int)(G)<<8) | ((unsigned int)(R)<<0))
static void UnpackbitVectorToGlyphList(BitVector* in, DynamicArray<unsigned int>* out)
{
	DynamicArray<unsigned int>::iterator it_begin = in->Storage.begin();
	DynamicArray<unsigned int>::iterator it_end = in->Storage.end();
	for (DynamicArray<unsigned int>::iterator it = it_begin; it < it_end; it++)
		if (unsigned int Var = *it)
			for (unsigned int BitCount = 0; BitCount < 32; BitCount++)
				if (Var & (1 << BitCount))//试试每一位是否为真 因为之写入是每一位都或1了
					out->push_back(((unsigned int)(it - it_begin) << 5) + BitCount);//偏移位置*32+32次循环的循环数  从32开始到255
}
static void GetPackedCharQuadCoord(PackedChar* packedCharCoord, float TexMapwidth, float TexMapheight, Align_quad* q)
{

	q->x0 = packedCharCoord->CharBox_xoff1;
	q->y0 = packedCharCoord->CharBox_yoff1;
	q->x1 = packedCharCoord->CharBox_xoff2;
	q->y1 = packedCharCoord->CharBox_yoff2;
	q->s0 = packedCharCoord->x0InTexMap / TexMapwidth;
	q->t0 = packedCharCoord->y0InTexMap / TexMapheight;
	q->s1 = packedCharCoord->x1InTexMap / TexMapwidth;
	q->t1 = packedCharCoord->y1InTexMap / TexMapheight;
}
static void SetGlyphShapeVertex(Glyph_Vertex* v, unsigned int type, short x, short y, short cx, short cy)
{
	v->VertType = type;
	v->x = x;
	v->y = y;
	v->Lastx = cx;
	v->Lasty = cy;
}
static bool FlagBitIsSet(truetype_uint8 flag, unsigned int bitIndex)
{
	return ((flag >> bitIndex) & 1) == 1;
}
static void CloseGlyphShape(Glyph_Vertex* Vertex, bool LastPoint_On_glyphCurve, unsigned int* Num_verts, float startX, float startY, float LastX, float LastY, float scx, float scy, bool StartPoint_On_glyphCurve)
{
	// 检查最后一个点和开始点是否重复，如果重复则直接返回
	if (LastX == startX && LastY == startY) {
		return; // 避免重复插入点
	}
	// 判断轮廓结束点和开始点的状态
	if (!LastPoint_On_glyphCurve) { // 如果闭合前最后一个点不在字形线上
		if (!StartPoint_On_glyphCurve) { // 如果开始点也不在字形线上
			// 1. 闭合前的点不在曲线上，起始点也不在曲线上
			SetGlyphShapeVertex(&Vertex[*Num_verts], vCurve, (LastX + scx) / 2, (LastY + scy) / 2, LastX, LastY);
			(*Num_verts)++;
			SetGlyphShapeVertex(&Vertex[*Num_verts], vCurve, (scx + startX) / 2, (scy + startY) / 2, scx, scy);
		}
		else {
			// 2. 闭合前的点不在曲线上，起始点在曲线上
			SetGlyphShapeVertex(&Vertex[*Num_verts], vCurve, startX, startY, LastX, LastY);
		}
	}
	else {
		// 如果闭合前最后一个点在曲线上
		if (!StartPoint_On_glyphCurve) {
			// 3. 闭合前的点在曲线上，起始点不在曲线上
			SetGlyphShapeVertex(&Vertex[*Num_verts], vCurve, (scx + startX) / 2, (scy + startY) / 2, scx, scy);
		}
		else {
			// 4. 闭合前的点在曲线上，起始点也在曲线上
			SetGlyphShapeVertex(&Vertex[*Num_verts], vline, startX, startY, 0, 0);
		}
	}
	(*Num_verts)++;
}
//获取字形闭合曲线数据
static unsigned int GetGlyphCloseShape(ttfBuildFontInfo* fontInfo, unsigned int glyphIndex, Glyph_Vertex** v)
{
	unsigned  int Num_verts = 0, NumPoints_ReadFromFontInfo = 0, TotalLength_Vertices = 0, InstructionArrayLength = 0;//返回的多少个点 字形多少个边 记录字形文件中有多少个控制点
	signed short NumberOfContours = 0;
	truetype_uint8* data = fontInfo->data;//字体文件地址
	int offset = ttfFontGetGlyphOffset(fontInfo, glyphIndex);//字形偏移量
	if (offset < 0)return 0;
	//std::cout << offset << "offset" << std::endl;
	unsigned int Store_CharglyphBoundingBox_And_NumContours_Bytes = 10;
	truetype_uint8* endptrOfContours = data + offset + Store_CharglyphBoundingBox_And_NumContours_Bytes;//移动10个字节是因为10个字节中存储着字符形的包围盒Box
	NumberOfContours = GetPointerTwoBytes_int16_Content(data + offset);//在这个位置读出来多少个单个字形轮廓数

	InstructionArrayLength = GetPointerTwoBytes_uint16_Content(data + offset + Store_CharglyphBoundingBox_And_NumContours_Bytes + NumberOfContours * 2);
	truetype_uint8* Points = endptrOfContours + NumberOfContours * 2 + 2 + InstructionArrayLength;//这个位置为控制点数组的首地址  需要跳过存储指令长度的内容长度 跳过存储指令数组的内容长度 
	Glyph_Vertex* glyph_vertex_Array = nullptr;//待返回的顶点数组
	//NumPoints_ReadFromFontInfo记录多少个控制点
	NumPoints_ReadFromFontInfo = GetPointerTwoBytes_uint16_Content(endptrOfContours + (NumberOfContours - 1) * 2) + 1;//字形地址偏移+字形轮廓数-1 记录数组的n-1下标 需要再加1为最后的点数量   NumberOfContours减1的目的是预留一个2个字节的空间读取
	TotalLength_Vertices = NumberOfContours * 2 + NumPoints_ReadFromFontInfo;//glyph_vertex_Array顶点数组的总长度 上一部分用来填进处理好的控制点  下一部分用来读取ttf字形信息的控制点
	glyph_vertex_Array = (Glyph_Vertex*)malloc(TotalLength_Vertices * sizeof(Glyph_Vertex));
	if (glyph_vertex_Array == nullptr)
		return 0;
	unsigned int UpperLength_glyph_vertex_Array = NumberOfContours * 2;//glyph_vertex_Array上半部分的长度
	//第一部分是加载点的类型 flag
	// X X X X X X X X
	// X X  y的相对偏移位置是否为负数(0代表负数在一个字节时成立)(2个字节时 0代表正的位置 1无意义)    x的相对偏移位置是否为负数(0代表负数在一个字节时成立)(2个字节时 0代表正的位置 1无意义)  这个字节的点类型位成立那么下一个字节存储重复本字节点类型的个数  y(偏移位置存储的字节数)(1为一个字节)  x(偏移位置存储的字节数)(1为一个字节)  点是否在字形曲线上
	truetype_uint8 flag_RepeatCount = 0, flag = 0;
	if (NumberOfContours > 0)
	{
		//先加载flag
		for (unsigned int i = 0; i < NumPoints_ReadFromFontInfo; i++)
		{
			if (flag_RepeatCount == 0)
			{
				flag = *Points++;
				if (FlagBitIsSet(flag, RepeatCountBit))
				{
					flag_RepeatCount = *Points++;//代表着剩下多少个点类型一样
				}
			}
			else
			{
				flag_RepeatCount--;
			}
			glyph_vertex_Array[UpperLength_glyph_vertex_Array + i].VertType = flag;
		}
		//再加载xcoordinate
		truetype_int16 VariableX = 0;
		for (unsigned int i = 0; i < NumPoints_ReadFromFontInfo; i++)
		{
			flag = glyph_vertex_Array[UpperLength_glyph_vertex_Array + i].VertType;
			if (FlagBitIsSet(flag, X_Position_BytesBit))//如果是一个字节
			{
				truetype_int16 RelativeDeltaX = *Points++;
				VariableX += FlagBitIsSet(flag, X_Position_SignBit) ? RelativeDeltaX : -RelativeDeltaX;//再判断是负数的位移还是正数的位移
			}
			else//如果为两个字节 0代表正的两个字节位置 1无意义
			{
				if (!FlagBitIsSet(flag, X_Position_SignBit))//0代表正的两个字节位置 
				{
					VariableX += GetPointerTwoBytes_int16_Content(Points);
					Points += 2;//空出2个字节 以便下一个循环
				}
			}
			glyph_vertex_Array[UpperLength_glyph_vertex_Array + i].x = VariableX;
		}
		//再加载ycoordinate
		truetype_int16 VariableY = 0;
		for (unsigned int i = 0; i < NumPoints_ReadFromFontInfo; i++)
		{
			flag = glyph_vertex_Array[UpperLength_glyph_vertex_Array + i].VertType;
			if (FlagBitIsSet(flag, Y_Position_BytesBit))//如果是一个字节
			{
				truetype_int16 RelativeDeltaY = *Points++;
				VariableY += FlagBitIsSet(flag, Y_Position_SignBit) ? RelativeDeltaY : -RelativeDeltaY;//再判断是负数的位移还是正数的位移
			}
			else//如果为两个字节 0代表正的两个字节位置 1无意义
			{
				if (!FlagBitIsSet(flag, Y_Position_SignBit))//0代表正的两个字节位置 
				{
					VariableY += GetPointerTwoBytes_int16_Content(Points);
					Points += 2;//空出2个字节 以便下一个循环
				}
			}
			glyph_vertex_Array[UpperLength_glyph_vertex_Array + i].y = VariableY;
		}
		//再将它们处理成我们想要的顶点数组
		//之后是要判断是否是在字形线上
		unsigned int Next_StartContour_index = 0, j = 0;
		truetype_int16 StartX = 0, StartY = 0, LastPx = 0, LastPy = 0, scx = 0, scy = 0;
		bool IsLastPoint_OnGlyphCurve = false;//用来判断上一个点是否在字形线上
		bool StartPoint_OnGlyphCurve = false;//用来轮廓开始的点是否在字形线上
		for (unsigned int i = 0; i < NumPoints_ReadFromFontInfo; i++)
		{
			flag = glyph_vertex_Array[UpperLength_glyph_vertex_Array + i].VertType;
			VariableX = glyph_vertex_Array[UpperLength_glyph_vertex_Array + i].x;
			VariableY = glyph_vertex_Array[UpperLength_glyph_vertex_Array + i].y;
			if (i == Next_StartContour_index)//下一个轮廓线开始的索引
			{

				if (i != 0)//最后需要闭合字形曲线 也就是到了单个轮廓线结束后(在下一个轮廓线处理程序进入前需要闭合) 当最后一个点处在不在字形线上就没有形成弧线的包围直线就没加载到顶点数组中需要考虑 当处在字形线上那就加载了不用管 我们需要最后一个点V[最后].(x,y)<-->V[0](即轮廓开始的点)   V[最后].(Lastx,Lasty)--->不在曲线上的 闭合之前的点
				{//当第一个轮廓线开始不需要执行
					CloseGlyphShape(glyph_vertex_Array, IsLastPoint_OnGlyphCurve, &Num_verts, StartX, StartY, LastPx, LastPy, scx, scy, StartPoint_OnGlyphCurve);
				}
				StartPoint_OnGlyphCurve = FlagBitIsSet(flag, Point_On_glyphCurveBit);
				if (!StartPoint_OnGlyphCurve)//如果轮廓开始的点不在字形线上
				{
					//那就接着判断下一个点是否在字形线上
					if (!FlagBitIsSet(glyph_vertex_Array[UpperLength_glyph_vertex_Array + i + 1].VertType, Point_On_glyphCurveBit))//如果下一个点也不在字形线上 那就求出两点的中点弧线切点 
					{
						StartX = (glyph_vertex_Array[UpperLength_glyph_vertex_Array + i + 1].x + VariableX) >> 1;//右移是除以2求中点的意思
						StartY = (glyph_vertex_Array[UpperLength_glyph_vertex_Array + i + 1].y + VariableY) >> 1;

					}
					else//如果下一个点在字形线上 使用下个点作为轮廓线开始的点
					{
						StartX = glyph_vertex_Array[UpperLength_glyph_vertex_Array + i + 1].x;
						StartY = glyph_vertex_Array[UpperLength_glyph_vertex_Array + i + 1].y;
					}
				}
				else
				{
					StartX = VariableX;
					StartY = VariableY;
				}
				scx = VariableX;
				scy = VariableY;
				SetGlyphShapeVertex(&glyph_vertex_Array[Num_verts++], vStartNextCountour, StartX, StartY, 0, 0);
				Next_StartContour_index = GetPointerTwoBytes_uint16_Content(endptrOfContours + j * 2) + 1;//轮廓结束指针加j*2的原因是因为是2个字节   再加上1是要索引到下一个轮廓开始点索引 因为本身这个位置存储本次的轮廓结束顶点的索引
				j++;//当条件成立时继续遍历存储轮廓结束数组
				IsLastPoint_OnGlyphCurve = true;//代表开始点是要在字形线上的
			}
			else//其他就是轮廓非开始点 
			{
				if (!FlagBitIsSet(flag, Point_On_glyphCurveBit))//如果轮廓当前这个点不在字形线上
				{
					//况且上一个点也不在字形线上
					if (!IsLastPoint_OnGlyphCurve)
					{
						SetGlyphShapeVertex(&glyph_vertex_Array[Num_verts++], vCurve, (LastPx + VariableX) >> 1, (LastPy + VariableY) >> 1, LastPx, LastPy);
					}
					//记录本次点顶点数据和顶点是否在字形线的状态 用于下一次循环的点获取上一点的信息
					LastPx = VariableX;
					LastPy = VariableY;
					IsLastPoint_OnGlyphCurve = false;//代表当前的点不在字形线上 方便下一次循环的点的获取调用
				}
				else//如果轮廓当前这个点在字形线上
				{
					//并且上一个点不在字形线上
					if (!IsLastPoint_OnGlyphCurve)
					{
						SetGlyphShapeVertex(&glyph_vertex_Array[Num_verts++], vCurve, VariableX, VariableY, LastPx, LastPy);//这两个点配对 与上上个点进行点的插值计算
					}
					else//上一个点也在字形线上 那就是一条线
					{
						SetGlyphShapeVertex(&glyph_vertex_Array[Num_verts++], vline, VariableX, VariableY, 0, 0);
					}
					IsLastPoint_OnGlyphCurve = true;//代表当前的点在字形线上
				}

			}
		}

		//最后需要闭合字形曲线 也就是到了单个轮廓线结束后(在下一个轮廓线处理程序进入前需要闭合) 当最后一个点处在不在字形线上就没有形成弧线的包围直线就没加载到顶点数组中需要考虑 当处在字形线上那就加载了不用管 我们需要最后一个点V[最后].(x,y)<-->V[0](即轮廓开始的点)   V[最后].(Lastx,Lasty)--->不在曲线上的 闭合之前的点
		// 闭合最后一个轮廓
		CloseGlyphShape(glyph_vertex_Array, IsLastPoint_OnGlyphCurve, &Num_verts, StartX, StartY, LastPx, LastPy, scx, scy, StartPoint_OnGlyphCurve);
	}
	/*  为什么要两次调用？
		 第一次（next_move 判断）：

		 确保中间轮廓在起点切换时闭合。
		 避免中间轮廓与新轮廓数据混淆，保证数据的连续性。
		 第二次（for 循环结束后）：

		 确保最后一个轮廓也能正确闭合。
		 遍历到最后一个点时，没有 next_move 的触发，需要手动闭合。*/
	*v = glyph_vertex_Array;
	return Num_verts;
}
static void AddPoint(GlyphShape_Point* dst, unsigned int Index, float x, float y)
{
	if (!dst)return;
	dst[Index].x = x;
	dst[Index].y = y;
}
static void Tessalate_curve(GlyphShape_Point* Pt, unsigned int* numPoints, float objactness, float firstX, float firstY, float secondX, float secondY, float thirdX, float thirdY, unsigned int recurse_depth)//下一个问题是顶点的下一个点 下下个点该怎么给
{
	//glm::vec2 MidOnP0AndP1 = 0.5f * (P0 + P1);
	//glm::vec2 MidOnP1AndP2 = 0.5f * (P1 + P2);
	//glm::vec2 Middle = (MidOnP0AndP1 + MidOnP1AndP2) * 0.5f;
	//glm::vec2 MidOnP0AndP2 = 0.5f * (P0 + P2);
	////std::cout << *numPoints << std::endl;
	////计算p0和p2中点和Middle之间的距离作为推出递归的结束标志返回推出堆栈
	//float DistanceSquare = glm::length(MidOnP0AndP2 - Middle);
	//if (DistanceSquare > 0.795f)
	//{
	//	//左半部 左子节点
	//	tessalate_curve(Pt, numPoints, P0, MidOnP0AndP1, Middle);
	//	//右半部 右子节点
	//	tessalate_curve(Pt, numPoints, Middle, MidOnP1AndP2, P2);
	//}
	//else
	//{

	//	AddPoint(Pt, *numPoints, P2);
	//	(*numPoints)++;

	//}
	float middleX = ((firstX + secondX) / 2 + (thirdX + secondX) / 2) / 2;//二次贝塞尔曲线
	float middleY = ((firstY + secondY) / 2 + (thirdY + secondY) / 2) / 2;
	float dx = (firstX + thirdX) / 2 - middleX;
	float dy = (firstY + thirdY) / 2 - middleY;
	if (recurse_depth > 16)//限制递归深度
		return;
	if (dx * dx + dy * dy >= objactness * objactness)//计算p0和p2中点和Middle之间的距离作为推出递归的结束标志返回推出堆栈 意思是只要不满足相应距离条件就一直遍历二叉树节点
	{
		//左半部 左子节点
		Tessalate_curve(Pt, numPoints, objactness, firstX, firstY, (firstX + secondX) / 2, (firstY + secondY) / 2, middleX, middleY, recurse_depth + 1);
		//右半部 右子节点
		Tessalate_curve(Pt, numPoints, objactness, middleX, middleY, (thirdX + secondX) / 2, (thirdY + secondY) / 2, thirdX, thirdY, recurse_depth + 1);
	}
	else
	{
		AddPoint(Pt, *numPoints, thirdX, thirdY);
		(*numPoints)++;
	}

}
static GlyphShape_Point* FlattenIntoCurve(Glyph_Vertex* v, unsigned int** windingCount, float objactness, unsigned int* windingLength, unsigned int vertCount)//判断点的类型如果直接移动就放 如果是曲线类型就贝塞尔插值
{
	//第一次是计算总共多少点
	//第二次分配malloc这些数量到一维顶点数组指针并且放入这个数组指针中
	GlyphShape_Point* ReturnPoints = nullptr;
	int n = 0;
	float x = 0, y = 0;
	for (unsigned int i = 0; i < vertCount; i++)//这部分记录有多少个轮廓
		if (v[i].VertType == vStartNextCountour)
			n++;
	*windingCount = (unsigned int*)malloc(sizeof(**windingCount) * n);
	*windingLength = n;//记录好了环绕轮廓线

	unsigned int NumPoints = 0, startNumPoint = 0;
	for (unsigned int PassCount = 0; PassCount < 2; PassCount++)
	{
		if (PassCount == 1)
		{
			ReturnPoints = (GlyphShape_Point*)malloc(NumPoints * sizeof(GlyphShape_Point));
		}
		NumPoints = 0;
		n = -1;
		for (unsigned int i = 0; i < vertCount; i++)
		{
			if (v[i].VertType == vStartNextCountour)//代表一个轮廓线的开始点 在处理之前先把之前的轮廓进行处理边数进行填充
			{
				if (n >= 0)
					(*windingCount)[n] = NumPoints - startNumPoint;//开始为轮廓环绕线的边数组进行赋值
				++n;
				startNumPoint = NumPoints;
				x = v[i].x;
				y = v[i].y;
				AddPoint(ReturnPoints, NumPoints++, x, y);
			}
			else if (v[i].VertType == vline)//如果是直线直接放进去
			{
				x = v[i].x;
				y = v[i].y;
				AddPoint(ReturnPoints, NumPoints++, x, y);
			}
			else if (v[i].VertType == vCurve)//如果是曲线进行插值
			{

				Tessalate_curve(ReturnPoints, &NumPoints, objactness,
					x, y,
					v[i].Lastx, v[i].Lasty,
					v[i].x, v[i].y, 0);

				x = v[i].x;
				y = v[i].y;
			}
		}
		(*windingCount)[n] = NumPoints - startNumPoint;
	}

	return ReturnPoints;
}
#define GlyphShape_edgeCompareOnSmaller(a,b) ((a).y0<(b).y0) //比较边那个高度更低
static void swapForGlyphShape_edge(GlyphShape_edge& P, GlyphShape_edge& Y)
{
	GlyphShape_edge temp{};
	temp = P;
	P = Y;
	Y = temp;
}
static void GlyphShape_edge_sort_ins(GlyphShape_edge* p, unsigned int n)//冒泡排序
{
	for (unsigned int range = 0; range < n - 1; range++)
	{
		for (unsigned int index = 0; index < n - 1 - range; index++)//n-1存在的意义在于怕数组越界
		{

			if (GlyphShape_edgeCompareOnSmaller(p[index + 1], p[index]))
				swapForGlyphShape_edge(p[index], p[index + 1]);
		}
	}
}
static void GlyphShape_edge_quick_sort(GlyphShape_edge* p, int n)//快速排序  形参为T*a 传入一维数组会改变一维数组的元素内容 存在的意义是减小排序的时长提高效率
{
	while (n > 12)
	{
		int middle, Compare0AndMiddle = 0, CompareMiddleAndend = 0, Compare0Andend = 0, z = 0, beginPointer = 0, endPointer = 0;
		middle = (n >> 1);//middle=n/2
		Compare0AndMiddle = GlyphShape_edgeCompareOnSmaller(p[0], p[middle]);//p[0].y0与p[middle].y0比较哪个小
		CompareMiddleAndend = GlyphShape_edgeCompareOnSmaller(p[middle], p[n - 1]);//p[middle].y0与p[n-1].y0比较哪个小
		if (Compare0AndMiddle != CompareMiddleAndend)//如果一个小 一个大 就代表不是递减 递增关系 需要将三个元素进行递增减关系排布
		{
			Compare0Andend = GlyphShape_edgeCompareOnSmaller(p[0], p[n - 1]);
			z = (Compare0Andend == CompareMiddleAndend) ? 0 : n - 1;//如果p[middle].y0>p[n-1].y0与p[0]>p[n-1].y0符号同样 
			//就执行p[middle]与p[0]交换 如果不是就执行p[middle]与p[n-1]交换
			swapForGlyphShape_edge(p[middle], p[z]);
		}
		//将middle与0号位置元素进行交换
		swapForGlyphShape_edge(p[0], p[middle]);//p[0]存储着关键分界值
		float keyValue = p[0].y0;
		beginPointer = 1;
		endPointer = n - 1;
		while (beginPointer < endPointer && beginPointer < n - 1 && endPointer >= 0)//如果开头指针大于等于结尾指针就代表重合就结束这个循环 否则就继续
		{
			while (p[beginPointer].y0 < keyValue)//如果beginPointer++同时找到了比分界值大的就停止循环 进行下面操作
			{
				beginPointer++;
			}
			while (p[endPointer].y0 > keyValue)//如果endPointer--同时找到了比分界值小的就停止循环 进行下面操作
			{
				endPointer--;
			}
			swapForGlyphShape_edge(p[beginPointer], p[endPointer]);//交换彼此代表 关键值左边小 右边大
			beginPointer++;
			endPointer--;//跳过已经交换数值的位置元素
		}
		if (endPointer < n - beginPointer)//小边进行递归
		{
			GlyphShape_edge_quick_sort(p, endPointer);//对p+0-p+endPointer的区间边进行快速递归排序
			//递推结束后回归到此再处理剩下的p+endpointer-p+n区间的边
			p += beginPointer;
			n -= beginPointer;
		}
		else
		{
			GlyphShape_edge_quick_sort(p + beginPointer, n - beginPointer);//对p+endpointer-p+n区间的边进行快速递归排序
			n = endPointer;//递推结束后回归到此再处理p+0-p+endPointer区间的边
		}
	}
}
static void sort_tessaEdge(GlyphShape_edge* p, unsigned int n)
{
	GlyphShape_edge_quick_sort(p, n);//先快速排序
	GlyphShape_edge_sort_ins(p, n);//然后冒泡排序
}
static float trapzoid_Area(float height, float tx0, float tx1, float bx0, float bx1)//梯形面积
{
	return height * ((tx1 - tx0) + (bx1 - bx0)) * 1 / 2;
}
static float triangle_Area(float height, float width)//三角形面积
{
	return height * width / 2;
}
static void Clip_edge_Fill(float* scaneline, int FillX, float x0, float y0, float x1, float y1, GlyphShape_active_edge* e)
{
	if (y1 <= y0) return; // 忽略无高度的边
	if (e->Miny >= y1) return; // 边完全在扫描线下方
	if (e->Maxy <= y0) return; // 边完全在扫描线上方

	// 限制 x0 和 x1 的范围，防止超出界限
	if (e->Miny > y0) {
		x0 += (x1 - x0) * (e->Miny - y0) / (y1 - y0);
		y0 = e->Miny;
	}
	if (e->Maxy < y1) {
		x1 += (x1 - x0) * (e->Maxy - y1) / (y1 - y0);
		y1 = e->Maxy;
	}

	// 确保 x0 和 x1 在合理范围内
	if (x0 < 0.0f) x0 = 0.0f;
	if (x1 < 0.0f) x1 = 0.0f;

	if (x0 > FillX + 1 && x1 > FillX + 1) {
		// 不影响当前像素，直接跳过
		return;
	}

	if (x0 <= FillX && x1 <= FillX) {
		scaneline[FillX] += e->direction * (y1 - y0);
	}
	else if (x0 >= FillX && x1 <= FillX + 1) {
		scaneline[FillX] += e->direction * (y1 - y0) * (1.0f - ((x0 - FillX) + (x1 - FillX)) / 2);
	}

}
static void fill_GlyphCloseShape_activeEdge(float* scanlineOnEdge, float* scanlineFillOffEdge, unsigned int width, GlyphShape_active_edge* e, float y_top)//填充活化边
{

	float scan_y_top = y_top;
	float scan_y_bottom = scan_y_top + 1;
	while (e)
	{

		if (e->slope_dxFordy == 0)//如果是竖直线
		{
			float x0 = e->fx;//ytop线与e边交点x值
			if (x0 < width)
			{
				if (x0 >= 0)
				{
					//填充scanline与矢量字体线有交点的像素
					Clip_edge_Fill(scanlineOnEdge, (int)x0, x0, scan_y_top, x0, scan_y_bottom, e);

					//填充scanlinefill不与矢量字体线有交点的像素  交点的下一个位置
					Clip_edge_Fill(scanlineFillOffEdge, (int)(x0)+1, x0, scan_y_top, x0, scan_y_bottom, e);
				}
				else
				{
					//否则小于0 从0号位置开始填充
					Clip_edge_Fill(scanlineFillOffEdge - 1, 0, x0, y_top, x0, scan_y_bottom, e);
				}

			}

		}
		else//斜线
		{

			float scan_y_top_intersectEdge_X = 0.0f, scan_y_bottom_intersectEdge_X = 0.0f, step = 0.0f;
			float EdgeSlope_dxFordy = 0.0f, EdgeSlope_dyFordx = 0.0f;
			float sampleY0 = 0.0f, sampleY1 = 0.0f;//交点采样坐标
			float sampleX0 = 0.0f, sampleX1 = 0.0f;
			scan_y_top_intersectEdge_X = e->fx;
			scan_y_bottom_intersectEdge_X = scan_y_top_intersectEdge_X + e->slope_dxFordy;
			EdgeSlope_dxFordy = e->slope_dxFordy;
			EdgeSlope_dyFordx = e->slope_dyFordx;

			if (e->Miny > scan_y_top)//边的最低点大于扫描线top高度
			{
				sampleX0 = scan_y_top_intersectEdge_X + (e->Miny - scan_y_top) * EdgeSlope_dxFordy;
				sampleY0 = e->Miny;
			}
			else
			{
				sampleX0 = scan_y_top_intersectEdge_X;
				sampleY0 = scan_y_top;

			}
			if (scan_y_bottom > e->Maxy)//边的最高点低于扫描线bottom高度
			{
				sampleX1 = scan_y_top_intersectEdge_X + (e->Maxy - scan_y_top) * EdgeSlope_dxFordy;
				sampleY1 = e->Maxy;
			}
			else
			{
				sampleX1 = scan_y_bottom_intersectEdge_X;
				sampleY1 = scan_y_bottom;
			}
			//只要在方块盒子内
			if (sampleX0 >= 0 && sampleX1 >= 0 && sampleX0 <= width && sampleX1 <= width)
			{

				if ((int)sampleX0 == (int)sampleX1)//如果此时接近垂直
				{
					scanlineOnEdge[(int)sampleX0] += e->direction * trapzoid_Area(sampleY1 - sampleY0, sampleX0, (int)sampleX0 + 1.0f, sampleX1, (int)sampleX0 + 1.0f);//根据有符号面积填充像素
					scanlineFillOffEdge[(int)sampleX0 + 1] += e->direction * (sampleY1 - sampleY0);
				}
				else
				{
					float temp = 0.0f, area = 0.0f, x1CrossingIntersect_Edge_Y = 0.0f;
					if (sampleX0 > sampleX1)
					{
						//需要对称的反转
						sampleY1 = scan_y_bottom - (sampleY1 - scan_y_top);
						sampleY0 = scan_y_bottom - (sampleY0 - scan_y_top);
						temp = sampleY1; sampleY1 = sampleY0; sampleY0 = temp;//交换掉sampleY1 sampleY0

						EdgeSlope_dxFordy = -EdgeSlope_dxFordy;
						EdgeSlope_dyFordx = -EdgeSlope_dyFordx;
						//采样的两个横坐标交换
						temp = sampleX1; sampleX1 = sampleX0; sampleX0 = temp;
						//交换bottom 扫描线 top扫描线 交点坐标
						temp = scan_y_bottom_intersectEdge_X;
						scan_y_bottom_intersectEdge_X = scan_y_top_intersectEdge_X;
						scan_y_top_intersectEdge_X = temp;
					}

					x1CrossingIntersect_Edge_Y = ((int)sampleX0 + 1 - scan_y_top_intersectEdge_X) * EdgeSlope_dyFordx + scan_y_top;//记录从开始点开始过单位像素距离与边的交点
					if (x1CrossingIntersect_Edge_Y > scan_y_bottom)
						x1CrossingIntersect_Edge_Y = scan_y_bottom;
					float int_sampleX1_Intersect_OnEdge_Y = ((int)sampleX1 - scan_y_top_intersectEdge_X) * EdgeSlope_dyFordx + scan_y_top;
					if (int_sampleX1_Intersect_OnEdge_Y > scan_y_bottom)
					{
						int De = sampleX0 + 1 - (int)sampleX1;
						if (De != 0)
							EdgeSlope_dyFordx = (scan_y_bottom - int_sampleX1_Intersect_OnEdge_Y) / De;
					}
					area = e->direction * (x1CrossingIntersect_Edge_Y - sampleY0);
					step = e->direction * EdgeSlope_dyFordx;//为带方向的三角形面积用
					scanlineOnEdge[(int)sampleX0] += triangle_Area(area, (int)sampleX0 + 1 - sampleX0);//根据有符号面积占单位像素面积填充像素
					for (int x = (int)sampleX0 + 1; x < (int)sampleX1; x++)
					{
						scanlineOnEdge[x] += area + 1.0f * 1.0f * step / 2;
						area += step * 1.0f;
					}
					scanlineOnEdge[(int)sampleX1] += area + e->direction * trapzoid_Area(sampleY1 - int_sampleX1_Intersect_OnEdge_Y, sampleX1, sampleX1 + 1, scan_y_bottom_intersectEdge_X, (int)sampleX1 + 1);//根据有符号面积占单位像素面积填充像素
					//填充剩下不在字形线上的像素但是在字形轮廓内的像素	
					scanlineFillOffEdge[(int)sampleX1 + 1] += e->direction * (sampleY1 - sampleY0);


				}

			}
		}


		e = e->next;
	}
}
static GlyphShape_active_edge* New_ActiveGlyphShape_Edge(GlyphShape_edge* e, int offx, float start_y)//生成新的活化边
{
	//std::cout << e->y1 - e->y0 << std::endl;
	GlyphShape_active_edge* z = (GlyphShape_active_edge*)malloc(sizeof(*z));
	if (!z)return z;//如果z为空指针 返回空指针 !z 在if条件下成立
	if (e->y1 - e->y0 == 0) return nullptr;

	float k = (e->x1 - e->x0) / (e->y1 - e->y0);
	z->slope_dxFordy = k;
	if (k != 0)
		z->slope_dyFordx = 1.0f / k;
	z->fx = (start_y - e->y0) * k + e->x0;
	z->fx -= offx;//转到原点空间
	z->direction = e->Dir;
	z->Maxy = e->y1;
	z->Miny = e->y0;
	z->next = nullptr;
	return z;
}
static void Rasterize_Edge(PackRectTexture_Context* rectTexContext, GlyphShape_edge* e, unsigned int n, int offx, int offy)//这个渲染没问题是正着进行字符颜色填充的
{
	float* scanline_HasValue_OnEdge, * scanlineFill_HasValue_OffEdge, Y = 0, scanYtop = 0, scanYbottom = 0;

	GlyphShape_active_edge* activeList = nullptr, ** iterator = nullptr;//活化边列表 存储列表数组的遍历地址迭代器
	GlyphShape_edge* ebegin = e;

	scanline_HasValue_OnEdge = (float*)malloc((rectTexContext->CharRectWidth * 2 + 1) * sizeof(float));
	scanlineFill_HasValue_OffEdge = scanline_HasValue_OnEdge + rectTexContext->CharRectWidth;
	e[n].y0 = rectTexContext->CharRectHight + 1;//这一行作用防止while(e->y0<=bottom){e++}一直指针自增导致越界使条件不满足停止指针自增
	Y = offy - 1;//需要减1操作不然剩下的没减的一行被填充为全白灰度255
	for (unsigned int j = 0; j < rectTexContext->CharRectHight; j++)
	{

		if (j == 0 || j == rectTexContext->CharRectHight - 1) { // 特殊处理最顶部和最底部的扫描线
			memset(scanline_HasValue_OnEdge, 0, rectTexContext->CharRectWidth * sizeof(float));
			memset(scanlineFill_HasValue_OffEdge, 0, rectTexContext->CharRectWidth * sizeof(float));
		}
		scanYbottom = Y + 1;
		scanYtop = Y;
		memset(scanline_HasValue_OnEdge, 0, (rectTexContext->CharRectWidth) * sizeof(*scanline_HasValue_OnEdge));//每次循环需要置零清空每一行的像素值
		memset(scanlineFill_HasValue_OffEdge, 0, (rectTexContext->CharRectWidth + 1) * sizeof(*scanline_HasValue_OnEdge));

		if (scanYbottom > rectTexContext->CharRectHight)
			scanYbottom = rectTexContext->CharRectHight;

		//将边的最高点低于扫描线高度之前的边内容进行删除  
		iterator = &activeList;//获取活化边地址
		while (*iterator)
		{
			GlyphShape_active_edge* z = *iterator;
			if (z->Maxy <= scanYtop)//将边的最高点低于扫描线高度之前的边内容进行删除
			{
				*iterator = z->next;//iterator地址存放下一个节点边
				free(z);

			}
			else//否则指针移动遍历
			{
				iterator = &((*iterator)->next);
			}

		}
		//对当前扫描高度进行生成新的活化边链表数组
		while (e->y0 <= scanYbottom)//将当前低于此高度的边加入到活化边列表中
		{
			GlyphShape_active_edge* z = New_ActiveGlyphShape_Edge(e, offx, scanYtop);
			if (z != nullptr)
			{
				//将当前活化边信息放入activeList链表数组中
				z->next = activeList;
				activeList = z;
			}

			//越界判断处理
			if ((e - ebegin) > (n - 1))return;
			e++;
		}

		//活化边链表数组生成完成 进行边的颜色边缘alpha值赋值
		if (activeList)
			fill_GlyphCloseShape_activeEdge(scanline_HasValue_OnEdge, scanlineFill_HasValue_OffEdge, rectTexContext->CharRectWidth, activeList, scanYtop);//遍历当前扫描高度下的activeList所有活化边进行着色

		//将当前扫描高度的活化边的颜色值 根据此进行当前字符显存位置的alpha进行赋值填充
		float sum = 0.0f, k = 0.0f, m;
		for (unsigned int i = 0; i < rectTexContext->CharRectWidth; i++)
		{

			sum += scanlineFill_HasValue_OffEdge[i];
			k = scanline_HasValue_OnEdge[i] + sum;
			//在一直有像素与线段相交时scanlinefill[i]为0不起作用 scanline[i]对应相交像素的颜色值  
			 //当像素不与线段相交时scanlinefill[i]起作用 scanline[i]为0不起作用   实质上时scanlinefill[i]对最右侧不与线段相交时像素做贡献  scanline[i]对最与线段相交时像素做贡献
			 //相交时scanline[i]有值 scanlinefill[i]=0  不相交右侧时scanlinefill[i]有值 scanline[i]=0
			 //也就是相交时 k=scanline[i]     不相交k=sum=scanlinefill[之前截止到右侧无相交的索引]

			m = fabs(k) * 255;
			if (m > 255)m = 255;
			rectTexContext->Rectpixel[i + j * rectTexContext->stride_in_bytes] = (unsigned char)m;
		}
		//同时更新每个边与scanYtop的交点
		iterator = &activeList;//获取活化边地址
		while (*iterator)
		{
			GlyphShape_active_edge* z = *iterator;
			float new_fx = z->fx + z->slope_dxFordy * 1;
			if (new_fx < 0.0f) new_fx = 0.0f; // 确保交点不超出边界
			z->fx = new_fx;
			iterator = &((*iterator)->next);
		}


		Y++;

	}
	//之前malloc的释放掉
	free(scanline_HasValue_OnEdge);

}
static void Fill_CloseGlyphShape(GlyphShape_Point* pts, PackRectTexture_Context* rectTexContext, unsigned int* windingEdgeCount, unsigned int windinglength, float scale, int offx, int offy)//windingEdgeCount存储每一个环绕线的点边数量的数组  windinglength代表几个环绕线
{

	GlyphShape_edge* e;
	unsigned int totalEdgeCount = 0, vertpointOffset = 0, h = 0;
	if (windinglength == 0)return;
	for (unsigned int i = 0; i < windinglength; i++)
		totalEdgeCount += windingEdgeCount[i];//累加边数量
	e = (GlyphShape_edge*)malloc(sizeof(*e) * (totalEdgeCount + 1));
	if (e == 0)return;
	totalEdgeCount = 0;
	for (unsigned int i = 0; i < windinglength; i++)//有多少个环绕闭合线
	{
		GlyphShape_Point* p = pts + vertpointOffset;
		vertpointOffset += windingEdgeCount[i];//对于一个环绕闭合线 顶点个数等于边数
		h = windingEdgeCount[i] - 1;
		//经过以下循环遍历会漏掉第一个点和最后一个点的边的信息 所以之前要使h=最后一个点索引
		for (unsigned int j = 0; j < windingEdgeCount[i]; h = j++)//windingEdgeCount[i] 第i个环绕线对应多少个边 先执行的是第一个点和最后一个点的边信息处理 往后是按顺序处理第一个点和下一个点边信息处理
		{
			//h=j++ 代表先执行h=j    j=j+1 相当于h<j 滞后
			unsigned int a = 0, b = 0;
			a = j;//j单位边第二个点  第一次执行j为0
			b = h;//h单位边第一个点  第一次执行h为n末尾
			if (p[a].y == p[b].y)//如果是水平就跳过继续遍历下一个  
				continue;
			//如果第一个点的高度高于第二个点方向为正方向 第二个点的高度低于第一个点方向为负方向 
			e[totalEdgeCount].Dir = 1;//先假设为正方向 如果条件判断是第二个点的高度低于第一个点会修改掉它  a永远是高度低的点 b永远是高度高的点
			if (p[h].y > p[j].y)
			{
				a = h;
				b = j;
				e[totalEdgeCount].Dir = -1;
			}
			//totalvertCount多少个点对应多少个边
			e[totalEdgeCount].x0 = p[a].x * scale;
			e[totalEdgeCount].y0 = p[a].y * -scale;
			e[totalEdgeCount].x1 = p[b].x * scale;
			e[totalEdgeCount].y1 = p[b].y * -scale;
			totalEdgeCount++;
		}
	}//这时候边有最高最低点和环绕线方向
	//开始将边进行从低到高排序
	sort_tessaEdge(e, totalEdgeCount);
	//最后根据排好的边进行渲染
	Rasterize_Edge(rectTexContext, e, totalEdgeCount, offx, offy);
	free(windingEdgeCount);//之前malloc的要释放掉 释放资源
	free(e);
}

static void RenderSingleCharacterIntoRect(ttfBuildFontInfo* fontInfo, int glyphIndex, unsigned char* CharRectpixelAddress, unsigned int CharRectWidth, unsigned int CharRectHeight, unsigned int stride, float scale)//将单个字符渲染到方块中 字体顶点信息  字符字形索引 这个字符的显存地址 宽高限制循环次数渲染 整体图像宽度 字体顶点缩放比例
{

	Glyph_Vertex* vertice = nullptr;
	unsigned int num_verts = GetGlyphCloseShape(fontInfo, glyphIndex, &vertice);//将原来外包边顶点转变为可以贝塞尔插值的点 其实求出切点
	unsigned int* winding_EdgeCounts = nullptr;//记录每一个环绕线的边的数量的数组 其中数组的数量就是windinglength
	unsigned int windinglength = 0;//记录多少个环绕线
	//判断vertice每个点的类型是曲线还是直接放入的点 如果是曲线就贝塞尔曲线插一些点 如果不是就直接放入
	GlyphShape_Point* points = FlattenIntoCurve(vertice, &winding_EdgeCounts, 0.695, &windinglength, num_verts);
	//将这些点形成的闭合边进行内部填充
	int offx = 0, offy = 0;
	PackRectTexture_Context PackedRectTexContext{ 0 };
	PackedRectTexContext.Rectpixel = CharRectpixelAddress;
	PackedRectTexContext.CharRectHight = CharRectHeight;
	PackedRectTexContext.CharRectWidth = CharRectWidth;
	PackedRectTexContext.stride_in_bytes = stride;
	TruetypeGetGlyphBitmapBox(fontInfo, glyphIndex, scale, &offx, &offy, 0, 0);
	Fill_CloseGlyphShape(points, &PackedRectTexContext, winding_EdgeCounts, windinglength, scale, offx, offy);
	//之前malloc的释放掉
	free(points);
}
static void RenderFontsKnownIntoRect(PackFontTexture_Context* fontTexContext, ttfBuildFontInfo* fontInfo, PackedCharRange* CharRange, PackedRect* rect)//需要图像集信息 字体顶点信息  一系列字符方块位置即在图集图像的位置
{
	int x0 = 0, y0 = 0, x1 = 0, y1 = 0, advanceX = 0;
	float scale = Truetype_Getscale_pixel(fontInfo, CharRange->FontSize);
	for (unsigned int i = 0; i < CharRange->num_chars; i++)//遍历 个数为rect的数组个数 字符个数
	{
		if (rect[i].was_PackedValid && rect[i].w != 0 && rect[i].h != 0)
		{
			PackedChar* CharCoordTmp = &CharRange->PackedCharCoordData_for_Range[i];//将要被写入 相应采样和字符包围盒坐标
			unsigned int codePoint = CharRange->Array_of_UnicodePoint[i];//找出codepoint Ascll索引间接找出index
			int glyfIndex = ttfFontFindGlyphIndex(fontInfo, codePoint);//字符到字形索引
			TruetypeGetGlyphBitmapBox(fontInfo, glyfIndex, scale, &x0, &y0, &x1, &y1);//获取字符方块包围盒的最小最大的坐标
			TruetypeGetMetric_spacing(fontInfo, glyfIndex, &advanceX);//获取字符步进距离到下一个字符的距离
			//开始将单个字符的贝塞尔顶点信息进行渲染到一个字符方块中
		/*	rect[i].x+= fontTexContext->padding;
			rect[i].y+= fontTexContext->padding;*/
			rect[i].w -= fontTexContext->padding;
			rect[i].h -= fontTexContext->padding;//这是为了防止其他字符的颜色混进来

			RenderSingleCharacterIntoRect(
				fontInfo,
				glyfIndex,
				fontTexContext->pixel + (int)rect[i].x + (int)rect[i].y * fontTexContext->stride_in_bytes,
				rect[i].w,
				rect[i].h,
				fontTexContext->stride_in_bytes,
				scale
			);
			CharCoordTmp->x0InTexMap = rect[i].x;
			CharCoordTmp->y0InTexMap = rect[i].y;
			CharCoordTmp->x1InTexMap = rect[i].x + rect[i].w;
			CharCoordTmp->y1InTexMap = rect[i].y + rect[i].h;
			CharCoordTmp->CharBox_xoff1 = x0;
			CharCoordTmp->CharBox_yoff1 = y0;
			CharCoordTmp->CharBox_xoff2 = x1;
			CharCoordTmp->CharBox_yoff2 = y1;
			CharCoordTmp->advanceX = scale * advanceX;
			CharCoordTmp->CodePoint = codePoint;
		}
	}
}
static bool ttfFontBuild(FontAtlas* fontTexatlas)
{
	//src_Font_tmp_Array[0]<===微软雅黑
	//src_Font_tmp_Array[1]<===宋体
	// src_Font_tmp_Array[2]<===黑体字体
	//src_Font_tmp_Array[3]<===calivo
	//......还有别的字体文件
	DynamicArray<BuildFontSrcData>src_Font_tmp_Array;//可能加载的是不同字体文件 微软雅黑 宋体 黑体字体文件 calivo Italy字体文件 
	src_Font_tmp_Array.resize(fontTexatlas->FontConfigData.size());//设定要加载多少个字体文件信息
	for (unsigned int i = 0; i < fontTexatlas->FontConfigData.size(); i++)
	{
		BuildFontSrcData& src_Font_tmpInfo = src_Font_tmp_Array[i];
		fontloadInfo& fontCfg = fontTexatlas->FontConfigData[i];//获取某一个特定风格字体配置信息
		
		int fontOffset = ttfFontGetfontOffset_Index((unsigned char*)fontCfg.FontData, fontCfg.fontId);//字体信息开始的位置
		if (!initTTFfontInfo(&src_Font_tmpInfo.FontInfo, (unsigned char*)fontCfg.FontData, fontOffset))//加载ttfFontInfo
		{
			printf("First step: initializing ttf data failed\n");
			return false;
		}
		//输出第一步加载ttf字体成功信息
		printf("First step: initializing ttf data successfully\n");
		//写入字符ascll索引的范围
		src_Font_tmpInfo.SrcCodePointRange = fontCfg.SrcRangeForCharCodePoint;
		//写入字符最后一个索引
		for (Char16* SrcCodePointRange_Reading_Pointer = src_Font_tmpInfo.SrcCodePointRange; SrcCodePointRange_Reading_Pointer[0] && SrcCodePointRange_Reading_Pointer[1]; SrcCodePointRange_Reading_Pointer += 2)
		{
			src_Font_tmpInfo.GlyphLastCodePoint = std::max(src_Font_tmpInfo.GlyphLastCodePoint, (unsigned int)SrcCodePointRange_Reading_Pointer[1]);
		}
		
	}
	unsigned int GlyphCounts = 0;
	unsigned int totalSurface = 0;//总的面积用来写入最后的纹理图集的宽度
	DynamicArray<PackedRect>AllocateRectArray;//这个需要分配的地Rect字符方块地址
	DynamicArray<PackedChar>AllocatePackedCharArray;//这个需要分配的包装的字符方块坐标 采样坐标 数组地址
	for (unsigned int i = 0; i < src_Font_tmp_Array.size(); i++)//遍历有多少个风格字体
	{
		BuildFontSrcData& src_Font_tmpInfo = src_Font_tmp_Array[i];//src_Font_tmpInfo.SrcCodePointRange 存储 大写字符型前后范围 小写字符型前后范围 数字型前后范围 符号型前后范围 Latin 字母前后范围
		fontloadInfo& fontCfg = fontTexatlas->FontConfigData[i];
		src_Font_tmpInfo.GlyphListCodePointBitArray.Create(src_Font_tmpInfo.GlyphLastCodePoint + 1);//src_Font_tmpInfo.GlyphListCodePointBitArray里面存储字符ASCLL索引
		for (Char16* OneStyle_Character_CodePoint_Range_Pointer = src_Font_tmpInfo.SrcCodePointRange; OneStyle_Character_CodePoint_Range_Pointer[0] && OneStyle_Character_CodePoint_Range_Pointer[1]; OneStyle_Character_CodePoint_Range_Pointer += 2)
		{
			for (unsigned int AscllCodePoint = OneStyle_Character_CodePoint_Range_Pointer[0]; AscllCodePoint <= OneStyle_Character_CodePoint_Range_Pointer[1]; AscllCodePoint++)
			{//写进去glyphList ASCLL索引 多少个字符
				if (!ttfFontFindGlyphIndex(&src_Font_tmpInfo.FontInfo, AscllCodePoint))
					continue;
				src_Font_tmpInfo.GlyphListCodePointBitArray.SetBit(AscllCodePoint);
				GlyphCounts++;
				src_Font_tmpInfo.GlyphCount++;
			}
		}
		//输出加载字符个数成功的消息
		printf("Second step: record glyph Count----Numchars successfully\n");
		src_Font_tmpInfo.glyphList_of_CodePoint.reserve(GlyphCounts);//src_Font_tmpInfo.glyphList_of_CodePoint需要写入
		UnpackbitVectorToGlyphList(&src_Font_tmpInfo.GlyphListCodePointBitArray, &src_Font_tmpInfo.glyphList_of_CodePoint);//src_Font_tmpInfo.glyphList_of_CodePoint存储着字符ASCLL索引
		src_Font_tmpInfo.GlyphListCodePointBitArray.Clear();
		//输出glyph数据搬运成功消息
		printf("Third step: move glyphbit-->src_Font_tmpInfo.glyphList_of_CodePoint successfully\n");
		_ASSERT(src_Font_tmpInfo.glyphList_of_CodePoint.size() == GlyphCounts);//确保长度大小相同
		AllocateRectArray.resize(GlyphCounts);//设置Rect packedChars数组大小为字符数
		AllocatePackedCharArray.resize(GlyphCounts);
		//存储各个字符的宽高 和待写入的坐标
		src_Font_tmpInfo.Rect = AllocateRectArray.Data;
		//构建 PackedCharRange 结构体信息
		src_Font_tmpInfo.packedChar_for_range.Array_of_UnicodePoint = src_Font_tmpInfo.glyphList_of_CodePoint.Data;//存储ASCLL索引码
		src_Font_tmpInfo.packedChar_for_range.PackedCharCoordData_for_Range = AllocatePackedCharArray.Data;//存储字符色块的采样坐标 Box包围盒的坐标
		src_Font_tmpInfo.packedChar_for_range.num_chars = src_Font_tmpInfo.glyphList_of_CodePoint.size();//多少个字符
		src_Font_tmpInfo.packedChar_for_range.FontSize = fontCfg.Fontsize;//字体大小


		//需要进行获取想要的像素和字体文件固定像素大小的缩放因子
		float pixel_scale = Truetype_Getscale_pixel(&src_Font_tmpInfo.FontInfo, fontCfg.Fontsize);
		// //下面进行Rect的宽高写入 
		for (unsigned int i = 0; i < GlyphCounts; i++)
		{
			int x0, y0, x1, y1;
			int glyphIndex = ttfFontFindGlyphIndex(&src_Font_tmpInfo.FontInfo, src_Font_tmpInfo.glyphList_of_CodePoint[i]);//字符索引偏移
			TruetypeGetGlyphBitmapBox(&src_Font_tmpInfo.FontInfo, glyphIndex, pixel_scale, &x0, &y0, &x1, &y1);
			src_Font_tmpInfo.Rect[i].w = (float)(x1 - x0 + fontTexatlas->TexGlyphPadding * 1.76);
			src_Font_tmpInfo.Rect[i].h = (float)(y1 - y0 + fontTexatlas->TexGlyphPadding * 1.76);
			src_Font_tmpInfo.Rect[i].was_PackedValid = true;
			totalSurface += src_Font_tmpInfo.Rect[i].w * src_Font_tmpInfo.Rect[i].h;
		}

		const int surface_Area_sqrt = sqrt(totalSurface);//先进行对整体宽赋值 方便对节点个数的处理
		fontTexatlas->TexWidth = (surface_Area_sqrt >= 4096 * 0.7f) ? 4096 : (surface_Area_sqrt >= 2048 * 0.7f) ? 2048 : (surface_Area_sqrt >= 1024 * 0.7f) ? 1024 : 512;

		PackedContext packContext;
		//下面进行字符方块的位置分配及字符纹理图集的整体高进行赋值
		unsigned int Tex_Height_Max = 1024 * 32;
		PackedRectBegin(&packContext, fontTexatlas->TexWidth, Tex_Height_Max);
		//Rect[i].(x,y) atlas->TexWidth 和 atlas->TexHeight
		PackRect(&packContext, src_Font_tmpInfo.Rect, GlyphCounts);//已经对现在知道的字符位置进行了赋值

		//现在要为atlas的白色色块分配
		PackedRect whiteRect{ 0,0,fontTexatlas->WhiteWidth,fontTexatlas->WhiteHeight,true };
		PackRect(&packContext, &whiteRect, 1);
		//输出字符方块坐标分配和src_Font_tmpInfo成员成功的消息
		printf("Fourth step: allocate rect coord and write src_Font_tmpInfo member info successfully\n");
		//现在根据已经知道的位置计算图集的总高度
		for (unsigned int i = 0; i < GlyphCounts; i++)//继续遍历rect数组
			if (src_Font_tmpInfo.Rect[i].was_PackedValid)
				fontTexatlas->TexHeight = std::max(fontTexatlas->TexHeight, (unsigned int)(src_Font_tmpInfo.Rect[i].y + src_Font_tmpInfo.Rect[i].h));
		//开始渲染所有字符的字体顶点进行RGB颜色填充到字符图集Texture中
		fontTexatlas->TexAlpha8 = (unsigned char*)malloc(fontTexatlas->TexHeight * fontTexatlas->TexWidth * sizeof(unsigned char));
		memset(fontTexatlas->TexAlpha8, 0, fontTexatlas->TexHeight * fontTexatlas->TexWidth * sizeof(unsigned char));
		PackFontTexture_Context PackFontTexture = {
			fontTexatlas->TexAlpha8, //灰度像素地址
			fontTexatlas->TexHeight, //高度
			fontTexatlas->TexWidth,  //宽度
			fontTexatlas->TexGlyphPadding };//字符彼此之间的间距
		RenderFontsKnownIntoRect(&PackFontTexture, &src_Font_tmpInfo.FontInfo, &src_Font_tmpInfo.packedChar_for_range, src_Font_tmpInfo.Rect);//这部分开始渲染所有字符到各自的字符方块中
		//输出填充字形轮廓到字符方块的消息
		printf("Fifth step: render fontContour into rect and fill pixel successfully\n");
		fontTexatlas->BuildWhiteRect(
			PackFontTexture.pixel + (int)whiteRect.x + (int)whiteRect.y * PackFontTexture.stride_in_bytes,
			whiteRect.w,
			whiteRect.h,
			PackFontTexture.stride_in_bytes
		);//开始为白色色块赋上颜色
		fontTexatlas->WhiteU1 = (whiteRect.x+2) / fontTexatlas->TexWidth;
		fontTexatlas->WhiteV1 = (whiteRect.y+2) / fontTexatlas->TexHeight;//为白色色块的纹理坐标进行赋值
		fontTexatlas->WhiteU2 = (whiteRect.x + whiteRect.w) / fontTexatlas->TexWidth;
		fontTexatlas->WhiteV2 = (whiteRect.y + whiteRect.h) / fontTexatlas->TexHeight;

		//开始进行纹理0-1坐标和unicodePoint的刻入
		fontTexatlas->glyphfonts = new font_of_characters;
		for (unsigned int i = 0; i < GlyphCounts; i++)
		{
			Align_quad tmpQuad = {};
			PackedChar packedChar = src_Font_tmpInfo.packedChar_for_range.PackedCharCoordData_for_Range[i];
			GetPackedCharQuadCoord(&packedChar, fontTexatlas->TexWidth, fontTexatlas->TexHeight, &tmpQuad);
			fontTexatlas->glyphfonts->AddGlyph(tmpQuad.x0, tmpQuad.y0, tmpQuad.x1, tmpQuad.y1, tmpQuad.s0, tmpQuad.t0, tmpQuad.s1, tmpQuad.t1, packedChar.advanceX, packedChar.CodePoint);
		}
		//输出纹理0-1坐标的刻入成功消息
		printf("Sixth step: Write texcoord successfully\n");
		//开始写入字符串ASCLL字符码和Glyph的索引的对应表
		fontTexatlas->glyphfonts->buildLookUpTable_Glyph_CharCodePoint();
		//输出字符串ASCLL字符码和Glyph的索引的对应表建立成功消息
		printf("Seventh step: build LookUpTable_Glyph_CharCodePoint successfully\n");
		//文本渲染最终要访问的是fontTexatlas->TexAlpha8   fontTexatlas->glyphfonts  到时候调用renderText 用一个指针变量接受这个fontTexatlas->glyphfonts 的地址

		//释放链表节点
		PackedRectEnd(&packContext);
	}
	// ---- 开始释放资源 ----
	for (unsigned int i = 0; i < src_Font_tmp_Array.size(); i++)
	{
		BuildFontSrcData& src_Font_tmpInfo = src_Font_tmp_Array[i];

		// 清理 Rect 和 PackedChar 的数据
		src_Font_tmpInfo.Rect = nullptr;
		src_Font_tmpInfo.packedChar_for_range.Array_of_UnicodePoint = nullptr;
		src_Font_tmpInfo.packedChar_for_range.PackedCharCoordData_for_Range = nullptr;

		// 清理其他动态分配的资源
		src_Font_tmpInfo.GlyphListCodePointBitArray.Clear();
		src_Font_tmpInfo.glyphList_of_CodePoint.clear();
	}
	return true;
}
static unsigned int DecompressedLength(const unsigned char* src);
static unsigned int stb_decompress(unsigned char* output, const unsigned char* i, unsigned int length);
static const char* GetCompressedFontDataTTFBase85();
static unsigned int Decode85Byte(char c) { return c >= '\\' ? c - 36 : c - 35; }
static void Decode85(const unsigned char* in, unsigned char* out)
{
	while (*in)
	{
		unsigned int tmp = Decode85Byte(in[0]) + 85 * (Decode85Byte(in[1]) + 85 * (Decode85Byte(in[2]) + 85 * (Decode85Byte(in[3]) + 85 * Decode85Byte(in[4]))));
		//将它们分成4个字节的unsigned char类型
		out[0] = ((tmp >> 0) & 0xFF);//第一个字节
		out[1] = ((tmp >> 8) & 0xFF);//第二个字节
		out[2] = ((tmp >> 16) & 0xFF);//第三个字节
		out[3] = ((tmp >> 24) & 0xFF);//第四个字节
		in += 5;
		out += 4;
	}
}
void FontAtlas::AddFont_FromFile(const char* filename, float size_pixels, Char16* GlyphRange)//加载中文字体
{

	FILE* f;
	fopen_s(&f, filename, "rb");//打开文件
	long off = 0, sz = 0;
	unsigned int  Data_BufSize = ((off = ftell(f)) != -1 && !fseek(f, 0, SEEK_END) && (sz = ftell(f)) != -1 && !fseek(f, off, SEEK_SET)) ? (unsigned long long)sz : (unsigned long long) - 1;
	void* ttfData = malloc(Data_BufSize);
	fread(ttfData, 1, Data_BufSize, f);
	fontloadInfo font_LdInfo;
	font_LdInfo.Fontsize = size_pixels;
	font_LdInfo.FontData = ttfData;
	font_LdInfo.FontDataSize = Data_BufSize;
	font_LdInfo.SrcRangeForCharCodePoint = GlyphRange;
	font_LdInfo.fontId = 0;
	this->fontSize = size_pixels;
	AddFont(&font_LdInfo);
}
void FontAtlas::Addfont_internal()
{
	fontloadInfo font_LdInfo;
	if (font_LdInfo.Fontsize <= 0.0f)
		font_LdInfo.Fontsize = 13.0f;

	this->fontSize = font_LdInfo.Fontsize;
	Char16* glyph_CodePointRange = (font_LdInfo.SrcRangeForCharCodePoint != nullptr) ? font_LdInfo.SrcRangeForCharCodePoint : Get_LatinGlyph_CodePointRange();
	const char* ttf_compress_Based85Encode = GetCompressedFontDataTTFBase85();//获得85加密的ttf 字体文件

	//先85解码 后普通解码
	unsigned int compressed85_DataSize = 0, Data_BufSize = 0;//加密的85数据大小 解码后的大小
	unsigned  char* compressed85_Data = nullptr, * ttfData = nullptr;//加密的85数据地址 解码后的ttf数据地址

	//ttf_compress_Based85Encode-------解码------>compressed85_Data
	compressed85_DataSize = ((strlen(ttf_compress_Based85Encode) + 4) / 5) * 4;
	compressed85_Data = (unsigned  char*)malloc(compressed85_DataSize * sizeof(unsigned  char));//分配内存
	Decode85((const unsigned char*)ttf_compress_Based85Encode, compressed85_Data);

	//compressed85_Data------->解码-------->ttfData 
	Data_BufSize = DecompressedLength((const unsigned char*)compressed85_Data);
	ttfData = (unsigned char*)malloc(Data_BufSize * sizeof(unsigned  char));
	stb_decompress(ttfData, (const unsigned char*)compressed85_Data, (unsigned int)compressed85_DataSize);

	font_LdInfo.FontData = ttfData;
	font_LdInfo.FontDataSize = Data_BufSize;
	font_LdInfo.SrcRangeForCharCodePoint = glyph_CodePointRange;
	font_LdInfo.fontId = 0;
	AddFont(&font_LdInfo);
}

void FontAtlas::AddFont(fontloadInfo* fontcfgInfo)//添加新的风格字体
{
	this->FontConfigData.push_back(*fontcfgInfo);
}
void FontAtlas::BuildFont()
{
	this->FontBuild = ttfFontBuild;//获取建立字体信息函数功能地址
	this->FontBuild(this);
}
void FontAtlas::WriteAlpha8_Data(unsigned int* glTexDataAddr)
{

	unsigned char* src = this->TexAlpha8;
	for (unsigned int n = this->TexHeight * this->TexWidth; n > 0; n--)
	{
		*glTexDataAddr++ = Col(255, 255, 255, (unsigned char)(*src++));
	}
}
void FontAtlas::BuildWhiteRect(unsigned char* pixel, unsigned int width, unsigned int height, unsigned int stride_in_bytes)
{
	
	for (unsigned int j = 0; j < height; j++)
	{
		for (unsigned int i = 0; i < width; i++)
		{
			pixel[i + j * stride_in_bytes] = 255;
		}
	}
}
outglTexInfo FontAtlas::GetTexAlpha32()
{
	outglTexInfo outInfo{};
	unsigned int* data = (unsigned int*)malloc(this->TexHeight * this->TexWidth * sizeof(unsigned int));//存储大颜色图
	unsigned int* dst = data;
	memset(dst, 0, this->TexHeight * this->TexWidth * sizeof(unsigned int));//图像像素alpha置0
	WriteAlpha8_Data(dst);
	outInfo.Texpixel = dst;
	outInfo.height = this->TexHeight;
	outInfo.width = this->TexWidth;
	return outInfo;
}
Char16* FontAtlas::Get_LatinGlyph_CodePointRange()
{
	static Char16 range[] =
	{
		 0x0020, 0x00FF, // 基本拉丁英文Unicode Ascll范围 32-255
	};
	return &range[0];
}
Char16* FontAtlas::Get_ChineseGlyph_CodePointRange()
{
	static  Char16 ranges[] =
	{
		0x0020, 0x00FF, // 拉丁英文
		0x2000, 0x206F, // 拼音
		0x3000, 0x30FF, // 中日韩符号 拼音, 平假名, 片假名
		0x31F0, 0x31FF, // 日文片假名
		0xFF00, 0xFFEF, // 半宽字符
		0xFFFD, 0xFFFD, // 无效
		0x4e00, 0x9FAF, //中日韩越统一字符
		0,
	};
	return &ranges[0];
}
void font_of_characters::AddGlyph(float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1, float AdvanceX, unsigned int CharCodePoint)
{
	this->Glyphs.resize(this->Glyphs.size() + 1);//如果之前没有内容留一个空间放进去进行push内存写入操作
	FontGlyph& glyph = this->Glyphs.back();
	glyph.AdvanceX = AdvanceX;

	glyph.CodePoint = CharCodePoint;
	glyph.X0 = x0;
	glyph.Y0 = y0;
	glyph.X1 = x1;
	glyph.Y1 = y1;
	glyph.U0 = u0;
	glyph.V0 = v0;
	glyph.U1 = u1;
	glyph.V1 = v1;
}
void font_of_characters::buildLookUpTable_Glyph_CharCodePoint()
{
	unsigned int HighCodePoint = 0;
	for (unsigned int i = 0; i < this->Glyphs.size(); i++)
		HighCodePoint = std::max(HighCodePoint, this->Glyphs[i].CodePoint);
	this->IndexSearchTable.resize(HighCodePoint + 1);
	memset(this->IndexSearchTable.Data, -1, this->IndexSearchTable.size() * sizeof(unsigned int));
	for (unsigned int i = 0; i < this->Glyphs.size(); i++)
	{
		unsigned int codePoint = this->Glyphs[i].CodePoint;
		IndexSearchTable[codePoint] = i;
	}
}
bool font_of_characters::FindGlyph(unsigned int CodePoint, unsigned int& glyphID)
{
	if (CodePoint>this->IndexSearchTable.size())
		return false;
	if (this->IndexSearchTable[CodePoint] != -1)
	{
		glyphID = this->IndexSearchTable[CodePoint];
		if (glyphID>this->Glyphs.size())
			return false;
		return true;
	}

	return false;
}






static unsigned int DecompressedLength(const unsigned char* src)
{

	return (src[8] << 24) + (src[9] << 16) + (src[10] << 8) + src[11];
}
static unsigned char* stb__barrier_out_e, * stb__barrier_out_b;
static const unsigned char* stb__barrier_in_b;
static unsigned char* stb__dout;
static void stb__match(const unsigned char* data, unsigned int length)
{
	// INVERSE of memmove... write each byte before copying the next...
	if (stb__dout + length > stb__barrier_out_e) { stb__dout += length; return; }
	if (data < stb__barrier_out_b) { stb__dout = stb__barrier_out_e + 1; return; }
	while (length--) *stb__dout++ = *data++;
}

static void stb__lit(const unsigned char* data, unsigned int length)
{
	if (stb__dout + length > stb__barrier_out_e) { stb__dout += length; return; }
	if (data < stb__barrier_in_b) { stb__dout = stb__barrier_out_e + 1; return; }
	memcpy(stb__dout, data, length);
	stb__dout += length;
}

#define stb__in2(x)   ((i[x] << 8) + i[(x)+1])
#define stb__in3(x)   ((i[x] << 16) + stb__in2((x)+1))
#define stb__in4(x)   ((i[x] << 24) + stb__in3((x)+1))

static const unsigned char* stb_decompress_token(const unsigned char* i)
{
	if (*i >= 0x20) { // use fewer if's for cases that expand small
		if (*i >= 0x80)       stb__match(stb__dout - i[1] - 1, i[0] - 0x80 + 1), i += 2;
		else if (*i >= 0x40)  stb__match(stb__dout - (stb__in2(0) - 0x4000 + 1), i[2] + 1), i += 3;
		else /* *i >= 0x20 */ stb__lit(i + 1, i[0] - 0x20 + 1), i += 1 + (i[0] - 0x20 + 1);
	}
	else { // more ifs for cases that expand large, since overhead is amortized
		if (*i >= 0x18)       stb__match(stb__dout - (stb__in3(0) - 0x180000 + 1), i[3] + 1), i += 4;
		else if (*i >= 0x10)  stb__match(stb__dout - (stb__in3(0) - 0x100000 + 1), stb__in2(3) + 1), i += 5;
		else if (*i >= 0x08)  stb__lit(i + 2, stb__in2(0) - 0x0800 + 1), i += 2 + (stb__in2(0) - 0x0800 + 1);
		else if (*i == 0x07)  stb__lit(i + 3, stb__in2(1) + 1), i += 3 + (stb__in2(1) + 1);
		else if (*i == 0x06)  stb__match(stb__dout - (stb__in3(1) + 1), i[4] + 1), i += 5;
		else if (*i == 0x04)  stb__match(stb__dout - (stb__in3(1) + 1), stb__in2(4) + 1), i += 6;
	}
	return i;
}

static unsigned int stb_adler32(unsigned int adler32, unsigned char* buffer, unsigned int buflen)
{
	const unsigned long ADLER_MOD = 65521;
	unsigned long s1 = adler32 & 0xffff, s2 = adler32 >> 16;
	unsigned long blocklen = buflen % 5552;

	unsigned long i;
	while (buflen) {
		for (i = 0; i + 7 < blocklen; i += 8) {
			s1 += buffer[0], s2 += s1;
			s1 += buffer[1], s2 += s1;
			s1 += buffer[2], s2 += s1;
			s1 += buffer[3], s2 += s1;
			s1 += buffer[4], s2 += s1;
			s1 += buffer[5], s2 += s1;
			s1 += buffer[6], s2 += s1;
			s1 += buffer[7], s2 += s1;

			buffer += 8;
		}

		for (; i < blocklen; ++i)
			s1 += *buffer++, s2 += s1;

		s1 %= ADLER_MOD, s2 %= ADLER_MOD;
		buflen -= blocklen;
		blocklen = 5552;
	}
	return (unsigned int)(s2 << 16) + (unsigned int)s1;
}

static unsigned int stb_decompress(unsigned char* output, const unsigned char* i, unsigned int /*length*/)
{
	if (stb__in4(0) != 0x57bC0000) return 0;
	if (stb__in4(4) != 0)          return 0; // error! stream is > 4GB
	const unsigned int olen = DecompressedLength(i);
	stb__barrier_in_b = i;
	stb__barrier_out_e = output + olen;
	stb__barrier_out_b = output;
	i += 16;

	stb__dout = output;
	for (;;) {
		const unsigned char* old_i = i;
		i = stb_decompress_token(i);
		if (i == old_i) {
			if (*i == 0x05 && i[1] == 0xfa) {
				if (stb__dout != output + olen) return 0;
				if (stb_adler32(1, output, olen) != (unsigned int)stb__in4(2))
					return 0;
				return olen;
			}
			else {
				return 0;
			}
		}
		if (stb__dout > output + olen)
			return 0;
	}
}

static const char proggy_clean_ttf_compressed_data_base85[11980 + 1] =
"7])#######hV0qs'/###[),##/l:$#Q6>##5[n42>c-TH`->>#/e>11NNV=Bv(*:.F?uu#(gRU.o0XGH`$vhLG1hxt9?W`#,5LsCp#-i>.r$<$6pD>Lb';9Crc6tgXmKVeU2cD4Eo3R/"
"2*>]b(MC;$jPfY.;h^`IWM9<Lh2TlS+f-s$o6Q<BWH`YiU.xfLq$N;$0iR/GX:U(jcW2p/W*q?-qmnUCI;jHSAiFWM.R*kU@C=GH?a9wp8f$e.-4^Qg1)Q-GL(lf(r/7GrRgwV%MS=C#"
"`8ND>Qo#t'X#(v#Y9w0#1D$CIf;W'#pWUPXOuxXuU(H9M(1<q-UE31#^-V'8IRUo7Qf./L>=Ke$$'5F%)]0^#0X@U.a<r:QLtFsLcL6##lOj)#.Y5<-R&KgLwqJfLgN&;Q?gI^#DY2uL"
"i@^rMl9t=cWq6##weg>$FBjVQTSDgEKnIS7EM9>ZY9w0#L;>>#Mx&4Mvt//L[MkA#W@lK.N'[0#7RL_&#w+F%HtG9M#XL`N&.,GM4Pg;-<nLENhvx>-VsM.M0rJfLH2eTM`*oJMHRC`N"
"kfimM2J,W-jXS:)r0wK#@Fge$U>`w'N7G#$#fB#$E^$#:9:hk+eOe--6x)F7*E%?76%^GMHePW-Z5l'&GiF#$956:rS?dA#fiK:)Yr+`&#0j@'DbG&#^$PG.Ll+DNa<XCMKEV*N)LN/N"
"*b=%Q6pia-Xg8I$<MR&,VdJe$<(7G;Ckl'&hF;;$<_=X(b.RS%%)###MPBuuE1V:v&cX&#2m#(&cV]`k9OhLMbn%s$G2,B$BfD3X*sp5#l,$R#]x_X1xKX%b5U*[r5iMfUo9U`N99hG)"
"tm+/Us9pG)XPu`<0s-)WTt(gCRxIg(%6sfh=ktMKn3j)<6<b5Sk_/0(^]AaN#(p/L>&VZ>1i%h1S9u5o@YaaW$e+b<TWFn/Z:Oh(Cx2$lNEoN^e)#CFY@@I;BOQ*sRwZtZxRcU7uW6CX"
"ow0i(?$Q[cjOd[P4d)]>ROPOpxTO7Stwi1::iB1q)C_=dV26J;2,]7op$]uQr@_V7$q^%lQwtuHY]=DX,n3L#0PHDO4f9>dC@O>HBuKPpP*E,N+b3L#lpR/MrTEH.IAQk.a>D[.e;mc."
"x]Ip.PH^'/aqUO/$1WxLoW0[iLA<QT;5HKD+@qQ'NQ(3_PLhE48R.qAPSwQ0/WK?Z,[x?-J;jQTWA0X@KJ(_Y8N-:/M74:/-ZpKrUss?d#dZq]DAbkU*JqkL+nwX@@47`5>w=4h(9.`G"
"CRUxHPeR`5Mjol(dUWxZa(>STrPkrJiWx`5U7F#.g*jrohGg`cg:lSTvEY/EV_7H4Q9[Z%cnv;JQYZ5q.l7Zeas:HOIZOB?G<Nald$qs]@]L<J7bR*>gv:[7MI2k).'2($5FNP&EQ(,)"
"U]W]+fh18.vsai00);D3@4ku5P?DP8aJt+;qUM]=+b'8@;mViBKx0DE[-auGl8:PJ&Dj+M6OC]O^((##]`0i)drT;-7X`=-H3[igUnPG-NZlo.#k@h#=Ork$m>a>$-?Tm$UV(?#P6YY#"
"'/###xe7q.73rI3*pP/$1>s9)W,JrM7SN]'/4C#v$U`0#V.[0>xQsH$fEmPMgY2u7Kh(G%siIfLSoS+MK2eTM$=5,M8p`A.;_R%#u[K#$x4AG8.kK/HSB==-'Ie/QTtG?-.*^N-4B/ZM"
"_3YlQC7(p7q)&](`6_c)$/*JL(L-^(]$wIM`dPtOdGA,U3:w2M-0<q-]L_?^)1vw'.,MRsqVr.L;aN&#/EgJ)PBc[-f>+WomX2u7lqM2iEumMTcsF?-aT=Z-97UEnXglEn1K-bnEO`gu"
"Ft(c%=;Am_Qs@jLooI&NX;]0#j4#F14;gl8-GQpgwhrq8'=l_f-b49'UOqkLu7-##oDY2L(te+Mch&gLYtJ,MEtJfLh'x'M=$CS-ZZ%P]8bZ>#S?YY#%Q&q'3^Fw&?D)UDNrocM3A76/"
"/oL?#h7gl85[qW/NDOk%16ij;+:1a'iNIdb-ou8.P*w,v5#EI$TWS>Pot-R*H'-SEpA:g)f+O$%%`kA#G=8RMmG1&O`>to8bC]T&$,n.LoO>29sp3dt-52U%VM#q7'DHpg+#Z9%H[K<L"
"%a2E-grWVM3@2=-k22tL]4$##6We'8UJCKE[d_=%wI;'6X-GsLX4j^SgJ$##R*w,vP3wK#iiW&#*h^D&R?jp7+/u&#(AP##XU8c$fSYW-J95_-Dp[g9wcO&#M-h1OcJlc-*vpw0xUX&#"
"OQFKNX@QI'IoPp7nb,QU//MQ&ZDkKP)X<WSVL(68uVl&#c'[0#(s1X&xm$Y%B7*K:eDA323j998GXbA#pwMs-jgD$9QISB-A_(aN4xoFM^@C58D0+Q+q3n0#3U1InDjF682-SjMXJK)("
"h$hxua_K]ul92%'BOU&#BRRh-slg8KDlr:%L71Ka:.A;%YULjDPmL<LYs8i#XwJOYaKPKc1h:'9Ke,g)b),78=I39B;xiY$bgGw-&.Zi9InXDuYa%G*f2Bq7mn9^#p1vv%#(Wi-;/Z5h"
"o;#2:;%d&#x9v68C5g?ntX0X)pT`;%pB3q7mgGN)3%(P8nTd5L7GeA-GL@+%J3u2:(Yf>et`e;)f#Km8&+DC$I46>#Kr]]u-[=99tts1.qb#q72g1WJO81q+eN'03'eM>&1XxY-caEnO"
"j%2n8)),?ILR5^.Ibn<-X-Mq7[a82Lq:F&#ce+S9wsCK*x`569E8ew'He]h:sI[2LM$[guka3ZRd6:t%IG:;$%YiJ:Nq=?eAw;/:nnDq0(CYcMpG)qLN4$##&J<j$UpK<Q4a1]MupW^-"
"sj_$%[HK%'F####QRZJ::Y3EGl4'@%FkiAOg#p[##O`gukTfBHagL<LHw%q&OV0##F=6/:chIm0@eCP8X]:kFI%hl8hgO@RcBhS-@Qb$%+m=hPDLg*%K8ln(wcf3/'DW-$.lR?n[nCH-"
"eXOONTJlh:.RYF%3'p6sq:UIMA945&^HFS87@$EP2iG<-lCO$%c`uKGD3rC$x0BL8aFn--`ke%#HMP'vh1/R&O_J9'um,.<tx[@%wsJk&bUT2`0uMv7gg#qp/ij.L56'hl;.s5CUrxjO"
"M7-##.l+Au'A&O:-T72L]P`&=;ctp'XScX*rU.>-XTt,%OVU4)S1+R-#dg0/Nn?Ku1^0f$B*P:Rowwm-`0PKjYDDM'3]d39VZHEl4,.j']Pk-M.h^&:0FACm$maq-&sgw0t7/6(^xtk%"
"LuH88Fj-ekm>GA#_>568x6(OFRl-IZp`&b,_P'$M<Jnq79VsJW/mWS*PUiq76;]/NM_>hLbxfc$mj`,O;&%W2m`Zh:/)Uetw:aJ%]K9h:TcF]u_-Sj9,VK3M.*'&0D[Ca]J9gp8,kAW]"
"%(?A%R$f<->Zts'^kn=-^@c4%-pY6qI%J%1IGxfLU9CP8cbPlXv);C=b),<2mOvP8up,UVf3839acAWAW-W?#ao/^#%KYo8fRULNd2.>%m]UK:n%r$'sw]J;5pAoO_#2mO3n,'=H5(et"
"Hg*`+RLgv>=4U8guD$I%D:W>-r5V*%j*W:Kvej.Lp$<M-SGZ':+Q_k+uvOSLiEo(<aD/K<CCc`'Lx>'?;++O'>()jLR-^u68PHm8ZFWe+ej8h:9r6L*0//c&iH&R8pRbA#Kjm%upV1g:"
"a_#Ur7FuA#(tRh#.Y5K+@?3<-8m0$PEn;J:rh6?I6uG<-`wMU'ircp0LaE_OtlMb&1#6T.#FDKu#1Lw%u%+GM+X'e?YLfjM[VO0MbuFp7;>Q&#WIo)0@F%q7c#4XAXN-U&VB<HFF*qL("
"$/V,;(kXZejWO`<[5?\?ewY(*9=%wDc;,u<'9t3W-(H1th3+G]ucQ]kLs7df($/*JL]@*t7Bu_G3_7mp7<iaQjO@.kLg;x3B0lqp7Hf,^Ze7-##@/c58Mo(3;knp0%)A7?-W+eI'o8)b<"
"nKnw'Ho8C=Y>pqB>0ie&jhZ[?iLR@@_AvA-iQC(=ksRZRVp7`.=+NpBC%rh&3]R:8XDmE5^V8O(x<<aG/1N$#FX$0V5Y6x'aErI3I$7x%E`v<-BY,)%-?Psf*l?%C3.mM(=/M0:JxG'?"
"7WhH%o'a<-80g0NBxoO(GH<dM]n.+%q@jH?f.UsJ2Ggs&4<-e47&Kl+f//9@`b+?.TeN_&B8Ss?v;^Trk;f#YvJkl&w$]>-+k?'(<S:68tq*WoDfZu';mM?8X[ma8W%*`-=;D.(nc7/;"
")g:T1=^J$&BRV(-lTmNB6xqB[@0*o.erM*<SWF]u2=st-*(6v>^](H.aREZSi,#1:[IXaZFOm<-ui#qUq2$##Ri;u75OK#(RtaW-K-F`S+cF]uN`-KMQ%rP/Xri.LRcB##=YL3BgM/3M"
"D?@f&1'BW-)Ju<L25gl8uhVm1hL$##*8###'A3/LkKW+(^rWX?5W_8g)a(m&K8P>#bmmWCMkk&#TR`C,5d>g)F;t,4:@_l8G/5h4vUd%&%950:VXD'QdWoY-F$BtUwmfe$YqL'8(PWX("
"P?^@Po3$##`MSs?DWBZ/S>+4%>fX,VWv/w'KD`LP5IbH;rTV>n3cEK8U#bX]l-/V+^lj3;vlMb&[5YQ8#pekX9JP3XUC72L,,?+Ni&co7ApnO*5NK,((W-i:$,kp'UDAO(G0Sq7MVjJs"
"bIu)'Z,*[>br5fX^:FPAWr-m2KgL<LUN098kTF&#lvo58=/vjDo;.;)Ka*hLR#/k=rKbxuV`>Q_nN6'8uTG&#1T5g)uLv:873UpTLgH+#FgpH'_o1780Ph8KmxQJ8#H72L4@768@Tm&Q"
"h4CB/5OvmA&,Q&QbUoi$a_%3M01H)4x7I^&KQVgtFnV+;[Pc>[m4k//,]1?#`VY[Jr*3&&slRfLiVZJ:]?=K3Sw=[$=uRB?3xk48@aeg<Z'<$#4H)6,>e0jT6'N#(q%.O=?2S]u*(m<-"
"V8J'(1)G][68hW$5'q[GC&5j`TE?m'esFGNRM)j,ffZ?-qx8;->g4t*:CIP/[Qap7/9'#(1sao7w-.qNUdkJ)tCF&#B^;xGvn2r9FEPFFFcL@.iFNkTve$m%#QvQS8U@)2Z+3K:AKM5i"
"sZ88+dKQ)W6>J%CL<KE>`.d*(B`-n8D9oK<Up]c$X$(,)M8Zt7/[rdkqTgl-0cuGMv'?>-XV1q['-5k'cAZ69e;D_?$ZPP&s^+7])$*$#@QYi9,5P&#9r+$%CE=68>K8r0=dSC%%(@p7"
".m7jilQ02'0-VWAg<a/''3u.=4L$Y)6k/K:_[3=&jvL<L0C/2'v:^;-DIBW,B4E68:kZ;%?8(Q8BH=kO65BW?xSG&#@uU,DS*,?.+(o(#1vCS8#CHF>TlGW'b)Tq7VT9q^*^$$.:&N@@"
"$&)WHtPm*5_rO0&e%K&#-30j(E4#'Zb.o/(Tpm$>K'f@[PvFl,hfINTNU6u'0pao7%XUp9]5.>%h`8_=VYbxuel.NTSsJfLacFu3B'lQSu/m6-Oqem8T+oE--$0a/k]uj9EwsG>%veR*"
"hv^BFpQj:K'#SJ,sB-'#](j.Lg92rTw-*n%@/;39rrJF,l#qV%OrtBeC6/,;qB3ebNW[?,Hqj2L.1NP&GjUR=1D8QaS3Up&@*9wP?+lo7b?@%'k4`p0Z$22%K3+iCZj?XJN4Nm&+YF]u"
"@-W$U%VEQ/,,>>#)D<h#`)h0:<Q6909ua+&VU%n2:cG3FJ-%@Bj-DgLr`Hw&HAKjKjseK</xKT*)B,N9X3]krc12t'pgTV(Lv-tL[xg_%=M_q7a^x?7Ubd>#%8cY#YZ?=,`Wdxu/ae&#"
"w6)R89tI#6@s'(6Bf7a&?S=^ZI_kS&ai`&=tE72L_D,;^R)7[$s<Eh#c&)q.MXI%#v9ROa5FZO%sF7q7Nwb&#ptUJ:aqJe$Sl68%.D###EC><?-aF&#RNQv>o8lKN%5/$(vdfq7+ebA#"
"u1p]ovUKW&Y%q]'>$1@-[xfn$7ZTp7mM,G,Ko7a&Gu%G[RMxJs[0MM%wci.LFDK)(<c`Q8N)jEIF*+?P2a8g%)$q]o2aH8C&<SibC/q,(e:v;-b#6[$NtDZ84Je2KNvB#$P5?tQ3nt(0"
"d=j.LQf./Ll33+(;q3L-w=8dX$#WF&uIJ@-bfI>%:_i2B5CsR8&9Z&#=mPEnm0f`<&c)QL5uJ#%u%lJj+D-r;BoF&#4DoS97h5g)E#o:&S4weDF,9^Hoe`h*L+_a*NrLW-1pG_&2UdB8"
"6e%B/:=>)N4xeW.*wft-;$'58-ESqr<b?UI(_%@[P46>#U`'6AQ]m&6/`Z>#S?YY#Vc;r7U2&326d=w&H####?TZ`*4?&.MK?LP8Vxg>$[QXc%QJv92.(Db*B)gb*BM9dM*hJMAo*c&#"
"b0v=Pjer]$gG&JXDf->'StvU7505l9$AFvgYRI^&<^b68?j#q9QX4SM'RO#&sL1IM.rJfLUAj221]d##DW=m83u5;'bYx,*Sl0hL(W;;$doB&O/TQ:(Z^xBdLjL<Lni;''X.`$#8+1GD"
":k$YUWsbn8ogh6rxZ2Z9]%nd+>V#*8U_72Lh+2Q8Cj0i:6hp&$C/:p(HK>T8Y[gHQ4`4)'$Ab(Nof%V'8hL&#<NEdtg(n'=S1A(Q1/I&4([%dM`,Iu'1:_hL>SfD07&6D<fp8dHM7/g+"
"tlPN9J*rKaPct&?'uBCem^jn%9_K)<,C5K3s=5g&GmJb*[SYq7K;TRLGCsM-$$;S%:Y@r7AK0pprpL<Lrh,q7e/%KWK:50I^+m'vi`3?%Zp+<-d+$L-Sv:@.o19n$s0&39;kn;S%BSq*"
"$3WoJSCLweV[aZ'MQIjO<7;X-X;&+dMLvu#^UsGEC9WEc[X(wI7#2.(F0jV*eZf<-Qv3J-c+J5AlrB#$p(H68LvEA'q3n0#m,[`*8Ft)FcYgEud]CWfm68,(aLA$@EFTgLXoBq/UPlp7"
":d[/;r_ix=:TF`S5H-b<LI&HY(K=h#)]Lk$K14lVfm:x$H<3^Ql<M`$OhapBnkup'D#L$Pb_`N*g]2e;X/Dtg,bsj&K#2[-:iYr'_wgH)NUIR8a1n#S?Yej'h8^58UbZd+^FKD*T@;6A"
"7aQC[K8d-(v6GI$x:T<&'Gp5Uf>@M.*J:;$-rv29'M]8qMv-tLp,'886iaC=Hb*YJoKJ,(j%K=H`K.v9HggqBIiZu'QvBT.#=)0ukruV&.)3=(^1`o*Pj4<-<aN((^7('#Z0wK#5GX@7"
"u][`*S^43933A4rl][`*O4CgLEl]v$1Q3AeF37dbXk,.)vj#x'd`;qgbQR%FW,2(?LO=s%Sc68%NP'##Aotl8x=BE#j1UD([3$M(]UI2LX3RpKN@;/#f'f/&_mt&F)XdF<9t4)Qa.*kT"
"LwQ'(TTB9.xH'>#MJ+gLq9-##@HuZPN0]u:h7.T..G:;$/Usj(T7`Q8tT72LnYl<-qx8;-HV7Q-&Xdx%1a,hC=0u+HlsV>nuIQL-5<N?)NBS)QN*_I,?&)2'IM%L3I)X((e/dl2&8'<M"
":^#M*Q+[T.Xri.LYS3v%fF`68h;b-X[/En'CR.q7E)p'/kle2HM,u;^%OKC-N+Ll%F9CF<Nf'^#t2L,;27W:0O@6##U6W7:$rJfLWHj$#)woqBefIZ.PK<b*t7ed;p*_m;4ExK#h@&]>"
"_>@kXQtMacfD.m-VAb8;IReM3$wf0''hra*so568'Ip&vRs849'MRYSp%:t:h5qSgwpEr$B>Q,;s(C#$)`svQuF$##-D,##,g68@2[T;.XSdN9Qe)rpt._K-#5wF)sP'##p#C0c%-Gb%"
"hd+<-j'Ai*x&&HMkT]C'OSl##5RG[JXaHN;d'uA#x._U;.`PU@(Z3dt4r152@:v,'R.Sj'w#0<-;kPI)FfJ&#AYJ&#//)>-k=m=*XnK$>=)72L]0I%>.G690a:$##<,);?;72#?x9+d;"
"^V'9;jY@;)br#q^YQpx:X#Te$Z^'=-=bGhLf:D6&bNwZ9-ZD#n^9HhLMr5G;']d&6'wYmTFmL<LD)F^%[tC'8;+9E#C$g%#5Y>q9wI>P(9mI[>kC-ekLC/R&CH+s'B;K-M6$EB%is00:"
"+A4[7xks.LrNk0&E)wILYF@2L'0Nb$+pv<(2.768/FrY&h$^3i&@+G%JT'<-,v`3;_)I9M^AE]CN?Cl2AZg+%4iTpT3<n-&%H%b<FDj2M<hH=&Eh<2Len$b*aTX=-8QxN)k11IM1c^j%"
"9s<L<NFSo)B?+<-(GxsF,^-Eh@$4dXhN$+#rxK8'je'D7k`e;)2pYwPA'_p9&@^18ml1^[@g4t*[JOa*[=Qp7(qJ_oOL^('7fB&Hq-:sf,sNj8xq^>$U4O]GKx'm9)b@p7YsvK3w^YR-"
"CdQ*:Ir<($u&)#(&?L9Rg3H)4fiEp^iI9O8KnTj,]H?D*r7'M;PwZ9K0E^k&-cpI;.p/6_vwoFMV<->#%Xi.LxVnrU(4&8/P+:hLSKj$#U%]49t'I:rgMi'FL@a:0Y-uA[39',(vbma*"
"hU%<-SRF`Tt:542R_VV$p@[p8DV[A,?1839FWdF<TddF<9Ah-6&9tWoDlh]&1SpGMq>Ti1O*H&#(AL8[_P%.M>v^-))qOT*F5Cq0`Ye%+$B6i:7@0IX<N+T+0MlMBPQ*Vj>SsD<U4JHY"
"8kD2)2fU/M#$e.)T4,_=8hLim[&);?UkK'-x?'(:siIfL<$pFM`i<?%W(mGDHM%>iWP,##P`%/L<eXi:@Z9C.7o=@(pXdAO/NLQ8lPl+HPOQa8wD8=^GlPa8TKI1CjhsCTSLJM'/Wl>-"
"S(qw%sf/@%#B6;/U7K]uZbi^Oc^2n<bhPmUkMw>%t<)'mEVE''n`WnJra$^TKvX5B>;_aSEK',(hwa0:i4G?.Bci.(X[?b*($,=-n<.Q%`(X=?+@Am*Js0&=3bh8K]mL<LoNs'6,'85`"
"0?t/'_U59@]ddF<#LdF<eWdF<OuN/45rY<-L@&#+fm>69=Lb,OcZV/);TTm8VI;?%OtJ<(b4mq7M6:u?KRdF<gR@2L=FNU-<b[(9c/ML3m;Z[$oF3g)GAWqpARc=<ROu7cL5l;-[A]%/"
"+fsd;l#SafT/f*W]0=O'$(Tb<[)*@e775R-:Yob%g*>l*:xP?Yb.5)%w_I?7uk5JC+FS(m#i'k.'a0i)9<7b'fs'59hq$*5Uhv##pi^8+hIEBF`nvo`;'l0.^S1<-wUK2/Coh58KKhLj"
"M=SO*rfO`+qC`W-On.=AJ56>>i2@2LH6A:&5q`?9I3@@'04&p2/LVa*T-4<-i3;M9UvZd+N7>b*eIwg:CC)c<>nO&#<IGe;__.thjZl<%w(Wk2xmp4Q@I#I9,DF]u7-P=.-_:YJ]aS@V"
"?6*C()dOp7:WL,b&3Rg/.cmM9&r^>$(>.Z-I&J(Q0Hd5Q%7Co-b`-c<N(6r@ip+AurK<m86QIth*#v;-OBqi+L7wDE-Ir8K['m+DDSLwK&/.?-V%U_%3:qKNu$_b*B-kp7NaD'QdWQPK"
"Yq[@>P)hI;*_F]u`Rb[.j8_Q/<&>uu+VsH$sM9TA%?)(vmJ80),P7E>)tjD%2L=-t#fK[%`v=Q8<FfNkgg^oIbah*#8/Qt$F&:K*-(N/'+1vMB,u()-a.VUU*#[e%gAAO(S>WlA2);Sa"
">gXm8YB`1d@K#n]76-a$U,mF<fX]idqd)<3,]J7JmW4`6]uks=4-72L(jEk+:bJ0M^q-8Dm_Z?0olP1C9Sa&H[d&c$ooQUj]Exd*3ZM@-WGW2%s',B-_M%>%Ul:#/'xoFM9QX-$.QN'>"
"[%$Z$uF6pA6Ki2O5:8w*vP1<-1`[G,)-m#>0`P&#eb#.3i)rtB61(o'$?X3B</R90;eZ]%Ncq;-Tl]#F>2Qft^ae_5tKL9MUe9b*sLEQ95C&`=G?@Mj=wh*'3E>=-<)Gt*Iw)'QG:`@I"
"wOf7&]1i'S01B+Ev/Nac#9S;=;YQpg_6U`*kVY39xK,[/6Aj7:'1Bm-_1EYfa1+o&o4hp7KN_Q(OlIo@S%;jVdn0'1<Vc52=u`3^o-n1'g4v58Hj&6_t7$##?M)c<$bgQ_'SY((-xkA#"
"Y(,p'H9rIVY-b,'%bCPF7.J<Up^,(dU1VY*5#WkTU>h19w,WQhLI)3S#f$2(eb,jr*b;3Vw]*7NH%$c4Vs,eD9>XW8?N]o+(*pgC%/72LV-u<Hp,3@e^9UB1J+ak9-TN/mhKPg+AJYd$"
"MlvAF_jCK*.O-^(63adMT->W%iewS8W6m2rtCpo'RS1R84=@paTKt)>=%&1[)*vp'u+x,VrwN;&]kuO9JDbg=pO$J*.jVe;u'm0dr9l,<*wMK*Oe=g8lV_KEBFkO'oU]^=[-792#ok,)"
"i]lR8qQ2oA8wcRCZ^7w/Njh;?.stX?Q1>S1q4Bn$)K1<-rGdO'$Wr.Lc.CG)$/*JL4tNR/,SVO3,aUw'DJN:)Ss;wGn9A32ijw%FL+Z0Fn.U9;reSq)bmI32U==5ALuG&#Vf1398/pVo"
"1*c-(aY168o<`JsSbk-,1N;$>0:OUas(3:8Z972LSfF8eb=c-;>SPw7.6hn3m`9^Xkn(r.qS[0;T%&Qc=+STRxX'q1BNk3&*eu2;&8q$&x>Q#Q7^Tf+6<(d%ZVmj2bDi%.3L2n+4W'$P"
"iDDG)g,r%+?,$@?uou5tSe2aN_AQU*<h`e-GI7)?OK2A.d7_c)?wQ5AS@DL3r#7fSkgl6-++D:'A,uq7SvlB$pcpH'q3n0#_%dY#xCpr-l<F0NR@-##FEV6NTF6##$l84N1w?AO>'IAO"
"URQ##V^Fv-XFbGM7Fl(N<3DhLGF%q.1rC$#:T__&Pi68%0xi_&[qFJ(77j_&JWoF.V735&T,[R*:xFR*K5>>#`bW-?4Ne_&6Ne_&6Ne_&n`kr-#GJcM6X;uM6X;uM(.a..^2TkL%oR(#"
";u.T%fAr%4tJ8&><1=GHZ_+m9/#H1F^R#SC#*N=BA9(D?v[UiFY>>^8p,KKF.W]L29uLkLlu/+4T<XoIB&hx=T1PcDaB&;HH+-AFr?(m9HZV)FKS8JCw;SD=6[^/DZUL`EUDf]GGlG&>"
"w$)F./^n3+rlo+DB;5sIYGNk+i1t-69Jg--0pao7Sm#K)pdHW&;LuDNH@H>#/X-TI(;P>#,Gc>#0Su>#4`1?#8lC?#<xU?#@.i?#D:%@#HF7@#LRI@#P_[@#Tkn@#Xw*A#]-=A#a9OA#"
"d<F&#*;G##.GY##2Sl##6`($#:l:$#>xL$#B.`$#F:r$#JF.%#NR@%#R_R%#Vke%#Zww%#_-4&#3^Rh%Sflr-k'MS.o?.5/sWel/wpEM0%3'/1)K^f1-d>G21&v(35>V`39V7A4=onx4"
"A1OY5EI0;6Ibgr6M$HS7Q<)58C5w,;WoA*#[%T*#`1g*#d=#+#hI5+#lUG+#pbY+#tnl+#x$),#&1;,#*=M,#.I`,#2Ur,#6b.-#;w[H#iQtA#m^0B#qjBB#uvTB##-hB#'9$C#+E6C#"
"/QHC#3^ZC#7jmC#;v)D#?,<D#C8ND#GDaD#KPsD#O]/E#g1A5#KA*1#gC17#MGd;#8(02#L-d3#rWM4#Hga1#,<w0#T.j<#O#'2#CYN1#qa^:#_4m3#o@/=#eG8=#t8J5#`+78#4uI-#"
"m3B2#SB[8#Q0@8#i[*9#iOn8#1Nm;#^sN9#qh<9#:=x-#P;K2#$%X9#bC+.#Rg;<#mN=.#MTF.#RZO.#2?)4#Y#(/#[)1/#b;L/#dAU/#0Sv;#lY$0#n`-0#sf60#(F24#wrH0#%/e0#"
"TmD<#%JSMFove:CTBEXI:<eh2g)B,3h2^G3i;#d3jD>)4kMYD4lVu`4m`:&5niUA5@(A5BA1]PBB:xlBCC=2CDLXMCEUtiCf&0g2'tN?PGT4CPGT4CPGT4CPGT4CPGT4CPGT4CPGT4CP"
"GT4CPGT4CPGT4CPGT4CPGT4CPGT4CP-qekC`.9kEg^+F$kwViFJTB&5KTB&5KTB&5KTB&5KTB&5KTB&5KTB&5KTB&5KTB&5KTB&5KTB&5KTB&5KTB&5KTB&5KTB&5o,^<-28ZI'O?;xp"
"O?;xpO?;xpO?;xpO?;xpO?;xpO?;xpO?;xpO?;xpO?;xpO?;xpO?;xpO?;xpO?;xp;7q-#lLYI:xvD=#";
static const char* GetCompressedFontDataTTFBase85()
{
	return proggy_clean_ttf_compressed_data_base85;
}