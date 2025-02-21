#include "font_opengl.h"


static void CheckGLError(const char* stmt, const char* fname, int line) {
	GLenum err = glGetError();
	while (err != GL_NO_ERROR) {
		std::cerr << "OpenGL error " << err << " at " << fname << ":" << line << " for " << stmt << std::endl;
		err = glGetError();
	}
}
#define GL_CHECK(stmt) do { stmt; CheckGLError(#stmt, __FILE__, __LINE__); } while (0)
#define PI 3.14159265358979323846f
//io输入输出端口
BuiltIO* io = nullptr;//总信息的访问口
static bool IsOtherMenuHoverable(ShowWindowContainer*ShowWin,ShowMenu*showmenu)
{
	for (ShowMenu*tmpMenu:ShowWin->MenuArray)
	{
		if (tmpMenu!=showmenu)
		{
			if (tmpMenu->MenuRect.Contain(io->InputMessage.MousePos))//只要出这个菜单之外的菜单有悬浮反应就会打断本菜单的悬浮
				return true;	
		}
	}
	return false;
}
static void Clamp(float&Value,float Min,float Max)//限制函数功能
{
	if (Value > Max)
		Value = Max;
	else if (Value < Min)
		Value = Min;
	//在这个区间什么都不做
}

static DynamicArray<unsigned int> TextChar_utf_8_ToUnicode(const char* utf8_str)
{
	DynamicArray<unsigned int>  unicode_points;
	const unsigned int SecondByteValidBitCount = 6, thirdByteValidBitCount = 6;
	for (unsigned int i = 0; utf8_str[i] != '\0';)
	{
		unsigned char C = utf8_str[i];//读取单个UTF-8字符
		unsigned int unicode = 0x00;
		if (C < 0x80)//C<0x80范围的 一个字节  字符为单字节的ascll码
		{
			unicode = C;
			unicode_points.push_back(unicode);
			i++;
		}
		else if (C < 0xE0)//0x80<C<0xE0 使用 2 个字节进行编码 提取第一个字节的后五位 和第二个字节的后六位 所以第一个字节需要移动第二个字节的有效位数 合并为一个数据
		{

			if (utf8_str[i + 1] != '\0')//如果第二个字节不为空
			{
				unicode |= (utf8_str[i + 1] & 0x3F);//获取第二个字节的后六位有效数位
				unicode |= ((C & 0x1F) << SecondByteValidBitCount);//获取第一个字节的后五位有效数位 并最终或操作合并形成一个长字节的数据
				unicode_points.push_back(unicode);
				i += 2;
			}
			else
				std::cerr << "无效 UTF-8 序列" << std::endl;
		}
		else if (C < 0xF0)//C<0xF0 使用 3 个字节进行编码   提取第一个字节的后4位 和第二个字节的后六位 第三个字节的后六位 所以第一个字节需要移动第二个字节和第三个字节的位数 ...合并为一个数据
		{
			if (utf8_str[i + 2] != '\0')//如果第三个字节不为空
			{

				unicode |= (utf8_str[i + 2] & 0x3F);//获取第三个字节的后六位
				unicode |= ((utf8_str[i + 1] & 0x3F) << thirdByteValidBitCount);//获取第二个字节的后六位
				unicode |= ((C & 0x0F) << (SecondByteValidBitCount + thirdByteValidBitCount));//获取第一个字节的后4位有效数位 并最终或操作合并形成一个长字节的数据
				unicode_points.push_back(unicode);
				i += 3;
			}
			else
				std::cerr << "无效 UTF-8 序列" << std::endl;
		}
	}
	return unicode_points;
}
//计算单个文本的宽高
static glm::vec2 CalcuTextSize(const char*Text,float scalex,float scaley)
{
	//获取io的字体方块的X0 X1 Y0 Y1 计算字符方块宽度 字符方块高度 字符步进距离Advance 通过累加（每个字符的宽度 步进距离）=总文本宽度 取每个字符的高度的最大值就是文本的高度
	glm::vec2 textSize = glm::vec2(0,0);
	DynamicArray<unsigned int> str = TextChar_utf_8_ToUnicode(Text);//将utf-8转为unicode索引
	for (unsigned int i=0;i<str.size();i++)
	{
		unsigned int C = static_cast<unsigned int>(str[i]);
		unsigned int glyphid = 0;
		if (!io->atlas.glyphfonts->FindGlyph(C, glyphid))//寻找字符字形索引
			continue;
		FontGlyph glyph = io->atlas.glyphfonts->Glyphs[glyphid];
		textSize.y = std::max(textSize.y,fabs(scaley*(glyph.Y1- glyph.Y0)));
		textSize.x += scalex * glyph.AdvanceX;
	}
	return  textSize;
}
//圆周率
void DrawList::Clear()
{
	this->_VtxBuffer.clear();
	this->_IdxBuffer.clear();
	this->_VtxBuffer.resize(0);
	this->_IdxBuffer.resize(0);
	_ArcPath.resize(0);
	VtxCurrentIdx = 0;
}
void DrawList::ReserveForBuffer(unsigned int vtxcount, unsigned int idxcount)
{
	if (vtxcount == 0 || idxcount == 0) return;
	unsigned int oldVtxSize = this->_VtxBuffer.size();
	this->_VtxBuffer.resize(oldVtxSize + vtxcount);
	this->VtxWritePtr = this->_VtxBuffer.Data + oldVtxSize;
	unsigned int oldIdxSize = this->_IdxBuffer.size();
	this->_IdxBuffer.resize(oldIdxSize + idxcount);
	this->_IdxWritePtr = this->_IdxBuffer.Data + oldIdxSize;
}
void DrawList::_PathArc(glm::vec2 center, float radius, unsigned int MinSample, unsigned int MaxSample, float scale)
{
	if (radius * scale < 0.5f) // 如果缩放后的半径太小，直接返回中心点
	{
		_ArcPath.push_back(center);
		return;
	}

	unsigned int step = 1; // 每次采样的步进
	unsigned int Size = (MaxSample - MinSample) / step; // 要生成的顶点数
	unsigned int oldSize = _ArcPath.size();
	_ArcPath.resize(oldSize + Size); // 预分配存储空间

	glm::vec2* outptr = _ArcPath.Data + oldSize;
	for (unsigned int sampleIndex = MinSample; sampleIndex < MaxSample; sampleIndex += step)
	{
		unsigned int actualIndex = true ? sampleIndex : (MaxSample - (sampleIndex - MinSample) - 1);
		glm::vec2 s = io->ArcFastLookUpTable[actualIndex]; // 使用查找表获取三角函数值
		outptr->x = center.x + s.x * radius * scale; // o.x + r * cos(A) * scale
		outptr->y = center.y + s.y * radius * scale; // o.y + r * sin(A) * scale
		outptr++;
	}
}
void DrawList::PolyConvexFill(glm::vec4 color)
{
	unsigned int pointsCount = _ArcPath.size();
	if (pointsCount < 3) return; // 如果顶点数小于 3，无法构成多边形

	unsigned int faceCount = pointsCount - 2; // 三角形的数量
	unsigned int VtxCount = pointsCount, IdxCount = faceCount * 3;
	ReserveForBuffer(VtxCount, IdxCount); // 预留顶点和索引缓冲区
	glm::vec2 whiteTexCoord = glm::vec2(io->atlas.WhiteU1, io->atlas.WhiteV1);
	// 填充顶点缓冲区
	for (unsigned int i = 0; i < VtxCount; i++)
	{
		VtxWritePtr->pos = _ArcPath[i];
		VtxWritePtr->Tex = whiteTexCoord; // 纹理坐标：白色色块
		VtxWritePtr->Col = color;
		VtxWritePtr++;
	}

	// 填充索引缓冲区（生成三角形）
	for (unsigned int j = 2; j < VtxCount; j++)
	{
		_IdxWritePtr[0] = VtxCurrentIdx;       // 三角形的第一个顶点
		_IdxWritePtr[1] = VtxCurrentIdx + j - 1; // 第二个顶点
		_IdxWritePtr[2] = VtxCurrentIdx + j;     // 第三个顶点
		_IdxWritePtr += 3;
	}
	VtxCurrentIdx += VtxCount; // 更新当前顶点索引

}
void DrawList::AddLine(glm::vec2 p1, glm::vec2 p2, glm::vec4 color, float thickness)
{

	unsigned int VtxCount = 4 , IdxCount = 6 ;
	ReserveForBuffer(VtxCount, IdxCount);

	// 计算线条的方向向量
	glm::vec2 dir = p2 - p1;
	glm::vec2 perp = glm::vec2(-dir.y, dir.x); // 垂直向量
	float length = glm::length(dir);
	if (length == 0.0f) return; // 避免除以零
	glm::vec2 whiteTexCoord = glm::vec2(io->atlas.WhiteU1, io->atlas.WhiteV1);
	// 归一化方向向量和垂直向量
	dir /= length;
	perp /= length;

	// 计算线条的四个顶点
	glm::vec2 p1a = p1 + perp * thickness * 0.5f;
	glm::vec2 p1b = p1 - perp * thickness * 0.5f;
	glm::vec2 p2a = p2 + perp * thickness * 0.5f;
	glm::vec2 p2b = p2 - perp * thickness * 0.5f;

	// 添加顶点到顶点缓冲区
	VtxWritePtr[0].pos = p1a;VtxWritePtr[0].Tex=whiteTexCoord; VtxWritePtr[0].Col = color;
	VtxWritePtr[1].pos = p1b;VtxWritePtr[1].Tex=whiteTexCoord; VtxWritePtr[1].Col = color;
	VtxWritePtr[2].pos = p2a;VtxWritePtr[2].Tex=whiteTexCoord; VtxWritePtr[2].Col = color;
	VtxWritePtr[3].pos = p2b;VtxWritePtr[3].Tex=whiteTexCoord; VtxWritePtr[3].Col = color;

	// 添加索引到索引缓冲区
	_IdxWritePtr[0] = VtxCurrentIdx;
	_IdxWritePtr[1] = VtxCurrentIdx + 1;
	_IdxWritePtr[2] = VtxCurrentIdx + 2;
	_IdxWritePtr[3] = VtxCurrentIdx + 1;
	_IdxWritePtr[4] = VtxCurrentIdx + 2;
	_IdxWritePtr[5] = VtxCurrentIdx + 3;

	// 更新指针和索引
	VtxWritePtr += 4;
	VtxCurrentIdx += 4;
	_IdxWritePtr += 6;
}
void DrawList::AddTriangle(glm::vec2 p1, glm::vec2 p2, glm::vec2 p3, glm::vec4 color, float thickness)
{
	// 绘制三条边
	AddLine(p1, p2, color, thickness); // 第一条边
	AddLine(p2, p3, color, thickness); // 第二条边
	AddLine(p3, p1, color, thickness); // 第三条边
}
void DrawList::AddRectTriangleBorder(glm::vec2 Min, glm::vec2 Max, glm::vec4 color, float thickness)
{
	// 计算矩形的四个顶点
	glm::vec2 p1 = Min;                // 左下角
	glm::vec2 p2 = glm::vec2(Max.x, Min.y); // 右下角
	glm::vec2 p3 = Max;                // 右上角
	glm::vec2 p4 = glm::vec2(Min.x, Max.y); // 左上角

	// 绘制两个基本三角形的边框
	AddTriangle(p1, p2, p3, color, thickness); // 第一个三角形
	AddTriangle(p3, p4, p1, color, thickness); // 第二个三角形
}
void DrawList::AddRectBorder(glm::vec2 Min, glm::vec2 Max, glm::vec4 color, float thickness)
{
	// 绘制四条边
	AddLine(Min, glm::vec2(Max.x, Min.y), color, thickness); // 上边
	AddLine(glm::vec2(Max.x, Min.y), Max, color, thickness); // 右边
	AddLine(Max, glm::vec2(Min.x, Max.y), color, thickness); // 下边
	AddLine(glm::vec2(Min.x, Max.y), Min, color, thickness); // 左边
}
//void DrawList::AddTime(glm::vec2 Position, glm::vec3 Color, float scalex, float scaley)
//{
//	// 获取当前时间
//	auto now = std::chrono::system_clock::now();
//	auto in_time_t = std::chrono::system_clock::to_time_t(now);
//
//	// 将时间格式化为几点几分的字符串
//	std::stringstream ss;
//	std::tm timeinfo;
//	localtime_s(&timeinfo, &in_time_t);
//	ss << std::put_time(&timeinfo, "%Y/%m/%d %H:%M:%S");
//	std::string timeString = ss.str();
//
//	// 调用 AddText 函数来绘制时间字符串
//	AddText(Position, Color, timeString.c_str(), scalex, scaley);
//}
void DrawList::AddRectFill(glm::vec2 Min, glm::vec2 Max, glm::vec4 color, float rounding, float scale)
{
	_ArcPath.clear(); // 清空之前的路径

	// 计算矩形的中心点
	glm::vec2 center = (Min + Max) * 0.5f;

	// 缩放时以中心点为基准调整 Min 和 Max
	glm::vec2 scaledMin = Min;
	glm::vec2 scaledMax = center + (Max - center) * scale;//相对中点进行缩放
	float scaledRounding = rounding * scale;

	// 添加四个圆角路径
	_PathArc(glm::vec2(scaledMin.x + scaledRounding, scaledMin.y + scaledRounding), scaledRounding, 24, 36, scale); // 左下角
	_PathArc(glm::vec2(scaledMax.x - scaledRounding, scaledMin.y + scaledRounding), scaledRounding, 36, 48, scale); // 右下角
	_PathArc(glm::vec2(scaledMax.x - scaledRounding, scaledMax.y - scaledRounding), scaledRounding, 0, 12, scale);  // 右上角
	_PathArc(glm::vec2(scaledMin.x + scaledRounding, scaledMax.y - scaledRounding), scaledRounding, 12, 24, scale); // 左上角

	// 填充多边形
	PolyConvexFill(color);

}
void DrawList::AddText_VertTriangleBorder(glm::vec2 TextPosition, glm::vec4 LineColor, const char* Text, float scalex, float scaley, float thickness)
{
	const float lineHeight = io->atlas.fontSize * scaley;
	glm::vec2 Pos = TextPosition;
	DynamicArray<unsigned int> str = TextChar_utf_8_ToUnicode(Text);//将utf-8转为unicode索引
	for (unsigned int index = 0; index < str.size(); index++)
	{
		unsigned int C = static_cast<unsigned int>(str[index]);
		if (C == '\n')
		{
			Pos.y += lineHeight;
			Pos.x = TextPosition.x;
			continue;
		}
		unsigned int glyphid = 0;
		if (!io->atlas.glyphfonts->FindGlyph(C, glyphid))//寻找字符字形索引
			continue;
		FontGlyph glyph = io->atlas.glyphfonts->Glyphs[glyphid];

		float x0 = glyph.X0 * scalex + Pos.x;//min.x
		float y0 = glyph.Y0 * scaley + Pos.y;//min.y
		float x1 = glyph.X1 * scalex + Pos.x;//max.x
		float y1 = glyph.Y1 * scaley + Pos.y;//max.y
		glm::vec2 fontBoxmin = glm::vec2(x0,y0);
		glm::vec2 fontBoxmax = glm::vec2(x1, y1);
		AddRectTriangleBorder(fontBoxmin, fontBoxmax,LineColor,thickness);

		Pos.x += glyph.AdvanceX * scalex;
	}
}
void DrawList::AddText(glm::vec2 TextPosition, glm::vec4 TextColor, const char* Text, float scalex, float scaley)
{
	const float lineHeight = io->atlas.fontSize * scaley;
	glm::vec2 Pos = TextPosition;
	DynamicArray<unsigned int> str = TextChar_utf_8_ToUnicode(Text);//将utf-8转为unicode索引
	unsigned int RectVtxCount = 4 * str.size(), RectIdxCount = 6 * str.size();
	ReserveForBuffer(RectVtxCount, RectIdxCount);
	for (unsigned int index = 0; index < str.size(); index++)
	{
		unsigned int C = static_cast<unsigned int>(str[index]);
		if (C == '\n')
		{
			Pos.y += lineHeight;
			Pos.x = TextPosition.x;
			continue;
		}
		unsigned int glyphid = 0;
		if (!io->atlas.glyphfonts->FindGlyph(C, glyphid))//寻找字符字形索引
			continue;
		FontGlyph glyph = io->atlas.glyphfonts->Glyphs[glyphid];
		float U0 = glyph.U0;//texmin.x
		float V0 = glyph.V0;//texmin.y
		float U1 = glyph.U1;//texmax.x
		float V1 = glyph.V1;//texmax.y


		float x0 = glyph.X0 * scalex + Pos.x;//min.x
		float y0 = glyph.Y0 * scaley + Pos.y;//min.y
		float x1 = glyph.X1 * scalex + Pos.x;//max.x
		float y1 = glyph.Y1 * scaley + Pos.y;//max.y






		VtxWritePtr[0].pos = glm::vec2(x0, y0); VtxWritePtr[0].Tex = glm::vec2(U0, V0); VtxWritePtr[0].Col = TextColor;
		VtxWritePtr[1].pos = glm::vec2(x0, y1);  VtxWritePtr[1].Tex = glm::vec2(U0, V1);  VtxWritePtr[1].Col = TextColor;
		VtxWritePtr[2].pos = glm::vec2(x1, y0); VtxWritePtr[2].Tex = glm::vec2(U1, V0); VtxWritePtr[2].Col = TextColor;
		VtxWritePtr[3].pos = glm::vec2(x1, y1); VtxWritePtr[3].Tex = glm::vec2(U1, V1); VtxWritePtr[3].Col = TextColor;

		_IdxWritePtr[0] = VtxCurrentIdx;      _IdxWritePtr[1] = VtxCurrentIdx + 1;   _IdxWritePtr[2] = VtxCurrentIdx + 2;
		_IdxWritePtr[3] = VtxCurrentIdx + 1;  _IdxWritePtr[4] = VtxCurrentIdx + 2; _IdxWritePtr[5] = VtxCurrentIdx + 3;

		VtxWritePtr += 4;
		VtxCurrentIdx += 4;
		_IdxWritePtr += 6;



		Pos.x += glyph.AdvanceX * scalex;


	}
}

