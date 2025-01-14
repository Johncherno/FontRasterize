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

		// 检查字符是否在有效范围内
		if (C >= io->atlas.glyphfonts->IndexSearchTable.size()) {
			continue; // 无效字符，跳过
		}
		unsigned int glyphid = 0;
		if (glyphid >= io->atlas.glyphfonts->Glyphs.size()) {
			continue; // 字形ID无效，跳过
		}
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
		// 检查字符是否在有效范围内
		if (C >= io->atlas.glyphfonts->IndexSearchTable.size()) {
			continue; // 无效字符，跳过
		}
		unsigned int glyphid = 0;
		if (glyphid >= io->atlas.glyphfonts->Glyphs.size()) {
			continue; // 字形ID无效，跳过
		}
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

		// 检查字符是否在有效范围内
		if (C >= io->atlas.glyphfonts->IndexSearchTable.size()) {
			continue; // 无效字符，跳过
		}
		unsigned int glyphid = 0;
		if (glyphid >= io->atlas.glyphfonts->Glyphs.size()) {
			continue; // 字形ID无效，跳过
		}
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

		/*	std::cout << "clipRect.Min.x=" << clipRect.Min.x << std::endl;
			std::cout << "clipRect.Max.x="<< clipRect.Max.x << std::endl;
			std::cout << "x0="<<x0 << std::endl;
			std::cout << "x1=" << x1 << std::endl;
			std::cout << " " << std::endl;*/
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
	//建立drawlist函数表格查
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
		if (!DrawDataLayer->Flag.IsWindowVisible)//如果这个窗口不允许显示就不执行下面操作
		{
			DrawDataLayer->content.drawlist.Clear();//同时你这个窗口资源不发送显示 直接清除掉
			continue;
		}
			
		DrawList* cmdDrawlist = &DrawDataLayer->content.drawlist;//获取的是每一个允许显示窗口的绘制内容
		DynamicArray<DrawVert>& vtxBuffer = cmdDrawlist->_VtxBuffer;
		DynamicArray<unsigned int>& idxBuffer = cmdDrawlist->_IdxBuffer;
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


	//设置窗口的外部裁剪矩形 同时也能保证更新外部裁剪矩形参数
	showWindow->OutterRect.Min = showWindow->Pos;
	showWindow->OutterRect.Max = showWindow->Pos +glm::vec2(460, 490);
	showWindow->content.drawlist.AddRectFill(showWindow->OutterRect.Min, showWindow->OutterRect.Max, showWindow->TitleColor, 9.0f, 1);
	showWindow->content.drawlist.AddRectBorder(showWindow->OutterRect.Min, showWindow->OutterRect.Max,glm::vec4(1,1,0,1),2.0f);
	//showWindow->drawlist.AddRectTriangleBorder(showWindow->OutterRect.Min, showWindow->OutterRect.Max, glm::vec4(1, 1, 0, 1), 1.3f);
	showWindow->content.drawlist.AddClipText(showWindow->Pos + glm::vec2(0, 53), glm::vec4(0, 1,1, 1), Text, showWindow->OutterRect,showWindow->ScaleX, showWindow->ScaleY);//绘制标题

	if (showWindow->OutterRect.Contain(io->InputMessage.MousePos) && showWindow->Flag.AllowResponseInputEvent)//是否当前窗口的Rect吃了鼠标位置 并且允许响应事件 
	{
		showWindow->TitleColor = glm::vec4(0, 0.2, 0.2,1);
		if (io->InputMessage.KeyMesage.key == GLFW_KEY_LEFT_CONTROL && io->InputMessage.KeyMesage.press)
		{
			showWindow->ScaleX += io->InputMessage.Scroll * 0.05f; // 滚动的增量控制大小的变化比例
			showWindow->ScaleY += io->InputMessage.Scroll * 0.05f;
		}
		if (io->InputMessage.KeyMesage.key == GLFW_KEY_T && io->InputMessage.KeyMesage.press)//如果T按键按下绘制字体文本的顶点
		{
			
			showWindow->content.drawlist.AddText_VertTriangleBorder(showWindow->Pos + glm::vec2(0, 53), glm::vec4(1, 0, 1, 1), Text, showWindow->ScaleX, showWindow->ScaleY,1.1);
		}

		if (io->InputMessage.MouseDown[GLFW_MOUSE_BUTTON_LEFT])//如果对当前窗口有点击执行置顶 获取鼠标位移进行窗口位置移动
		{
			io->Has_Bring_front_opera = true;
			showWindow->Flag.Bring_Front = true;
			showWindow->Pos += io->InputMessage.MouseDeltaOffset;
		}
		else
		{
			io->Has_Bring_front_opera = false;
			showWindow->Flag.Bring_Front = false;
		}

	}
	else
	{
		showWindow->TitleColor = glm::vec4(0,0.2, 0,1);
	}
	
	if (showWindow->ScaleX < 0.1f)showWindow->ScaleX = 0.1f;
	if (showWindow->ScaleY > 3.0f)showWindow->ScaleY = 3.0f;

	if (showWindow->ScaleX < 0.1f) showWindow->ScaleX = 0.1f;
	if (showWindow->ScaleY > 3.0f) showWindow->ScaleY = 3.0f;
	this->CurrentShowWindowStack.push_back(showWindow);//这个是要在绘制当前窗口的子条目文本时需要用到当前窗口的主要位置 需要后期给弹出去
	
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