void DrawList::AddTextSizeRectBorder(glm::vec2 StartPosition, glm::vec2 TextSize, glm::vec4 Color, float thickness)
{
	AddLine(StartPosition,glm::vec2(StartPosition.x, StartPosition.y- TextSize.y),Color,thickness);
	AddLine(glm::vec2(StartPosition.x, StartPosition.y - TextSize.y), glm::vec2(StartPosition.x+ TextSize.x, StartPosition.y - TextSize.y),Color,thickness);
	AddLine(glm::vec2(StartPosition.x + TextSize.x, StartPosition.y - TextSize.y), glm::vec2(StartPosition.x + TextSize.x, StartPosition.y ),Color, thickness);
	AddLine(glm::vec2(StartPosition.x + TextSize.x, StartPosition.y), StartPosition, Color, thickness);
}

//void DrawList::AddNumber(glm::vec2 Position, glm::vec3 Color, float Number, int Precision, float scalex, float scaley)
//{
//	// 将数字转换为字符串
//	std::ostringstream numberStream;
//	numberStream << std::fixed << std::setprecision(Precision) << Number; // 使用固定点数表示法和设置精度
//	std::string numberString = numberStream.str();
//	// 调用 AddText 显示字符串
//	AddText(Position, Color, numberString.c_str(), scalex, scaley);
//}
//
//void DrawList::AddTestFont(glm::vec2 pos, unsigned int width, unsigned int height, glm::vec2 MinTexcoord, glm::vec2 MaxTexcoord, glm::vec3 col)
//{
//	unsigned int RectVtxCount = 4, RectIdxCount = 6;
//	ReserveForBuffer(RectVtxCount, RectIdxCount);
//	VtxWritePtr[0].pos = glm::vec2(pos.x, pos.y + height);          VtxWritePtr[0].Tex = glm::vec2(MinTexcoord.x, MaxTexcoord.y); VtxWritePtr[0].Col = col;
//	VtxWritePtr[1].pos = glm::vec2(pos.x, pos.y);                   VtxWritePtr[1].Tex = MinTexcoord; VtxWritePtr[1].Col = col;
//	VtxWritePtr[2].pos = glm::vec2(pos.x + width, pos.y + height);      VtxWritePtr[2].Tex = MaxTexcoord; VtxWritePtr[2].Col = col;
//	VtxWritePtr[3].pos = glm::vec2(pos.x + width, pos.y);           VtxWritePtr[3].Tex = glm::vec2(MaxTexcoord.x, MinTexcoord.y); VtxWritePtr[3].Col = col;
//
//	_IdxWritePtr[0] = VtxCurrentIdx;      _IdxWritePtr[1] = VtxCurrentIdx + 1;   _IdxWritePtr[2] = VtxCurrentIdx + 2;
//	_IdxWritePtr[3] = VtxCurrentIdx + 1;  _IdxWritePtr[4] = VtxCurrentIdx + 2; _IdxWritePtr[5] = VtxCurrentIdx + 3;
//
//	VtxCurrentIdx += 4;
//	VtxWritePtr += 4;
//	_IdxWritePtr += 6;
//}
//
//void DrawList::AddRotText(glm::vec2 TextPosition, glm::vec3 TextColor, const char* Text, FontAtlas* atlas, float scalex, float scaley, float time)
//{
//	if (!Text || !atlas || !atlas->glyphfonts) return;
//	
//	const float lineHeight = atlas->fontSize * scaley;
//	glm::vec2 Pos = TextPosition;
//
//	
//	
//	std::vector<unsigned int> str=TextChar_utf_8_ToUnicode(Text);//将utf-8转为unicode索引
//	unsigned int RectVtxCount = 4 * str.size(), RectIdxCount = 6 * str.size();
//	ReserveForBuffer(RectVtxCount, RectIdxCount);
//	for (size_t index = 0; index < str.size(); ++index)
//	{
//		unsigned int C = static_cast<unsigned int>(str[index]);
//		if (C == '\n')
//		{
//			Pos.y += lineHeight;
//			Pos.x = TextPosition.x;
//			continue;
//		}
//
//		if (C >= atlas->glyphfonts->IndexSearchTable.size()) continue;
//
//		unsigned int glyphid = 0;
//		if (glyphid >= atlas->glyphfonts->Glyphs.size()) continue;
//		if (!atlas->glyphfonts->FindGlyph(C, glyphid))//寻找字符字形索引
//		{
//			std::cerr << "有字符不在此字符集记录范围内" << std::endl;
//			return;
//		}
//		
//
//		FontGlyph glyph = atlas->glyphfonts->Glyphs[glyphid];
//		float U0 = glyph.U0, V0 = glyph.V0, U1 = glyph.U1, V1 = glyph.V1;
//
//		// Calculate glyph position
//		float x0 = glyph.X0 * scalex + Pos.x;
//		float y0 = glyph.Y0 * scaley + Pos.y;
//		float x1 = glyph.X1 * scalex + Pos.x;
//		float y1 = glyph.Y1 * scaley + Pos.y;
//
//		// Calculate glyph center
//		glm::vec2 center = glm::vec2((x0 + x1) / 2.0f, (y0 + y1) / 2.0f);
//
//		// Apply rotation and wave effect
//		float angle = sin(time + index * 0.5f) * 0.3f; // 使各个字符错位相位差旋转
//		float waveOffset = sin(time + index * 0.3f) * 0.08; // 使各个字符错位波动 相位差
//
//		auto rotate = [center, angle](float x, float y) -> glm::vec2 {
//			float s = sin(angle);
//			float c = cos(angle);
//			x -= center.x;
//			y -= center.y;
//			float xNew = x * c - y * s;
//			float yNew = x * s + y * c;
//			return glm::vec2(xNew + center.x, yNew + center.y);
//			};
//
//		glm::vec2 p0 = rotate(x0, y1 + waveOffset);
//		glm::vec2 p1 = rotate(x0, y0 + waveOffset);
//		glm::vec2 p2 = rotate(x1, y1 + waveOffset);
//		glm::vec2 p3 = rotate(x1, y0 + waveOffset);
//
//		VtxWritePtr[0].pos = p0; VtxWritePtr[0].Tex = glm::vec2(U0, V1); VtxWritePtr[0].Col = TextColor;
//		VtxWritePtr[1].pos = p1; VtxWritePtr[1].Tex = glm::vec2(U0, V0); VtxWritePtr[1].Col = TextColor;
//		VtxWritePtr[2].pos = p2; VtxWritePtr[2].Tex = glm::vec2(U1, V1); VtxWritePtr[2].Col = TextColor;
//		VtxWritePtr[3].pos = p3; VtxWritePtr[3].Tex = glm::vec2(U1, V0); VtxWritePtr[3].Col = TextColor;
//
//		_IdxWritePtr[0] = VtxCurrentIdx;      _IdxWritePtr[1] = VtxCurrentIdx + 1;   _IdxWritePtr[2] = VtxCurrentIdx + 2;
//		_IdxWritePtr[3] = VtxCurrentIdx + 1;  _IdxWritePtr[4] = VtxCurrentIdx + 2;   _IdxWritePtr[5] = VtxCurrentIdx + 3;
//
//		VtxWritePtr += 4;
//		VtxCurrentIdx += 4;
//		_IdxWritePtr += 6;
//
//		Pos.x += glyph.AdvanceX * scalex;
//	}
//}

void DrawList::AddClipText(glm::vec2 TextPosition, glm::vec4 TextColor, const char* Text, ClipRect clipRect, float scalex, float scaley)
{
	const float lineHeight = io->atlas.fontSize * scaley;
	glm::vec2 Pos = TextPosition;
	DynamicArray<unsigned int> str = TextChar_utf_8_ToUnicode(Text);//将utf-8转为unicode索引
	unsigned int RectVtxCount = 4 * str.size(), RectIdxCount = 6 * str.size();
	ReserveForBuffer(RectVtxCount, RectIdxCount);
	for (unsigned int index = 0; index < str.size(); index++)
	{
		unsigned int C = static_cast<unsigned int>(str[index]);
		if ((char)C == '\n')
		{
			Pos.y += lineHeight;
			Pos.x = TextPosition.x;
			continue;
		}

		unsigned int glyphid = 0;
		if (!io->atlas.glyphfonts->FindGlyph(C, glyphid))//寻找字符字形索引
			continue;
		FontGlyph glyph = io->atlas.glyphfonts->Glyphs[glyphid];
		float U0 = glyph.U0;//texmin.x
		float V0 = glyph.V0;//texmin.y
		float U1 = glyph.U1;//texmax.x
		float V1 = glyph.V1;//texmax.y


		float x0 = glyph.X0 * scalex + Pos.x;//min.x
		float y0 = glyph.Y0 * scaley + Pos.y;//min.y
		float x1 = glyph.X1 * scalex + Pos.x;//max.x
		float y1 = glyph.Y1 * scaley + Pos.y;//max.y

		if (x1 <= clipRect.Max.x && x0 >= clipRect.Min.x)
		{
			//std::cout << "条件成立" << std::endl;
			if (x0 < clipRect.Min.x)
			{
				U0 = U0 + (1.0f - (x1 - clipRect.Min.x) / (x1 - x0)) * (U1 - U0);
				x0 = clipRect.Min.x;
			}
			if (y0 < clipRect.Min.y)
			{
				V0 = V0 + (1.0f - (clipRect.Min.y - y0) / (y1 - y0)) * (V1 - V0);
				y0 = clipRect.Min.y;
			}
			if (x1 > clipRect.Max.x)
			{
				U1 = U0 + ((clipRect.Max.x - x0) / (x1 - x0)) * (U1 - U0);
				x1 = clipRect.Max.x;
			}
			if (y1 > clipRect.Max.y)
			{
				V1 = V0 + ((clipRect.Max.y - y0) / (y1 - y0)) * (V1 - V0);
				y1 = clipRect.Max.y;
			}

			if (y0 >= y1)
			{
				Pos.x += glyph.AdvanceX * scalex;//相当于字体符整个超过了clip区域就不执行下面
				continue;
			}

			VtxWritePtr[0].pos = glm::vec2(x0, y0); VtxWritePtr[0].Tex = glm::vec2(U0, V0); VtxWritePtr[0].Col = TextColor;
			VtxWritePtr[1].pos = glm::vec2(x0, y1);  VtxWritePtr[1].Tex = glm::vec2(U0, V1);  VtxWritePtr[1].Col = TextColor;
			VtxWritePtr[2].pos = glm::vec2(x1, y0); VtxWritePtr[2].Tex = glm::vec2(U1, V0); VtxWritePtr[2].Col = TextColor;
			VtxWritePtr[3].pos = glm::vec2(x1, y1); VtxWritePtr[3].Tex = glm::vec2(U1, V1); VtxWritePtr[3].Col = TextColor;

			_IdxWritePtr[0] = VtxCurrentIdx;      _IdxWritePtr[1] = VtxCurrentIdx + 1;   _IdxWritePtr[2] = VtxCurrentIdx + 2;
			_IdxWritePtr[3] = VtxCurrentIdx + 1;  _IdxWritePtr[4] = VtxCurrentIdx + 2; _IdxWritePtr[5] = VtxCurrentIdx + 3;

			VtxWritePtr += 4;
			VtxCurrentIdx += 4;
			_IdxWritePtr += 6;
		}

		Pos.x += glyph.AdvanceX * scalex;
	}
}
void DrawList::RendeAllCharactersIntoScreen(glm::vec2 pos, float ConstraintWidth, glm::vec4 Col, float scalex, float scaley)
{

	const float lineHeight = io->atlas.fontSize * scaley * 2;
	unsigned int RectVtxCount = 4 * io->atlas.glyphfonts->Glyphs.size(), RectIdxCount = 6 * io->atlas.glyphfonts->Glyphs.size();
	glm::vec2 Pos = pos;
	ReserveForBuffer(RectVtxCount, RectIdxCount);
	for (unsigned int i = 0; i < io->atlas.glyphfonts->Glyphs.size(); i++)
	{
		FontGlyph glyph = io->atlas.glyphfonts->Glyphs[i];
		if ((Pos.x - pos.x) > ConstraintWidth)
		{
			Pos.y += lineHeight;
			Pos.x = pos.x;
		}
		float U0 = glyph.U0;//texmin.x
		float V0 = glyph.V0;//texmin.y
		float U1 = glyph.U1;//texmax.x
		float V1 = glyph.V1;//texmax.y


		float x0 = glyph.X0 * scalex + Pos.x;//min.x
		float y0 = glyph.Y0 * scaley + Pos.y;//min.y
		float x1 = glyph.X1 * scalex + Pos.x;//max.x
		float y1 = glyph.Y1 * scaley + Pos.y;//max.y

		VtxWritePtr[0].pos = glm::vec2(x0, y0); VtxWritePtr[0].Tex = glm::vec2(U0, V0); VtxWritePtr[0].Col = Col;
		VtxWritePtr[1].pos = glm::vec2(x0, y1);  VtxWritePtr[1].Tex = glm::vec2(U0, V1);  VtxWritePtr[1].Col = Col;
		VtxWritePtr[2].pos = glm::vec2(x1, y0); VtxWritePtr[2].Tex = glm::vec2(U1, V0); VtxWritePtr[2].Col = Col;
		VtxWritePtr[3].pos = glm::vec2(x1, y1); VtxWritePtr[3].Tex = glm::vec2(U1, V1); VtxWritePtr[3].Col = Col;

		_IdxWritePtr[0] = VtxCurrentIdx;      _IdxWritePtr[1] = VtxCurrentIdx + 1;   _IdxWritePtr[2] = VtxCurrentIdx + 2;
		_IdxWritePtr[3] = VtxCurrentIdx + 1;  _IdxWritePtr[4] = VtxCurrentIdx + 2; _IdxWritePtr[5] = VtxCurrentIdx + 3;

		VtxWritePtr += 4;
		VtxCurrentIdx += 4;
		_IdxWritePtr += 6;


		Pos.x += glyph.AdvanceX * scalex * 2;
	}
}
void BuiltIO::CreateFontTexture()
{
	if (this->atlas.FontConfigData.size() == 0)//如果没有加载字体文件 就加载默认字体
	{
		this->atlas.Addfont_internal();//建立字体顶点信息并且绘制字符图集
	}
	this->atlas.BuildFont();
	outglTexInfo TexInfo = atlas.GetTexAlpha32();//返回输出图集信息
	GL_CHECK(glGenTextures(1, &this->TexId));
	GL_CHECK(glBindTexture(GL_TEXTURE_2D, this->TexId));//纹理化

	GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
	GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));

	GL_CHECK(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, TexInfo.width, TexInfo.height, 0, GL_RGBA,
		GL_UNSIGNED_BYTE, TexInfo.Texpixel));

	//先初始化绘制数据的显示视口
	this->drawdata.DisplayPos = glm::vec2(0, 0);
	this->drawdata.DisplaySize = glm::vec2(900, 800);//w h

	//声明着色器
	shader = new shaderProgram;
	//加载我们的着色器

	if (!shader->loadShaders("shader\\FontShader.vert", "shader\\FontShader.frag")) {
		std::cerr << "Error: Shader failed to load." << std::endl;
		return; // 或者终止程序
	}
	//建立drawlist三角角度函数表格查
	this->initializeArcTable();
}

void BuiltIO::CreateShowWindow_glBuffer(ShowWindowContainer* Show_Window)
{
	
	GL_CHECK(glGenVertexArrays(1, &Show_Window->content.Vao));
	GL_CHECK(glGenBuffers(1, &Show_Window->content.Vbo));
	GL_CHECK(glGenBuffers(1, &Show_Window->content.Ebo));
}


void BuiltIO::initializeArcTable()
{
	for (unsigned int i = 0; i < 48; i++)
	{
		const float Rad = (float)i * 2 * PI / 48;
		ArcFastLookUpTable[i] = glm::vec2(cosf(Rad), sinf(Rad));
	}


}

void BuiltIO::SetUpBufferState(GLuint vertex_object_array, GLuint vertex_Buffer_object, GLuint element_object_object)
{
	GL_CHECK(glBindVertexArray(vertex_object_array));
	GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, vertex_Buffer_object));
	GL_CHECK(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, element_object_object));


	GL_CHECK(glEnableVertexAttribArray(0));
	GL_CHECK(glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(DrawVert), (GLvoid*)offsetof(DrawVert, pos)));

	GL_CHECK(glEnableVertexAttribArray(1));
	GL_CHECK(glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(DrawVert), (GLvoid*)offsetof(DrawVert, Tex)));

	GL_CHECK(glEnableVertexAttribArray(2));
	GL_CHECK(glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(DrawVert), (GLvoid*)offsetof(DrawVert, Col)));


}
void BuiltIO::Bring_TopDrawDataLayer_ToFrontDisplay()
{

	ShowWindowContainer* Has_Bring_front_Window = nullptr;

	if (this->drawdata.DrawDataLayerBuffer.back()->Flag.Bring_Front)//如果你本身的置顶窗口需要有置顶操作 那就不要费那个劲儿访问整个内存是否哪个有置顶操作
		return;
	int index = -1;
	//如果显示顺序越靠下就循环次数就越多 相反就越少 所以要倒着访问 <O(n)
	for (int i = this->drawdata.DrawDataLayerBuffer.size() - 2; i >= 0; i--)
	{
		if (this->drawdata.DrawDataLayerBuffer[i]->Flag.Bring_Front)//如果这个窗口图层有置顶操作
		{
			Has_Bring_front_Window = this->drawdata.DrawDataLayerBuffer[i];
			index = i;
			break;
		}
	}
	if (index != -1)
	{
		// 使用 memmove 将元素向前移动一位
		memmove(&this->drawdata.DrawDataLayerBuffer[index], &this->drawdata.DrawDataLayerBuffer[index + 1], (this->drawdata.DrawDataLayerBuffer.size() - index - 1) * sizeof(ShowWindowContainer*));
		this->drawdata.DrawDataLayerBuffer.back() = Has_Bring_front_Window;//最后一个元素存放置顶绘制的窗口绘制资源
	}
}
void BuiltIO::AllowDrawDataLayerResponseEvent()
{
	bool found = false;  // 标志位，记录是否已经找到包含鼠标位置的优先置顶绘制窗口
	for (int i = this->drawdata.DrawDataLayerBuffer.size() - 1; i >= 0 && !found; i--)//从置顶顺序访问 如果置顶优先的吃了鼠标位置就响应 剩下的就不管清空判断flag 就是防止叠加的窗口同时响应鼠标是否在窗口的裁剪矩形的判断事件
	{
		if (this->drawdata.DrawDataLayerBuffer[i]->OutterRect.Contain(this->InputMessage.MousePos)) {
			this->drawdata.DrawDataLayerBuffer[i]->Flag.AllowResponseInputEvent = true;//以置顶顺序优先的优先设置鼠标响应
			found = true;  // 设置标志位
		}
		else {
			this->drawdata.DrawDataLayerBuffer[i]->Flag.AllowResponseInputEvent = false;
		}
	}//时间复杂度<O(n)


}
void BuiltIO::UpdateDrawDataBuffer(UI* DrawUI)
{
	if (this->Has_Bring_front_opera)//有置顶操作
	{
		this->Bring_TopDrawDataLayer_ToFrontDisplay();//对UI里面的窗口进行置顶操作
	}
	this->AllowDrawDataLayerResponseEvent();//对叠加窗口图层对鼠标的响应 置顶顺序优先的先争取对鼠标的响应操作
	
}
void BuiltIO::RenderDrawList()
{
	
	GL_CHECK(glEnable(GL_BLEND));
	GL_CHECK(glBlendEquation(GL_FUNC_ADD));
	GL_CHECK(glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA));
	GL_CHECK(glActiveTexture(GL_TEXTURE0));
	GL_CHECK(glBindTexture(GL_TEXTURE_2D, this->TexId));

	shader->use();
	//正交投影
	float R = this->drawdata.DisplayPos.x + this->drawdata.DisplaySize.x;
	float L = this->drawdata.DisplayPos.x;
	float T = this->drawdata.DisplayPos.y;
	float B = this->drawdata.DisplayPos.y + this->drawdata.DisplaySize.y;
	orthoProjection = glm::ortho(L, R, B, T, -1.0f, 1.0f);
	shader->setUniform("ProjectM", orthoProjection);
	for (unsigned int i = 0; i < this->drawdata.DrawDataLayerBuffer.size(); i++)
	{
		ShowWindowContainer*DrawDataLayer = this->drawdata.DrawDataLayerBuffer[i];
		//std::cout << "绘制窗口的顶点个数" << DrawDataLayer->content.drawlist._VtxBuffer.size() << std::endl;
		if (!DrawDataLayer->Flag.IsWindowVisible)//如果这个窗口不允许显示就不执行下面发送给gpu顶点缓冲操作
		{
			DrawDataLayer->content.drawlist.Clear();//同时你这个窗口顶点绘制资源不发送显示 直接清除掉
			continue;
		}
			
		DrawList* cmdDrawlist = &DrawDataLayer->content.drawlist;//获取的是每一个允许显示窗口的绘制内容
		DynamicArray<DrawVert> vtxBuffer = cmdDrawlist->_VtxBuffer;
		DynamicArray<unsigned int> idxBuffer = cmdDrawlist->_IdxBuffer;
		// 设置顶点数组对象和绑定缓冲区
		SetUpBufferState(DrawDataLayer->content.Vao, DrawDataLayer->content.Vbo, DrawDataLayer->content.Ebo);
		// 计算顶点缓冲区大小
	   //当之前存储的顶点缓冲大小和当前绘制文本的顶点缓冲大小不一样时进行重分配再进行更新
		if (DrawDataLayer->content.VertexBufferSize != vtxBuffer.size())
		{
			// 情况一：顶点数据量增加，重新分配整个缓冲区空间并复制数据
			// 情况二：顶点数据量减少，重新分配缓冲区空间为当前实际需要的大小
			DrawDataLayer->content.VertexBufferSize = vtxBuffer.size();

			GL_CHECK(glBufferData(GL_ARRAY_BUFFER, sizeof(DrawVert) * vtxBuffer.size(), nullptr, GL_DYNAMIC_DRAW));
			GL_CHECK(glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(DrawVert) * vtxBuffer.size(), vtxBuffer.Data));
		}
		else
		{   	// 情况三：顶点数据量相等，直接更新缓冲区中的数据
			GL_CHECK(glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(DrawVert) * vtxBuffer.size(), vtxBuffer.Data));
		}
		// 计算索引缓冲区大小
		//和顶点缓冲逻辑一样
		if (DrawDataLayer->content.IdxBufferSize != idxBuffer.size())//现在的索引缓冲的大小和上一次的大小比较 如果相等就更新 如果不一样就重新分配 每个绘制窗口的信息独立不干扰
		{
			DrawDataLayer->content.IdxBufferSize = idxBuffer.size();

			GL_CHECK(glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int) * idxBuffer.size(), nullptr, GL_DYNAMIC_DRAW));
			GL_CHECK(glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int) * idxBuffer.size(), idxBuffer.Data, GL_DYNAMIC_DRAW));
		}
		else
		{
			GL_CHECK(glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, sizeof(unsigned int) * idxBuffer.size(), idxBuffer.Data));
		}
		//填充完glBuffer完清除缓冲绘制
		cmdDrawlist->Clear();//这步操作顺便把每一个窗口的绘制列表清空方便下一次循环
		GL_CHECK(glBindVertexArray(DrawDataLayer->content.Vao));
		GL_CHECK(glDrawElements(GL_TRIANGLES, DrawDataLayer->content.IdxBufferSize, GL_UNSIGNED_INT, 0));
		GL_CHECK(glBindVertexArray(0));
	}
}

void BuiltIO::DeleteResource()
{
	for (ShowWindowContainer* showWindowDrawSource : this->drawdata.DrawDataLayerBuffer)
	{
		GL_CHECK(glDeleteVertexArrays(1, &showWindowDrawSource->content.Vao));
		GL_CHECK(glDeleteBuffers(1, &showWindowDrawSource->content.Vbo));
		GL_CHECK(glDeleteBuffers(1, &showWindowDrawSource->content.Ebo));
	}
}

//C代码
static void Mouse_callback(GLFWwindow* window, double xpos, double ypos)//更新IO的鼠标位移
{
	const double sensitivity = 0.93;
	static double lastxpos = 0, lastypos = 0;
	double xoffset = xpos - lastxpos;//获取x偏移量
	double yoffset = ypos - lastypos;//获取y偏移量
	lastxpos = xpos;
	lastypos = ypos;
	io->InputMessage.MouseDeltaOffset.x = xoffset * sensitivity;
	io->InputMessage.MouseDeltaOffset.y = yoffset * sensitivity;
}
static void Mouse_ButtonCallBack(GLFWwindow* window, int button, int action, int mods)//鼠标按键回调函数
{

	if (action == GLFW_PRESS)
		io->InputMessage.MouseDown[button] = true;
	else
		io->InputMessage.MouseDown[button] = false;

}
// 滚轮回调函数
static void Mouse_Scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
	const float sensitivity = 5.09f;
	io->InputMessage.Scroll = yoffset * sensitivity;
}
static void Key_Callback(GLFWwindow* window, int key, int scancode, int action, int mods)//键盘按键回调函数
{

	io->InputMessage.KeyMesage.key = key;
	if (action == GLFW_REPEAT || action == GLFW_PRESS)
		io->InputMessage.KeyMesage.press = true;
	else
		io->InputMessage.KeyMesage.press = false;
}
void SetUpContextIO()
{
	io = new BuiltIO;
}

void BuildBackendData()
{
	io->CreateFontTexture();//创建字体字符图集 并加载着色器 加载着色器时导致glBindVertexArray()这个OpenGL错误
	//io->SetupDrawData_VtxList(); //创建绘制数据顶点数组
	//设置默认的颜色
	io->Colors[TextCol] = glm::vec4(1, 1,1, 1);
	io->Colors[TitleRectCol] = glm::vec4(0.1, 0.1, 0.7, 1);
	io->Colors[OutterRectCol] = glm::vec4(0.2, 0.2, 0.2, 1);
	io->Colors[MenuRectCol] = glm::vec4(0.3, 0.1, 0.1, 1);
	io->Colors[HoverRectCol] = glm::vec4(0.4, 0.2, 0.2, 1);
	io->Colors[HoverTextRectCol] = glm::vec4(0.2, 0.1, 0.1, 1);
	io->Colors[ScrollBarCol]= glm::vec4(0.4, 0.4, 0.4, 1);
}

BuiltIO* GetIo()
{
	return io;
}
void UpdateEvent(GLFWwindow* window)
{
	/*写入IO对象里鼠标XY位移
	  写入IO对象里鼠标按键消息
	  写入IO对象里鼠标屏幕位置
	  写入IO对象里滚动条的滚动位移
	  写入IO对象里按键消息*/
	glfwSetCursorPosCallback(window, Mouse_callback); //写入IO对象里鼠标XY位移
	glfwSetMouseButtonCallback(window, Mouse_ButtonCallBack);//写入IO对象里鼠标按键消息
	int ShowWindowViewPortPosX = 0, ShowWindowViewPortPosY = 0, ShowWindowViewPortSizeW = 0, ShowWindowViewPortSizeH = 0, FrameBufferSizeW = 0, FrameBufferSizeH = 0;
	double RelativeCursorPosX = 0, RelativeCursorPosY = 0;
	glfwGetWindowSize(window, &ShowWindowViewPortSizeW, &ShowWindowViewPortSizeH);
	glfwGetFramebufferSize(window, &FrameBufferSizeW, &FrameBufferSizeH);
	glfwGetWindowPos(window, &ShowWindowViewPortPosX, &ShowWindowViewPortPosY);
	glfwGetCursorPos(window, &RelativeCursorPosX, &RelativeCursorPosY);
	io->InputMessage.MousePos = glm::vec2(ShowWindowViewPortPosX + RelativeCursorPosX, ShowWindowViewPortPosY + RelativeCursorPosY);//写入IO对象里鼠标屏幕位置

	if (ShowWindowViewPortSizeW <= 0.0001f && ShowWindowViewPortSizeH <= 0.0001f)
	{
		io->drawdata.framebufferscale.x = io->drawdata.framebufferscale.y = 0;
	}
	else
	{
		io->drawdata.framebufferscale.x = (float)FrameBufferSizeW / ShowWindowViewPortSizeW;
		io->drawdata.framebufferscale.y = (float)FrameBufferSizeH / ShowWindowViewPortSizeH;
	}
	io->drawdata.DisplaySize.x = ShowWindowViewPortSizeW;
	io->drawdata.DisplaySize.y = ShowWindowViewPortSizeH;
	io->drawdata.DisplayPos.x = ShowWindowViewPortPosX;
	io->drawdata.DisplayPos.y = ShowWindowViewPortPosY;
	glfwSetScrollCallback(window, Mouse_Scroll_callback);//写入IO对象里滚动条的滚动位移
	glfwSetKeyCallback(window, Key_Callback);

}
void ClearEvent()
{
	io->InputMessage.MouseDeltaOffset = glm::vec2(0, 0);
	io->InputMessage.MousePos = glm::vec2(0, 0);
	io->InputMessage.Scroll = 0;
	//因为按键类的判断是每时每刻执行不停更新 而位移数据是寄存的需要每次使用完清0
}
void  RenderDrawData(UI* DrawUI)
{
	float Display_W = io->drawdata.DisplaySize.x;
	float Display_H = io->drawdata.DisplaySize.y;
	float Xscale = io->drawdata.framebufferscale.x;
	float Yscale = io->drawdata.framebufferscale.y;
	glViewport(0, 0, (int)(Display_W * Xscale), (int)(Display_H * Yscale));
	io->UpdateDrawDataBuffer(DrawUI);//更新绘制数据缓冲
	io->RenderDrawList();
}
void DestructResource()
{
	io->DeleteResource();
}

void UI::StartShowWindow(const char* Text, bool Popen)
{
	
	ShowWindowContainer* showWindow = FindShowWindowByName(Text);
	bool ShowWindowJustCreated = (showWindow != nullptr); //是否已经创建过了
	if (!ShowWindowJustCreated)//如果没有被创建
	{
		showWindow = CreateNewShowWindow(Text); //防止一直new造成程序崩溃
	}
	showWindow->Flag.IsWindowVisible = Popen;
	//先计算标题的文本宽高
	glm::vec2 TextSize = CalcuTextSize(Text,showWindow->ScaleX, showWindow->ScaleY);
	//设置title标题栏的宽度、高度和外部裁剪框的宽度   后期再添加通过判断是否为第一帧 通过改变变量累次改变整个外部矩形 和标题框矩形的宽高
	showWindow->TitleRect.Width = showWindow->OutterRect.Width=TextSize.x + showWindow->spacing *showWindow->ScaleX* 149.8;
	showWindow->TitleRect.Height = TextSize.y + showWindow->spacing * showWindow->ScaleY *7.5;//标题框高度
	//设置OutterRect的左上角 右下角位置  
	showWindow->OutterRect.Min = showWindow->Pos;
	showWindow->OutterRect.Max = showWindow->Pos + glm::vec2(showWindow->OutterRect.Width, showWindow->OutterRect.Height);
	//设置Title标题框左上角 右下角位置
	showWindow->TitleRect.Min = showWindow->Pos;
	showWindow->TitleRect.Max = showWindow->Pos + glm::vec2(showWindow->TitleRect.Width, showWindow->TitleRect.Height);
	//	设置标题文本显示位置
	glm::vec2 TitleTextPos = showWindow->TitleRect.Min 
		+glm::vec2(showWindow->spacing  , showWindow->spacing )
		+ glm::vec2(0, TextSize.y);//文本绘制位置 =文本框最小位置 空出一些间隔 再空出文本高度再绘制
	 //绘制OutterRect外部裁剪矩形
	 showWindow->content.drawlist.AddRectFill(showWindow->OutterRect.Min, showWindow->OutterRect.Max,showWindow->COLOR[OutterRectCol],9,1);
	 // 绘制titleRect标题裁剪矩形
	 showWindow->content.drawlist.AddRectFill(showWindow->TitleRect.Min, showWindow->TitleRect.Max, showWindow->COLOR[TitleRectCol], 9, 1);
	//绘制title文本ClipText
	 showWindow->content.drawlist.AddClipText(TitleTextPos,showWindow->COLOR[TextCol],Text, showWindow->TitleRect,showWindow->ScaleX,showWindow->ScaleY);
	 //先将每个绘制内容颜色设置成默认在颜色 看下面有在操作可以改变这个颜色不 如果下边有操作可以改变这个颜色那就执行 如果没有相当于恢复原来在颜色
	 showWindow->COLOR[OutterRectCol] = io->Colors[OutterRectCol];
	 showWindow->COLOR[TitleRectCol] = io->Colors[TitleRectCol];
	 showWindow->COLOR[TextCol] = io->Colors[TextCol];

	 if (showWindow->Flag.AllowResponseInputEvent)//如果当前窗口允许响应输入事件
	 {
		 if (showWindow->OutterRect.Contain(io->InputMessage.MousePos))//如果鼠标悬停在这个外部裁剪矩形
		 {
			 //outterRect的颜色改变 否则颜色恢复原来
			 showWindow->COLOR[OutterRectCol] = glm::vec4(0.2,0.2,0,1);
			 //看是否标题文本框吃了鼠标 吃了outterRect的颜色恢复原来  TitleRect的颜色改变
			 if (showWindow->TitleRect.Contain(io->InputMessage.MousePos))
			 {
				 showWindow->COLOR[OutterRectCol] = io->Colors[OutterRectCol];
				 showWindow->COLOR[TitleRectCol] = glm::vec4(0.3,0.4,0.3,1);
			 }
		     //响应滚轮 判断按键改变内容的缩放比例
			 if (io->InputMessage.KeyMesage.key == GLFW_KEY_LEFT_CONTROL && io->InputMessage.KeyMesage.press)
			 {
				 showWindow->ScaleX += io->InputMessage.Scroll * 0.05f; // 滚动的增量控制大小的变化比例
				 showWindow->ScaleY += io->InputMessage.Scroll * 0.05f;
			 }
			 //等到鼠标左键按下 移动窗口位置 置顶操作
			 if (io->InputMessage.MouseDown[GLFW_MOUSE_BUTTON_LEFT])//如果对当前窗口有点击执行置顶 获取鼠标位移进行窗口位置移动
			 {
				 io->Has_Bring_front_opera = true;
				 showWindow->Flag.Bring_Front = true;
				 if(showWindow->Flag.Movable)//如果窗口移动的标志判断被激活了允许移动
				    showWindow->Pos += io->InputMessage.MouseDeltaOffset;
			 }
			 else
			 {
				 io->Has_Bring_front_opera = false;
				 showWindow->Flag.Bring_Front = false;
			 }
		 }
		
			
	 }
	Clamp(showWindow->ScaleX, 0.7f, 3.0f);//限制缩放大小
	Clamp(showWindow->ScaleY, 0.7f, 3.0f);

	 //更新完窗口位置后再设置每一次窗口绘制内容的跟踪位置
	 showWindow->cursorPos = showWindow->Pos;
	 showWindow->cursorPos.x += showWindow->spacing * showWindow->ScaleX* 2.3;//横坐标再隔开点距离
	 showWindow->cursorPos.y += showWindow->TitleRect.Height +showWindow->spacing*showWindow->ScaleY * 1.7; //隔开标题框的宽度 同时在隔开点距离  方便显示菜单栏时继承跟踪位置
	 this->CurrentShowWindowStack.push_back(showWindow);
}

void UI::StartMenu(MenuShowOrderType ShowOrder)
{
	ShowWindowContainer*CurrentShowWindow = this->CurrentShowWindowStack.back();
	CurrentShowWindow->MenuShowOrder = ShowOrder;//当前窗口在菜单栏摆放方式
}

void UI::Menu(const char* MenuName, bool enableMenuItem)
{
	ShowWindowContainer*CurrentShowWindow=this->CurrentShowWindowStack.back();//现在的窗口
	ShowMenu* menu = CurrentShowWindow->FindMenuByName(MenuName);//寻找这个窗口的这个名称对应的菜单栏
	if (menu == nullptr) //如果是第一次执行的就创建这个菜单
	{
		menu = CurrentShowWindow->CreateMenu(MenuName);//那就执行一次创建
	}
	CurrentShowWindow->MenuStack.push_back(menu); //供这个菜单栏的子条目显示访问当前菜单栏的位置和其他信息 EndMenu后弹出
	menu->EnableMenuItem = enableMenuItem;//是否允许子条目在显示
	//设置Menu的位置大小颜色参数  先暂时设成静止不变的 后期再通过现在的Menu宽度 和外部裁剪窗口的宽度进行相互制约
	menu->MenuRect.Width = CalcuTextSize(MenuName,CurrentShowWindow->ScaleX, CurrentShowWindow->ScaleY).x+CurrentShowWindow->spacing* CurrentShowWindow->ScaleX*2;
	menu->MenuRect.Height= CalcuTextSize(MenuName, CurrentShowWindow->ScaleX, CurrentShowWindow->ScaleY).y + CurrentShowWindow->spacing * CurrentShowWindow->ScaleY * 2;
	menu->MenuRect.Min = CurrentShowWindow->cursorPos;
	menu->MenuRect.Max = menu->MenuRect.Min + glm::vec2(menu->MenuRect.Width, menu->MenuRect.Height);
	//绘制menuRect(Menu->Rect.Min，Menu->Rect.Max，Menu->Color
	CurrentShowWindow->content.drawlist.AddRectFill(menu->MenuRect.Min, menu->MenuRect.Max,menu->MenuCol,9,1);
	//设置菜单栏文本显示位置
	glm::vec2 MenuTextPos = glm::vec2(menu->MenuRect.Min.x + CurrentShowWindow->spacing * CurrentShowWindow->ScaleX * 2, menu->MenuRect.Min.y + CurrentShowWindow->spacing * CurrentShowWindow->ScaleY * 2)
		+glm::vec2(0, CalcuTextSize(MenuName, CurrentShowWindow->ScaleX, CurrentShowWindow->ScaleY).y);//需要再空出文本高度
	//绘制menu的clip文本(MenuTextPos, menuRect, CurrentWindow->Color[Text], scaleX, scaleY);
	CurrentShowWindow->content.drawlist.AddClipText(MenuTextPos, io->Colors[TextCol],MenuName, menu->MenuRect, CurrentShowWindow->ScaleX, CurrentShowWindow->ScaleY);
	//耿欣现在窗口在内容跟踪位置
	if(CurrentShowWindow->MenuShowOrder==HorizontalShow)//如果当前窗口的菜单栏摆放方式为水平摆放
		CurrentShowWindow->cursorPos.x += menu->MenuRect.Width + CurrentShowWindow->spacing * CurrentShowWindow->ScaleX* 4.1; //跟踪位置往x轴方向自增距离 方便下一个菜单栏绘制 位置访问参数
	else if(CurrentShowWindow->MenuShowOrder == VerticalShow)
		CurrentShowWindow->cursorPos.y += menu->MenuRect.Height + CurrentShowWindow->spacing * CurrentShowWindow->ScaleY * 5.1; //跟踪位置往y轴方向自增距离
	//如果当前窗口允许响应输入事件
	if (CurrentShowWindow->Flag.AllowResponseInputEvent)
	{
		//是否菜单框吃了鼠标位置
		if (menu->MenuRect.Contain(io->InputMessage.MousePos))//要明白这个菜单栏->Hoverable决定它的悬浮内容是否显示 而这个菜单栏->Hoverable由悬浮窗矩形外的鼠标点击事件和其他菜单栏是否Hoverable来清除 也就只有一个菜单栏Hoverable激活
		{
			//改变这个菜单栏的颜色
			menu->MenuCol = glm::vec4(0.2,0.1,0.2,1);
			menu->Hoverable = true;
		}
		else//说明菜单栏没有吃掉鼠标位置
		{
			//恢复这个菜单栏原来的颜色
			menu->MenuCol = io->Colors[MenuRectCol];
			//现在判断悬浮窗矩形外的鼠标点击事件或其他菜单栏是否Hoverable来清除当前菜单栏->Hoverable
			 
			if (!menu->HoverRect.Contain(io->InputMessage.MousePos) && io->InputMessage.MouseDown[GLFW_MOUSE_BUTTON_LEFT]
				|| IsOtherMenuHoverable(CurrentShowWindow, menu))//主要排除当前Menu查看其他Menu
							menu->Hoverable = false; //清除当前菜单栏悬浮窗显示标志

		}
	}
}

void UI::MenuItem(const char* ItemName, bool* ShowotherContextFlag)//纯只是往菜单栏的子条目数组添加
{
	//现在的窗口
	ShowWindowContainer*CurrentShowWindow = this->CurrentShowWindowStack.back();
	//现在的窗口的现在的菜单 访问完后需要弹出这个菜单
	ShowMenu* CurrentMenu = CurrentShowWindow->MenuStack.back();
	ShowMenuItem Item;
	Item.name = ItemName;
	Item.ShowOtherContext = ShowotherContextFlag;
	Item.ItemRect=ClipRect(glm::vec2(0,0),glm::vec2(0,0));
	CurrentMenu->MenuItemStack.push_back(Item);//每次放进去后最终结束需要弹出来

}

void UI::EndMenu()
{
	//现在的窗口
	ShowWindowContainer* CurrentShowWindow = this->CurrentShowWindowStack.back();
	//现在的窗口的现在的菜单 访问完后需要弹出这个菜单
	ShowMenu* CurrentMenu = CurrentShowWindow->MenuStack.back();
	if (CurrentMenu->EnableMenuItem)//是否当前菜单允许子条目开启显示
	{

		if (CurrentMenu->Hoverable)//如果当前菜单允许悬浮窗显示
		{
			float HoverRectWidth = 0; //悬浮窗的总宽度 通常取子条目文本宽度的最大值再加上一些间隔
			float HoverRectHeight = 0; //悬浮窗的总宽度 累加每一给文本的有效高度再加上一些间隔
			for (ShowMenuItem Item : CurrentMenu->MenuItemStack)//遍历所有子条目的宽高计算出总的悬浮窗的有效高宽
			{
				glm::vec2 TextSize = CalcuTextSize(Item.name, CurrentShowWindow->ScaleX, CurrentShowWindow->ScaleY);
				HoverRectWidth = std::max(HoverRectWidth, TextSize.x + CurrentShowWindow->spacing * CurrentShowWindow->ScaleX * 7.8f);
				HoverRectHeight += TextSize.y + CurrentShowWindow->spacing * CurrentShowWindow->ScaleY * 7.8;
				
			}
			//设置菜单栏的悬浮窗的位置距离参数
			CurrentMenu->HoverRect.Width = HoverRectWidth+ CurrentMenu->ScrollBarWH.x;//在原先的基础上加上滚动条的宽度
			CurrentMenu->HoverRect.Height = HoverRectHeight;
			CurrentMenu->HoverRect.Min = glm::vec2(CurrentMenu->MenuRect.Max.x - CurrentShowWindow->spacing * CurrentShowWindow->ScaleX * 2, CurrentMenu->MenuRect.Max.y);
			CurrentMenu->HoverRect.Max = CurrentMenu->HoverRect.Min + glm::vec2(CurrentMenu->HoverRect.Width, CurrentMenu->HoverRect.Height);
			//绘制菜单栏的悬浮窗
			CurrentShowWindow->content.drawlist.AddRectFill(CurrentMenu->HoverRect.Min, CurrentMenu->HoverRect.Max, io->Colors[HoverRectCol], 9, 1);
			//设置滚动条的参数
			CurrentMenu->ItemScrollbar.Width = CurrentMenu->ScrollBarWH.x* CurrentShowWindow->ScaleX;
			CurrentMenu->ItemScrollbar.Height=CurrentMenu->ScrollBarWH.y * CurrentShowWindow->ScaleY;
			CurrentMenu->ItemScrollbar.Min = glm::vec2(CurrentMenu->HoverRect.Max.x- CurrentMenu->ScrollBarWH.x, CurrentMenu->HoverRect.Min.y+CurrentShowWindow->spacing)
				+glm::vec2(0, -CurrentMenu->MenuItemOffset.y);
			CurrentMenu->ItemScrollbar.Max = CurrentMenu->ItemScrollbar.Min + CurrentMenu->ScrollBarWH;
			//再绘制滚动条
			CurrentShowWindow->content.drawlist.AddRectFill(CurrentMenu->ItemScrollbar.Min, CurrentMenu->ItemScrollbar.Max, CurrentMenu->ScrollbarCol,5,1);
			

			//设置悬浮窗的文本框位置
			glm::vec2 HoverTextRectPos = CurrentMenu->HoverRect.Min +
				glm::vec2(CurrentShowWindow->spacing * CurrentShowWindow->ScaleX * 1.2, CurrentShowWindow->spacing * CurrentShowWindow->ScaleX * 1.2 + CurrentMenu->MenuItemOffset.y); //隔开点距离 然后加上由滚动条控制的偏移delta位移
			//响应滚轮移动事件 使位移变化(保证鼠标在悬浮窗口内部成立) 同时绘制滚动条 但是要限制MenuOffset的值
			Clamp(CurrentMenu->MenuItemOffset.y, CurrentMenu->ItemScrollbar.Height-CurrentMenu->HoverRect.Height  ,0 );
			if(CurrentMenu->HoverRect.Contain(io->InputMessage.MousePos))//响应鼠标滚轮滚动偏移
			  CurrentMenu->MenuItemOffset.y += io->InputMessage.Scroll;
			//再判断鼠标是否悬停在滚动条 并且鼠标点击了 通过滚动条调整菜单栏的子条目 偏移
			if (CurrentMenu->ItemScrollbar.Contain(io->InputMessage.MousePos))
			{
				CurrentMenu->ScrollbarCol = glm::vec4(1,0.4,0,1);
				if (io->InputMessage.MouseDown[GLFW_MOUSE_BUTTON_LEFT])
				{
					CurrentMenu->MenuItemOffset.y -= io->InputMessage.MouseDeltaOffset.y;
					//同时清除当前窗口可移动的标志允许判断
					CurrentShowWindow->Flag.Movable = false;
				}
				
			}
			else {
				CurrentShowWindow->Flag.Movable = true;
				CurrentMenu->ScrollbarCol = io->Colors[ScrollBarCol];
			}
				
			for (ShowMenuItem& Item : CurrentMenu->MenuItemStack)//遍历本个菜单栏的所有子条目
			{
				//设置单个子条目的文本框位置 宽高参数
				Item.ItemRect.Width= CalcuTextSize(Item.name, CurrentShowWindow->ScaleX, CurrentShowWindow->ScaleY).x + CurrentShowWindow->spacing * CurrentShowWindow->ScaleX * 4.7;
				Item.ItemRect.Height = CalcuTextSize(Item.name, CurrentShowWindow->ScaleX, CurrentShowWindow->ScaleY).y + CurrentShowWindow->spacing * CurrentShowWindow->ScaleY * 4.7;
				Item.ItemRect.Min = HoverTextRectPos;
				Item.ItemRect.Max = HoverTextRectPos + glm::vec2(Item.ItemRect.Width, Item.ItemRect.Height);

				//所以要在这判断子条目的文本框是否越出悬浮窗的边界 裁剪掉 改变单个子条目文本框参数
				if (Item.ItemRect.Min.y> CurrentMenu->HoverRect.Min.y&& Item.ItemRect.Max.y< CurrentMenu->HoverRect.Max.y)//如果当前悬浮窗单个文本框吃了鼠标位置 且要保证这个单个子条目框在整个悬浮窗内部
				{
					glm::vec4 HoverTextRextCol = io->Colors[HoverTextRectCol];//悬浮窗单个文本框颜色
					if (Item.ItemRect.Contain(io->InputMessage.MousePos))//当鼠标位置在单个文本框悬浮
					{
						HoverTextRextCol = glm::vec4(0.5,0.6,0,1);//文本框 鼠标悬浮之上的高亮颜色
						if (io->InputMessage.MouseDown[GLFW_MOUSE_BUTTON_LEFT]) {//在悬浮基础之上鼠标点击一下
							if (Item.ShowOtherContext != nullptr)
								*Item.ShowOtherContext = true;//激活显示其他内容的flag
						}	
					}
					// 绘制菜单栏子条目文本框
					CurrentShowWindow->content.drawlist.AddRectFill(Item.ItemRect.Min, Item.ItemRect.Max, HoverTextRextCol, 9, 1);
					//设置悬浮窗单个文本位置   文本框隔出点距离再空出文本高度即为文本开始显示的位置
					glm::vec2 HoverTextPos = HoverTextRectPos + glm::vec2(0, CalcuTextSize(Item.name, CurrentShowWindow->ScaleX, CurrentShowWindow->ScaleY).y)
						+ glm::vec2(CurrentShowWindow->spacing * 1.3, CurrentShowWindow->spacing * 1.3);
					//绘制菜单栏子条目文本
					CurrentShowWindow->content.drawlist.AddClipText(HoverTextPos, io->Colors[TextCol], Item.name, CurrentMenu->HoverRect, CurrentShowWindow->ScaleX, CurrentShowWindow->ScaleY);
				}
				
				HoverTextRectPos.y += CurrentShowWindow->spacing * 2 + Item.ItemRect.Height;//纵坐标往下平移
			}


		}
		else//如果不让显示悬浮窗就置零滚菜单栏子条目动偏移
			CurrentMenu->MenuItemOffset = glm::vec2(0,0);
		  
	}
	//处理完所有逻辑和绘制命令后 清理循环更新的内存
	CurrentMenu->MenuItemStack.clear();
	CurrentShowWindow->MenuStack.pop_back();
}
void UI::EndShowWindow()
{
	ShowWindowContainer* CurrentShowWindow = this->CurrentShowWindowStack.back();
	CurrentShowWindow->Flag.AllowResponseInputEvent = false;//使用完了清空标志
	this->CurrentShowWindowStack.pop_back();
}
ShowWindowContainer* UI::CreateNewShowWindow(const char* name)
{
	ShowWindowContainer* newShowWindow = new ShowWindowContainer();//初始化现在新生成的窗口的参数
	
	this->NameToShowWindowMap.insert(name, newShowWindow);
	if (this->LastShowWindow == nullptr)//如果之前没有窗口它为第一个窗口直接取中间位置显示
		newShowWindow->Pos = glm::vec2(io->drawdata.DisplaySize.x / 2, io->drawdata.DisplaySize.y / 2);
	else//如果之前已经有窗口就隔点距离
		newShowWindow->Pos = this->LastShowWindow->Pos + glm::vec2(30, 60);
	this->LastShowWindow = newShowWindow;//需要下一次窗口访问上一个窗口时使用
	
	io->drawdata.DrawDataLayerBuffer.push_back(newShowWindow);//只有在第一次创建时才操作 是在窗口顶点资源发送时才进行访问 后面就不push_back了 一直访问或者进行修改
	io->CreateShowWindow_glBuffer(newShowWindow);
	return newShowWindow;
}

ShowWindowContainer* UI::FindShowWindowByName(const char* name)
{
	if (this->NameToShowWindowMap.find(name))//如果能从树找到键值
		return this->NameToShowWindowMap[name];
	return nullptr;
}

void UI::BgText(const char* Text)
{
	ShowWindowContainer* CurrentShowWindow = this->CurrentShowWindowStack.back();
	CurrentShowWindow->content.drawlist.AddClipText(CurrentShowWindow->cursorPos+glm::vec2(3*CurrentShowWindow->spacing, 
		3*CurrentShowWindow->spacing), io->Colors[TextCol], Text, CurrentShowWindow->OutterRect, CurrentShowWindow->ScaleX, CurrentShowWindow->ScaleX);
}
ShowMenu* ShowWindowContainer::CreateMenu(const char* name)
{
	
	ShowMenu* newShowMenu = new ShowMenu();
	this->MenuMap.insert(name,newShowMenu);//只执行一次
	this->MenuArray.push_back(newShowMenu);//只执行一次放入内容
	return newShowMenu;
}

ShowMenu* ShowWindowContainer::FindMenuByName(const char* name)
{
	if(this->MenuMap.find(name))
		return this->MenuMap[name];
	return nullptr;
}
