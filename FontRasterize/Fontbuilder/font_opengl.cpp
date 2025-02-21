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
//io��������˿�
BuiltIO* io = nullptr;//����Ϣ�ķ��ʿ�
static bool IsOtherMenuHoverable(ShowWindowContainer*ShowWin,ShowMenu*showmenu)
{
	for (ShowMenu*tmpMenu:ShowWin->MenuArray)
	{
		if (tmpMenu!=showmenu)
		{
			if (tmpMenu->MenuRect.Contain(io->InputMessage.MousePos))//ֻҪ������˵�֮��Ĳ˵���������Ӧ�ͻ��ϱ��˵�������
				return true;	
		}
	}
	return false;
}
static void Clamp(float&Value,float Min,float Max)//���ƺ�������
{
	if (Value > Max)
		Value = Max;
	else if (Value < Min)
		Value = Min;
	//���������ʲô������
}

static DynamicArray<unsigned int> TextChar_utf_8_ToUnicode(const char* utf8_str)
{
	DynamicArray<unsigned int>  unicode_points;
	const unsigned int SecondByteValidBitCount = 6, thirdByteValidBitCount = 6;
	for (unsigned int i = 0; utf8_str[i] != '\0';)
	{
		unsigned char C = utf8_str[i];//��ȡ����UTF-8�ַ�
		unsigned int unicode = 0x00;
		if (C < 0x80)//C<0x80��Χ�� һ���ֽ�  �ַ�Ϊ���ֽڵ�ascll��
		{
			unicode = C;
			unicode_points.push_back(unicode);
			i++;
		}
		else if (C < 0xE0)//0x80<C<0xE0 ʹ�� 2 ���ֽڽ��б��� ��ȡ��һ���ֽڵĺ���λ �͵ڶ����ֽڵĺ���λ ���Ե�һ���ֽ���Ҫ�ƶ��ڶ����ֽڵ���Чλ�� �ϲ�Ϊһ������
		{

			if (utf8_str[i + 1] != '\0')//����ڶ����ֽڲ�Ϊ��
			{
				unicode |= (utf8_str[i + 1] & 0x3F);//��ȡ�ڶ����ֽڵĺ���λ��Ч��λ
				unicode |= ((C & 0x1F) << SecondByteValidBitCount);//��ȡ��һ���ֽڵĺ���λ��Ч��λ �����ջ�����ϲ��γ�һ�����ֽڵ�����
				unicode_points.push_back(unicode);
				i += 2;
			}
			else
				std::cerr << "��Ч UTF-8 ����" << std::endl;
		}
		else if (C < 0xF0)//C<0xF0 ʹ�� 3 ���ֽڽ��б���   ��ȡ��һ���ֽڵĺ�4λ �͵ڶ����ֽڵĺ���λ �������ֽڵĺ���λ ���Ե�һ���ֽ���Ҫ�ƶ��ڶ����ֽں͵������ֽڵ�λ�� ...�ϲ�Ϊһ������
		{
			if (utf8_str[i + 2] != '\0')//����������ֽڲ�Ϊ��
			{

				unicode |= (utf8_str[i + 2] & 0x3F);//��ȡ�������ֽڵĺ���λ
				unicode |= ((utf8_str[i + 1] & 0x3F) << thirdByteValidBitCount);//��ȡ�ڶ����ֽڵĺ���λ
				unicode |= ((C & 0x0F) << (SecondByteValidBitCount + thirdByteValidBitCount));//��ȡ��һ���ֽڵĺ�4λ��Ч��λ �����ջ�����ϲ��γ�һ�����ֽڵ�����
				unicode_points.push_back(unicode);
				i += 3;
			}
			else
				std::cerr << "��Ч UTF-8 ����" << std::endl;
		}
	}
	return unicode_points;
}
//���㵥���ı��Ŀ��
static glm::vec2 CalcuTextSize(const char*Text,float scalex,float scaley)
{
	//��ȡio�����巽���X0 X1 Y0 Y1 �����ַ������� �ַ�����߶� �ַ���������Advance ͨ���ۼӣ�ÿ���ַ��Ŀ�� �������룩=���ı���� ȡÿ���ַ��ĸ߶ȵ����ֵ�����ı��ĸ߶�
	glm::vec2 textSize = glm::vec2(0,0);
	DynamicArray<unsigned int> str = TextChar_utf_8_ToUnicode(Text);//��utf-8תΪunicode����
	for (unsigned int i=0;i<str.size();i++)
	{
		unsigned int C = static_cast<unsigned int>(str[i]);
		unsigned int glyphid = 0;
		if (!io->atlas.glyphfonts->FindGlyph(C, glyphid))//Ѱ���ַ���������
			continue;
		FontGlyph glyph = io->atlas.glyphfonts->Glyphs[glyphid];
		textSize.y = std::max(textSize.y,fabs(scaley*(glyph.Y1- glyph.Y0)));
		textSize.x += scalex * glyph.AdvanceX;
	}
	return  textSize;
}
//Բ����
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
	if (radius * scale < 0.5f) // ������ź�İ뾶̫С��ֱ�ӷ������ĵ�
	{
		_ArcPath.push_back(center);
		return;
	}

	unsigned int step = 1; // ÿ�β����Ĳ���
	unsigned int Size = (MaxSample - MinSample) / step; // Ҫ���ɵĶ�����
	unsigned int oldSize = _ArcPath.size();
	_ArcPath.resize(oldSize + Size); // Ԥ����洢�ռ�

	glm::vec2* outptr = _ArcPath.Data + oldSize;
	for (unsigned int sampleIndex = MinSample; sampleIndex < MaxSample; sampleIndex += step)
	{
		unsigned int actualIndex = true ? sampleIndex : (MaxSample - (sampleIndex - MinSample) - 1);
		glm::vec2 s = io->ArcFastLookUpTable[actualIndex]; // ʹ�ò��ұ��ȡ���Ǻ���ֵ
		outptr->x = center.x + s.x * radius * scale; // o.x + r * cos(A) * scale
		outptr->y = center.y + s.y * radius * scale; // o.y + r * sin(A) * scale
		outptr++;
	}
}
void DrawList::PolyConvexFill(glm::vec4 color)
{
	unsigned int pointsCount = _ArcPath.size();
	if (pointsCount < 3) return; // ���������С�� 3���޷����ɶ����

	unsigned int faceCount = pointsCount - 2; // �����ε�����
	unsigned int VtxCount = pointsCount, IdxCount = faceCount * 3;
	ReserveForBuffer(VtxCount, IdxCount); // Ԥ�����������������
	glm::vec2 whiteTexCoord = glm::vec2(io->atlas.WhiteU1, io->atlas.WhiteV1);
	// ��䶥�㻺����
	for (unsigned int i = 0; i < VtxCount; i++)
	{
		VtxWritePtr->pos = _ArcPath[i];
		VtxWritePtr->Tex = whiteTexCoord; // �������꣺��ɫɫ��
		VtxWritePtr->Col = color;
		VtxWritePtr++;
	}

	// ������������������������Σ�
	for (unsigned int j = 2; j < VtxCount; j++)
	{
		_IdxWritePtr[0] = VtxCurrentIdx;       // �����εĵ�һ������
		_IdxWritePtr[1] = VtxCurrentIdx + j - 1; // �ڶ�������
		_IdxWritePtr[2] = VtxCurrentIdx + j;     // ����������
		_IdxWritePtr += 3;
	}
	VtxCurrentIdx += VtxCount; // ���µ�ǰ��������

}
void DrawList::AddLine(glm::vec2 p1, glm::vec2 p2, glm::vec4 color, float thickness)
{

	unsigned int VtxCount = 4 , IdxCount = 6 ;
	ReserveForBuffer(VtxCount, IdxCount);

	// ���������ķ�������
	glm::vec2 dir = p2 - p1;
	glm::vec2 perp = glm::vec2(-dir.y, dir.x); // ��ֱ����
	float length = glm::length(dir);
	if (length == 0.0f) return; // ���������
	glm::vec2 whiteTexCoord = glm::vec2(io->atlas.WhiteU1, io->atlas.WhiteV1);
	// ��һ�����������ʹ�ֱ����
	dir /= length;
	perp /= length;

	// �����������ĸ�����
	glm::vec2 p1a = p1 + perp * thickness * 0.5f;
	glm::vec2 p1b = p1 - perp * thickness * 0.5f;
	glm::vec2 p2a = p2 + perp * thickness * 0.5f;
	glm::vec2 p2b = p2 - perp * thickness * 0.5f;

	// ��Ӷ��㵽���㻺����
	VtxWritePtr[0].pos = p1a;VtxWritePtr[0].Tex=whiteTexCoord; VtxWritePtr[0].Col = color;
	VtxWritePtr[1].pos = p1b;VtxWritePtr[1].Tex=whiteTexCoord; VtxWritePtr[1].Col = color;
	VtxWritePtr[2].pos = p2a;VtxWritePtr[2].Tex=whiteTexCoord; VtxWritePtr[2].Col = color;
	VtxWritePtr[3].pos = p2b;VtxWritePtr[3].Tex=whiteTexCoord; VtxWritePtr[3].Col = color;

	// �������������������
	_IdxWritePtr[0] = VtxCurrentIdx;
	_IdxWritePtr[1] = VtxCurrentIdx + 1;
	_IdxWritePtr[2] = VtxCurrentIdx + 2;
	_IdxWritePtr[3] = VtxCurrentIdx + 1;
	_IdxWritePtr[4] = VtxCurrentIdx + 2;
	_IdxWritePtr[5] = VtxCurrentIdx + 3;

	// ����ָ�������
	VtxWritePtr += 4;
	VtxCurrentIdx += 4;
	_IdxWritePtr += 6;
}
void DrawList::AddTriangle(glm::vec2 p1, glm::vec2 p2, glm::vec2 p3, glm::vec4 color, float thickness)
{
	// ����������
	AddLine(p1, p2, color, thickness); // ��һ����
	AddLine(p2, p3, color, thickness); // �ڶ�����
	AddLine(p3, p1, color, thickness); // ��������
}
void DrawList::AddRectTriangleBorder(glm::vec2 Min, glm::vec2 Max, glm::vec4 color, float thickness)
{
	// ������ε��ĸ�����
	glm::vec2 p1 = Min;                // ���½�
	glm::vec2 p2 = glm::vec2(Max.x, Min.y); // ���½�
	glm::vec2 p3 = Max;                // ���Ͻ�
	glm::vec2 p4 = glm::vec2(Min.x, Max.y); // ���Ͻ�

	// �����������������εı߿�
	AddTriangle(p1, p2, p3, color, thickness); // ��һ��������
	AddTriangle(p3, p4, p1, color, thickness); // �ڶ���������
}
void DrawList::AddRectBorder(glm::vec2 Min, glm::vec2 Max, glm::vec4 color, float thickness)
{
	// ����������
	AddLine(Min, glm::vec2(Max.x, Min.y), color, thickness); // �ϱ�
	AddLine(glm::vec2(Max.x, Min.y), Max, color, thickness); // �ұ�
	AddLine(Max, glm::vec2(Min.x, Max.y), color, thickness); // �±�
	AddLine(glm::vec2(Min.x, Max.y), Min, color, thickness); // ���
}
//void DrawList::AddTime(glm::vec2 Position, glm::vec3 Color, float scalex, float scaley)
//{
//	// ��ȡ��ǰʱ��
//	auto now = std::chrono::system_clock::now();
//	auto in_time_t = std::chrono::system_clock::to_time_t(now);
//
//	// ��ʱ���ʽ��Ϊ���㼸�ֵ��ַ���
//	std::stringstream ss;
//	std::tm timeinfo;
//	localtime_s(&timeinfo, &in_time_t);
//	ss << std::put_time(&timeinfo, "%Y/%m/%d %H:%M:%S");
//	std::string timeString = ss.str();
//
//	// ���� AddText ����������ʱ���ַ���
//	AddText(Position, Color, timeString.c_str(), scalex, scaley);
//}
void DrawList::AddRectFill(glm::vec2 Min, glm::vec2 Max, glm::vec4 color, float rounding, float scale)
{
	_ArcPath.clear(); // ���֮ǰ��·��

	// ������ε����ĵ�
	glm::vec2 center = (Min + Max) * 0.5f;

	// ����ʱ�����ĵ�Ϊ��׼���� Min �� Max
	glm::vec2 scaledMin = Min;
	glm::vec2 scaledMax = center + (Max - center) * scale;//����е��������
	float scaledRounding = rounding * scale;

	// ����ĸ�Բ��·��
	_PathArc(glm::vec2(scaledMin.x + scaledRounding, scaledMin.y + scaledRounding), scaledRounding, 24, 36, scale); // ���½�
	_PathArc(glm::vec2(scaledMax.x - scaledRounding, scaledMin.y + scaledRounding), scaledRounding, 36, 48, scale); // ���½�
	_PathArc(glm::vec2(scaledMax.x - scaledRounding, scaledMax.y - scaledRounding), scaledRounding, 0, 12, scale);  // ���Ͻ�
	_PathArc(glm::vec2(scaledMin.x + scaledRounding, scaledMax.y - scaledRounding), scaledRounding, 12, 24, scale); // ���Ͻ�

	// �������
	PolyConvexFill(color);

}
void DrawList::AddText_VertTriangleBorder(glm::vec2 TextPosition, glm::vec4 LineColor, const char* Text, float scalex, float scaley, float thickness)
{
	const float lineHeight = io->atlas.fontSize * scaley;
	glm::vec2 Pos = TextPosition;
	DynamicArray<unsigned int> str = TextChar_utf_8_ToUnicode(Text);//��utf-8תΪunicode����
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
		if (!io->atlas.glyphfonts->FindGlyph(C, glyphid))//Ѱ���ַ���������
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
	DynamicArray<unsigned int> str = TextChar_utf_8_ToUnicode(Text);//��utf-8תΪunicode����
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
		if (!io->atlas.glyphfonts->FindGlyph(C, glyphid))//Ѱ���ַ���������
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
//	// ������ת��Ϊ�ַ���
//	std::ostringstream numberStream;
//	numberStream << std::fixed << std::setprecision(Precision) << Number; // ʹ�ù̶�������ʾ�������þ���
//	std::string numberString = numberStream.str();
//	// ���� AddText ��ʾ�ַ���
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
//	std::vector<unsigned int> str=TextChar_utf_8_ToUnicode(Text);//��utf-8תΪunicode����
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
//		if (!atlas->glyphfonts->FindGlyph(C, glyphid))//Ѱ���ַ���������
//		{
//			std::cerr << "���ַ����ڴ��ַ�����¼��Χ��" << std::endl;
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
//		float angle = sin(time + index * 0.5f) * 0.3f; // ʹ�����ַ���λ��λ����ת
//		float waveOffset = sin(time + index * 0.3f) * 0.08; // ʹ�����ַ���λ���� ��λ��
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
	DynamicArray<unsigned int> str = TextChar_utf_8_ToUnicode(Text);//��utf-8תΪunicode����
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
		if (!io->atlas.glyphfonts->FindGlyph(C, glyphid))//Ѱ���ַ���������
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
			//std::cout << "��������" << std::endl;
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
				Pos.x += glyph.AdvanceX * scalex;//�൱�����������������clip����Ͳ�ִ������
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
	if (this->atlas.FontConfigData.size() == 0)//���û�м��������ļ� �ͼ���Ĭ������
	{
		this->atlas.Addfont_internal();//�������嶥����Ϣ���һ����ַ�ͼ��
	}
	this->atlas.BuildFont();
	outglTexInfo TexInfo = atlas.GetTexAlpha32();//�������ͼ����Ϣ
	GL_CHECK(glGenTextures(1, &this->TexId));
	GL_CHECK(glBindTexture(GL_TEXTURE_2D, this->TexId));//����

	GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
	GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));

	GL_CHECK(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, TexInfo.width, TexInfo.height, 0, GL_RGBA,
		GL_UNSIGNED_BYTE, TexInfo.Texpixel));

	//�ȳ�ʼ���������ݵ���ʾ�ӿ�
	this->drawdata.DisplayPos = glm::vec2(0, 0);
	this->drawdata.DisplaySize = glm::vec2(900, 800);//w h

	//������ɫ��
	shader = new shaderProgram;
	//�������ǵ���ɫ��

	if (!shader->loadShaders("shader\\FontShader.vert", "shader\\FontShader.frag")) {
		std::cerr << "Error: Shader failed to load." << std::endl;
		return; // ������ֹ����
	}
	//����drawlist���ǽǶȺ�������
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

	if (this->drawdata.DrawDataLayerBuffer.back()->Flag.Bring_Front)//����㱾����ö�������Ҫ���ö����� �ǾͲ�Ҫ���Ǹ��������������ڴ��Ƿ��ĸ����ö�����
		return;
	int index = -1;
	//�����ʾ˳��Խ���¾�ѭ��������Խ�� �෴��Խ�� ����Ҫ���ŷ��� <O(n)
	for (int i = this->drawdata.DrawDataLayerBuffer.size() - 2; i >= 0; i--)
	{
		if (this->drawdata.DrawDataLayerBuffer[i]->Flag.Bring_Front)//����������ͼ�����ö�����
		{
			Has_Bring_front_Window = this->drawdata.DrawDataLayerBuffer[i];
			index = i;
			break;
		}
	}
	if (index != -1)
	{
		// ʹ�� memmove ��Ԫ����ǰ�ƶ�һλ
		memmove(&this->drawdata.DrawDataLayerBuffer[index], &this->drawdata.DrawDataLayerBuffer[index + 1], (this->drawdata.DrawDataLayerBuffer.size() - index - 1) * sizeof(ShowWindowContainer*));
		this->drawdata.DrawDataLayerBuffer.back() = Has_Bring_front_Window;//���һ��Ԫ�ش���ö����ƵĴ��ڻ�����Դ
	}
}
void BuiltIO::AllowDrawDataLayerResponseEvent()
{
	bool found = false;  // ��־λ����¼�Ƿ��Ѿ��ҵ��������λ�õ������ö����ƴ���
	for (int i = this->drawdata.DrawDataLayerBuffer.size() - 1; i >= 0 && !found; i--)//���ö�˳����� ����ö����ȵĳ������λ�þ���Ӧ ʣ�µľͲ�������ж�flag ���Ƿ�ֹ���ӵĴ���ͬʱ��Ӧ����Ƿ��ڴ��ڵĲü����ε��ж��¼�
	{
		if (this->drawdata.DrawDataLayerBuffer[i]->OutterRect.Contain(this->InputMessage.MousePos)) {
			this->drawdata.DrawDataLayerBuffer[i]->Flag.AllowResponseInputEvent = true;//���ö�˳�����ȵ��������������Ӧ
			found = true;  // ���ñ�־λ
		}
		else {
			this->drawdata.DrawDataLayerBuffer[i]->Flag.AllowResponseInputEvent = false;
		}
	}//ʱ�临�Ӷ�<O(n)


}
void BuiltIO::UpdateDrawDataBuffer(UI* DrawUI)
{
	if (this->Has_Bring_front_opera)//���ö�����
	{
		this->Bring_TopDrawDataLayer_ToFrontDisplay();//��UI����Ĵ��ڽ����ö�����
	}
	this->AllowDrawDataLayerResponseEvent();//�Ե��Ӵ���ͼ���������Ӧ �ö�˳�����ȵ�����ȡ��������Ӧ����
	
}
void BuiltIO::RenderDrawList()
{
	
	GL_CHECK(glEnable(GL_BLEND));
	GL_CHECK(glBlendEquation(GL_FUNC_ADD));
	GL_CHECK(glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA));
	GL_CHECK(glActiveTexture(GL_TEXTURE0));
	GL_CHECK(glBindTexture(GL_TEXTURE_2D, this->TexId));

	shader->use();
	//����ͶӰ
	float R = this->drawdata.DisplayPos.x + this->drawdata.DisplaySize.x;
	float L = this->drawdata.DisplayPos.x;
	float T = this->drawdata.DisplayPos.y;
	float B = this->drawdata.DisplayPos.y + this->drawdata.DisplaySize.y;
	orthoProjection = glm::ortho(L, R, B, T, -1.0f, 1.0f);
	shader->setUniform("ProjectM", orthoProjection);
	for (unsigned int i = 0; i < this->drawdata.DrawDataLayerBuffer.size(); i++)
	{
		ShowWindowContainer*DrawDataLayer = this->drawdata.DrawDataLayerBuffer[i];
		//std::cout << "���ƴ��ڵĶ������" << DrawDataLayer->content.drawlist._VtxBuffer.size() << std::endl;
		if (!DrawDataLayer->Flag.IsWindowVisible)//���������ڲ�������ʾ�Ͳ�ִ�����淢�͸�gpu���㻺�����
		{
			DrawDataLayer->content.drawlist.Clear();//ͬʱ��������ڶ��������Դ��������ʾ ֱ�������
			continue;
		}
			
		DrawList* cmdDrawlist = &DrawDataLayer->content.drawlist;//��ȡ����ÿһ��������ʾ���ڵĻ�������
		DynamicArray<DrawVert> vtxBuffer = cmdDrawlist->_VtxBuffer;
		DynamicArray<unsigned int> idxBuffer = cmdDrawlist->_IdxBuffer;
		// ���ö����������Ͱ󶨻�����
		SetUpBufferState(DrawDataLayer->content.Vao, DrawDataLayer->content.Vbo, DrawDataLayer->content.Ebo);
		// ���㶥�㻺������С
	   //��֮ǰ�洢�Ķ��㻺���С�͵�ǰ�����ı��Ķ��㻺���С��һ��ʱ�����ط����ٽ��и���
		if (DrawDataLayer->content.VertexBufferSize != vtxBuffer.size())
		{
			// ���һ���������������ӣ����·��������������ռ䲢��������
			// ��������������������٣����·��仺�����ռ�Ϊ��ǰʵ����Ҫ�Ĵ�С
			DrawDataLayer->content.VertexBufferSize = vtxBuffer.size();

			GL_CHECK(glBufferData(GL_ARRAY_BUFFER, sizeof(DrawVert) * vtxBuffer.size(), nullptr, GL_DYNAMIC_DRAW));
			GL_CHECK(glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(DrawVert) * vtxBuffer.size(), vtxBuffer.Data));
		}
		else
		{   	// �������������������ȣ�ֱ�Ӹ��»������е�����
			GL_CHECK(glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(DrawVert) * vtxBuffer.size(), vtxBuffer.Data));
		}
		// ����������������С
		//�Ͷ��㻺���߼�һ��
		if (DrawDataLayer->content.IdxBufferSize != idxBuffer.size())//���ڵ���������Ĵ�С����һ�εĴ�С�Ƚ� �����Ⱦ͸��� �����һ�������·��� ÿ�����ƴ��ڵ���Ϣ����������
		{
			DrawDataLayer->content.IdxBufferSize = idxBuffer.size();

			GL_CHECK(glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int) * idxBuffer.size(), nullptr, GL_DYNAMIC_DRAW));
			GL_CHECK(glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int) * idxBuffer.size(), idxBuffer.Data, GL_DYNAMIC_DRAW));
		}
		else
		{
			GL_CHECK(glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, sizeof(unsigned int) * idxBuffer.size(), idxBuffer.Data));
		}
		//�����glBuffer������������
		cmdDrawlist->Clear();//�ⲽ����˳���ÿһ�����ڵĻ����б���շ�����һ��ѭ��
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

//C����
static void Mouse_callback(GLFWwindow* window, double xpos, double ypos)//����IO�����λ��
{
	const double sensitivity = 0.93;
	static double lastxpos = 0, lastypos = 0;
	double xoffset = xpos - lastxpos;//��ȡxƫ����
	double yoffset = ypos - lastypos;//��ȡyƫ����
	lastxpos = xpos;
	lastypos = ypos;
	io->InputMessage.MouseDeltaOffset.x = xoffset * sensitivity;
	io->InputMessage.MouseDeltaOffset.y = yoffset * sensitivity;
}
static void Mouse_ButtonCallBack(GLFWwindow* window, int button, int action, int mods)//��갴���ص�����
{

	if (action == GLFW_PRESS)
		io->InputMessage.MouseDown[button] = true;
	else
		io->InputMessage.MouseDown[button] = false;

}
// ���ֻص�����
static void Mouse_Scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
	const float sensitivity = 5.09f;
	io->InputMessage.Scroll = yoffset * sensitivity;
}
static void Key_Callback(GLFWwindow* window, int key, int scancode, int action, int mods)//���̰����ص�����
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
	io->CreateFontTexture();//���������ַ�ͼ�� ��������ɫ�� ������ɫ��ʱ����glBindVertexArray()���OpenGL����
	//io->SetupDrawData_VtxList(); //�����������ݶ�������
	//����Ĭ�ϵ���ɫ
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
	/*д��IO���������XYλ��
	  д��IO��������갴����Ϣ
	  д��IO�����������Ļλ��
	  д��IO������������Ĺ���λ��
	  д��IO�����ﰴ����Ϣ*/
	glfwSetCursorPosCallback(window, Mouse_callback); //д��IO���������XYλ��
	glfwSetMouseButtonCallback(window, Mouse_ButtonCallBack);//д��IO��������갴����Ϣ
	int ShowWindowViewPortPosX = 0, ShowWindowViewPortPosY = 0, ShowWindowViewPortSizeW = 0, ShowWindowViewPortSizeH = 0, FrameBufferSizeW = 0, FrameBufferSizeH = 0;
	double RelativeCursorPosX = 0, RelativeCursorPosY = 0;
	glfwGetWindowSize(window, &ShowWindowViewPortSizeW, &ShowWindowViewPortSizeH);
	glfwGetFramebufferSize(window, &FrameBufferSizeW, &FrameBufferSizeH);
	glfwGetWindowPos(window, &ShowWindowViewPortPosX, &ShowWindowViewPortPosY);
	glfwGetCursorPos(window, &RelativeCursorPosX, &RelativeCursorPosY);
	io->InputMessage.MousePos = glm::vec2(ShowWindowViewPortPosX + RelativeCursorPosX, ShowWindowViewPortPosY + RelativeCursorPosY);//д��IO�����������Ļλ��

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
	glfwSetScrollCallback(window, Mouse_Scroll_callback);//д��IO������������Ĺ���λ��
	glfwSetKeyCallback(window, Key_Callback);

}
void ClearEvent()
{
	io->InputMessage.MouseDeltaOffset = glm::vec2(0, 0);
	io->InputMessage.MousePos = glm::vec2(0, 0);
	io->InputMessage.Scroll = 0;
	//��Ϊ��������ж���ÿʱÿ��ִ�в�ͣ���� ��λ�������ǼĴ����Ҫÿ��ʹ������0
}
void  RenderDrawData(UI* DrawUI)
{
	float Display_W = io->drawdata.DisplaySize.x;
	float Display_H = io->drawdata.DisplaySize.y;
	float Xscale = io->drawdata.framebufferscale.x;
	float Yscale = io->drawdata.framebufferscale.y;
	glViewport(0, 0, (int)(Display_W * Xscale), (int)(Display_H * Yscale));
	io->UpdateDrawDataBuffer(DrawUI);//���»������ݻ���
	io->RenderDrawList();
}
void DestructResource()
{
	io->DeleteResource();
}

void UI::StartShowWindow(const char* Text, bool Popen)
{
	
	ShowWindowContainer* showWindow = FindShowWindowByName(Text);
	bool ShowWindowJustCreated = (showWindow != nullptr); //�Ƿ��Ѿ���������
	if (!ShowWindowJustCreated)//���û�б�����
	{
		showWindow = CreateNewShowWindow(Text); //��ֹһֱnew��ɳ������
	}
	showWindow->Flag.IsWindowVisible = Popen;
	//�ȼ��������ı����
	glm::vec2 TextSize = CalcuTextSize(Text,showWindow->ScaleX, showWindow->ScaleY);
	//����title�������Ŀ�ȡ��߶Ⱥ��ⲿ�ü���Ŀ��   ���������ͨ���ж��Ƿ�Ϊ��һ֡ ͨ���ı�����۴θı������ⲿ���� �ͱ������εĿ��
	showWindow->TitleRect.Width = showWindow->OutterRect.Width=TextSize.x + showWindow->spacing *showWindow->ScaleX* 149.8;
	showWindow->TitleRect.Height = TextSize.y + showWindow->spacing * showWindow->ScaleY *7.5;//�����߶�
	//����OutterRect�����Ͻ� ���½�λ��  
	showWindow->OutterRect.Min = showWindow->Pos;
	showWindow->OutterRect.Max = showWindow->Pos + glm::vec2(showWindow->OutterRect.Width, showWindow->OutterRect.Height);
	//����Title��������Ͻ� ���½�λ��
	showWindow->TitleRect.Min = showWindow->Pos;
	showWindow->TitleRect.Max = showWindow->Pos + glm::vec2(showWindow->TitleRect.Width, showWindow->TitleRect.Height);
	//	���ñ����ı���ʾλ��
	glm::vec2 TitleTextPos = showWindow->TitleRect.Min 
		+glm::vec2(showWindow->spacing  , showWindow->spacing )
		+ glm::vec2(0, TextSize.y);//�ı�����λ�� =�ı�����Сλ�� �ճ�һЩ��� �ٿճ��ı��߶��ٻ���
	 //����OutterRect�ⲿ�ü�����
	 showWindow->content.drawlist.AddRectFill(showWindow->OutterRect.Min, showWindow->OutterRect.Max,showWindow->COLOR[OutterRectCol],9,1);
	 // ����titleRect����ü�����
	 showWindow->content.drawlist.AddRectFill(showWindow->TitleRect.Min, showWindow->TitleRect.Max, showWindow->COLOR[TitleRectCol], 9, 1);
	//����title�ı�ClipText
	 showWindow->content.drawlist.AddClipText(TitleTextPos,showWindow->COLOR[TextCol],Text, showWindow->TitleRect,showWindow->ScaleX,showWindow->ScaleY);
	 //�Ƚ�ÿ������������ɫ���ó�Ĭ������ɫ ���������ڲ������Ըı������ɫ�� ����±��в������Ըı������ɫ�Ǿ�ִ�� ���û���൱�ڻָ�ԭ������ɫ
	 showWindow->COLOR[OutterRectCol] = io->Colors[OutterRectCol];
	 showWindow->COLOR[TitleRectCol] = io->Colors[TitleRectCol];
	 showWindow->COLOR[TextCol] = io->Colors[TextCol];

	 if (showWindow->Flag.AllowResponseInputEvent)//�����ǰ����������Ӧ�����¼�
	 {
		 if (showWindow->OutterRect.Contain(io->InputMessage.MousePos))//��������ͣ������ⲿ�ü�����
		 {
			 //outterRect����ɫ�ı� ������ɫ�ָ�ԭ��
			 showWindow->COLOR[OutterRectCol] = glm::vec4(0.2,0.2,0,1);
			 //���Ƿ�����ı��������� ����outterRect����ɫ�ָ�ԭ��  TitleRect����ɫ�ı�
			 if (showWindow->TitleRect.Contain(io->InputMessage.MousePos))
			 {
				 showWindow->COLOR[OutterRectCol] = io->Colors[OutterRectCol];
				 showWindow->COLOR[TitleRectCol] = glm::vec4(0.3,0.4,0.3,1);
			 }
		     //��Ӧ���� �жϰ����ı����ݵ����ű���
			 if (io->InputMessage.KeyMesage.key == GLFW_KEY_LEFT_CONTROL && io->InputMessage.KeyMesage.press)
			 {
				 showWindow->ScaleX += io->InputMessage.Scroll * 0.05f; // �������������ƴ�С�ı仯����
				 showWindow->ScaleY += io->InputMessage.Scroll * 0.05f;
			 }
			 //�ȵ����������� �ƶ�����λ�� �ö�����
			 if (io->InputMessage.MouseDown[GLFW_MOUSE_BUTTON_LEFT])//����Ե�ǰ�����е��ִ���ö� ��ȡ���λ�ƽ��д���λ���ƶ�
			 {
				 io->Has_Bring_front_opera = true;
				 showWindow->Flag.Bring_Front = true;
				 if(showWindow->Flag.Movable)//��������ƶ��ı�־�жϱ������������ƶ�
				    showWindow->Pos += io->InputMessage.MouseDeltaOffset;
			 }
			 else
			 {
				 io->Has_Bring_front_opera = false;
				 showWindow->Flag.Bring_Front = false;
			 }
		 }
		
			
	 }
	Clamp(showWindow->ScaleX, 0.7f, 3.0f);//�������Ŵ�С
	Clamp(showWindow->ScaleY, 0.7f, 3.0f);

	 //�����괰��λ�ú�������ÿһ�δ��ڻ������ݵĸ���λ��
	 showWindow->cursorPos = showWindow->Pos;
	 showWindow->cursorPos.x += showWindow->spacing * showWindow->ScaleX* 2.3;//�������ٸ��������
	 showWindow->cursorPos.y += showWindow->TitleRect.Height +showWindow->spacing*showWindow->ScaleY * 1.7; //���������Ŀ�� ͬʱ�ڸ��������  ������ʾ�˵���ʱ�̳и���λ��
	 this->CurrentShowWindowStack.push_back(showWindow);
}

void UI::StartMenu(MenuShowOrderType ShowOrder)
{
	ShowWindowContainer*CurrentShowWindow = this->CurrentShowWindowStack.back();
	CurrentShowWindow->MenuShowOrder = ShowOrder;//��ǰ�����ڲ˵����ڷŷ�ʽ
}

void UI::Menu(const char* MenuName, bool enableMenuItem)
{
	ShowWindowContainer*CurrentShowWindow=this->CurrentShowWindowStack.back();//���ڵĴ���
	ShowMenu* menu = CurrentShowWindow->FindMenuByName(MenuName);//Ѱ��������ڵ�������ƶ�Ӧ�Ĳ˵���
	if (menu == nullptr) //����ǵ�һ��ִ�еľʹ�������˵�
	{
		menu = CurrentShowWindow->CreateMenu(MenuName);//�Ǿ�ִ��һ�δ���
	}
	CurrentShowWindow->MenuStack.push_back(menu); //������˵���������Ŀ��ʾ���ʵ�ǰ�˵�����λ�ú�������Ϣ EndMenu�󵯳�
	menu->EnableMenuItem = enableMenuItem;//�Ƿ���������Ŀ����ʾ
	//����Menu��λ�ô�С��ɫ����  ����ʱ��ɾ�ֹ����� ������ͨ�����ڵ�Menu��� ���ⲿ�ü����ڵĿ�Ƚ����໥��Լ
	menu->MenuRect.Width = CalcuTextSize(MenuName,CurrentShowWindow->ScaleX, CurrentShowWindow->ScaleY).x+CurrentShowWindow->spacing* CurrentShowWindow->ScaleX*2;
	menu->MenuRect.Height= CalcuTextSize(MenuName, CurrentShowWindow->ScaleX, CurrentShowWindow->ScaleY).y + CurrentShowWindow->spacing * CurrentShowWindow->ScaleY * 2;
	menu->MenuRect.Min = CurrentShowWindow->cursorPos;
	menu->MenuRect.Max = menu->MenuRect.Min + glm::vec2(menu->MenuRect.Width, menu->MenuRect.Height);
	//����menuRect(Menu->Rect.Min��Menu->Rect.Max��Menu->Color
	CurrentShowWindow->content.drawlist.AddRectFill(menu->MenuRect.Min, menu->MenuRect.Max,menu->MenuCol,9,1);
	//���ò˵����ı���ʾλ��
	glm::vec2 MenuTextPos = glm::vec2(menu->MenuRect.Min.x + CurrentShowWindow->spacing * CurrentShowWindow->ScaleX * 2, menu->MenuRect.Min.y + CurrentShowWindow->spacing * CurrentShowWindow->ScaleY * 2)
		+glm::vec2(0, CalcuTextSize(MenuName, CurrentShowWindow->ScaleX, CurrentShowWindow->ScaleY).y);//��Ҫ�ٿճ��ı��߶�
	//����menu��clip�ı�(MenuTextPos, menuRect, CurrentWindow->Color[Text], scaleX, scaleY);
	CurrentShowWindow->content.drawlist.AddClipText(MenuTextPos, io->Colors[TextCol],MenuName, menu->MenuRect, CurrentShowWindow->ScaleX, CurrentShowWindow->ScaleY);
	//�������ڴ��������ݸ���λ��
	if(CurrentShowWindow->MenuShowOrder==HorizontalShow)//�����ǰ���ڵĲ˵����ڷŷ�ʽΪˮƽ�ڷ�
		CurrentShowWindow->cursorPos.x += menu->MenuRect.Width + CurrentShowWindow->spacing * CurrentShowWindow->ScaleX* 4.1; //����λ����x�᷽���������� ������һ���˵������� λ�÷��ʲ���
	else if(CurrentShowWindow->MenuShowOrder == VerticalShow)
		CurrentShowWindow->cursorPos.y += menu->MenuRect.Height + CurrentShowWindow->spacing * CurrentShowWindow->ScaleY * 5.1; //����λ����y�᷽����������
	//�����ǰ����������Ӧ�����¼�
	if (CurrentShowWindow->Flag.AllowResponseInputEvent)
	{
		//�Ƿ�˵���������λ��
		if (menu->MenuRect.Contain(io->InputMessage.MousePos))//Ҫ��������˵���->Hoverable�����������������Ƿ���ʾ ������˵���->Hoverable���������������������¼��������˵����Ƿ�Hoverable����� Ҳ��ֻ��һ���˵���Hoverable����
		{
			//�ı�����˵�������ɫ
			menu->MenuCol = glm::vec4(0.2,0.1,0.2,1);
			menu->Hoverable = true;
		}
		else//˵���˵���û�гԵ����λ��
		{
			//�ָ�����˵���ԭ������ɫ
			menu->MenuCol = io->Colors[MenuRectCol];
			//�����ж��������������������¼��������˵����Ƿ�Hoverable�������ǰ�˵���->Hoverable
			 
			if (!menu->HoverRect.Contain(io->InputMessage.MousePos) && io->InputMessage.MouseDown[GLFW_MOUSE_BUTTON_LEFT]
				|| IsOtherMenuHoverable(CurrentShowWindow, menu))//��Ҫ�ų���ǰMenu�鿴����Menu
							menu->Hoverable = false; //�����ǰ�˵�����������ʾ��־

		}
	}
}

void UI::MenuItem(const char* ItemName, bool* ShowotherContextFlag)//��ֻ�����˵���������Ŀ�������
{
	//���ڵĴ���
	ShowWindowContainer*CurrentShowWindow = this->CurrentShowWindowStack.back();
	//���ڵĴ��ڵ����ڵĲ˵� ���������Ҫ��������˵�
	ShowMenu* CurrentMenu = CurrentShowWindow->MenuStack.back();
	ShowMenuItem Item;
	Item.name = ItemName;
	Item.ShowOtherContext = ShowotherContextFlag;
	Item.ItemRect=ClipRect(glm::vec2(0,0),glm::vec2(0,0));
	CurrentMenu->MenuItemStack.push_back(Item);//ÿ�ηŽ�ȥ�����ս�����Ҫ������

}

void UI::EndMenu()
{
	//���ڵĴ���
	ShowWindowContainer* CurrentShowWindow = this->CurrentShowWindowStack.back();
	//���ڵĴ��ڵ����ڵĲ˵� ���������Ҫ��������˵�
	ShowMenu* CurrentMenu = CurrentShowWindow->MenuStack.back();
	if (CurrentMenu->EnableMenuItem)//�Ƿ�ǰ�˵���������Ŀ������ʾ
	{

		if (CurrentMenu->Hoverable)//�����ǰ�˵�������������ʾ
		{
			float HoverRectWidth = 0; //���������ܿ�� ͨ��ȡ����Ŀ�ı���ȵ����ֵ�ټ���һЩ���
			float HoverRectHeight = 0; //���������ܿ�� �ۼ�ÿһ���ı�����Ч�߶��ټ���һЩ���
			for (ShowMenuItem Item : CurrentMenu->MenuItemStack)//������������Ŀ�Ŀ�߼�����ܵ�����������Ч�߿�
			{
				glm::vec2 TextSize = CalcuTextSize(Item.name, CurrentShowWindow->ScaleX, CurrentShowWindow->ScaleY);
				HoverRectWidth = std::max(HoverRectWidth, TextSize.x + CurrentShowWindow->spacing * CurrentShowWindow->ScaleX * 7.8f);
				HoverRectHeight += TextSize.y + CurrentShowWindow->spacing * CurrentShowWindow->ScaleY * 7.8;
				
			}
			//���ò˵�������������λ�þ������
			CurrentMenu->HoverRect.Width = HoverRectWidth+ CurrentMenu->ScrollBarWH.x;//��ԭ�ȵĻ����ϼ��Ϲ������Ŀ��
			CurrentMenu->HoverRect.Height = HoverRectHeight;
			CurrentMenu->HoverRect.Min = glm::vec2(CurrentMenu->MenuRect.Max.x - CurrentShowWindow->spacing * CurrentShowWindow->ScaleX * 2, CurrentMenu->MenuRect.Max.y);
			CurrentMenu->HoverRect.Max = CurrentMenu->HoverRect.Min + glm::vec2(CurrentMenu->HoverRect.Width, CurrentMenu->HoverRect.Height);
			//���Ʋ˵�����������
			CurrentShowWindow->content.drawlist.AddRectFill(CurrentMenu->HoverRect.Min, CurrentMenu->HoverRect.Max, io->Colors[HoverRectCol], 9, 1);
			//���ù������Ĳ���
			CurrentMenu->ItemScrollbar.Width = CurrentMenu->ScrollBarWH.x* CurrentShowWindow->ScaleX;
			CurrentMenu->ItemScrollbar.Height=CurrentMenu->ScrollBarWH.y * CurrentShowWindow->ScaleY;
			CurrentMenu->ItemScrollbar.Min = glm::vec2(CurrentMenu->HoverRect.Max.x- CurrentMenu->ScrollBarWH.x, CurrentMenu->HoverRect.Min.y+CurrentShowWindow->spacing)
				+glm::vec2(0, -CurrentMenu->MenuItemOffset.y);
			CurrentMenu->ItemScrollbar.Max = CurrentMenu->ItemScrollbar.Min + CurrentMenu->ScrollBarWH;
			//�ٻ��ƹ�����
			CurrentShowWindow->content.drawlist.AddRectFill(CurrentMenu->ItemScrollbar.Min, CurrentMenu->ItemScrollbar.Max, CurrentMenu->ScrollbarCol,5,1);
			

			//�������������ı���λ��
			glm::vec2 HoverTextRectPos = CurrentMenu->HoverRect.Min +
				glm::vec2(CurrentShowWindow->spacing * CurrentShowWindow->ScaleX * 1.2, CurrentShowWindow->spacing * CurrentShowWindow->ScaleX * 1.2 + CurrentMenu->MenuItemOffset.y); //��������� Ȼ������ɹ��������Ƶ�ƫ��deltaλ��
			//��Ӧ�����ƶ��¼� ʹλ�Ʊ仯(��֤��������������ڲ�����) ͬʱ���ƹ����� ����Ҫ����MenuOffset��ֵ
			Clamp(CurrentMenu->MenuItemOffset.y, CurrentMenu->ItemScrollbar.Height-CurrentMenu->HoverRect.Height  ,0 );
			if(CurrentMenu->HoverRect.Contain(io->InputMessage.MousePos))//��Ӧ�����ֹ���ƫ��
			  CurrentMenu->MenuItemOffset.y += io->InputMessage.Scroll;
			//���ж�����Ƿ���ͣ�ڹ����� ����������� ͨ�������������˵���������Ŀ ƫ��
			if (CurrentMenu->ItemScrollbar.Contain(io->InputMessage.MousePos))
			{
				CurrentMenu->ScrollbarCol = glm::vec4(1,0.4,0,1);
				if (io->InputMessage.MouseDown[GLFW_MOUSE_BUTTON_LEFT])
				{
					CurrentMenu->MenuItemOffset.y -= io->InputMessage.MouseDeltaOffset.y;
					//ͬʱ�����ǰ���ڿ��ƶ��ı�־�����ж�
					CurrentShowWindow->Flag.Movable = false;
				}
				
			}
			else {
				CurrentShowWindow->Flag.Movable = true;
				CurrentMenu->ScrollbarCol = io->Colors[ScrollBarCol];
			}
				
			for (ShowMenuItem& Item : CurrentMenu->MenuItemStack)//���������˵�������������Ŀ
			{
				//���õ�������Ŀ���ı���λ�� ��߲���
				Item.ItemRect.Width= CalcuTextSize(Item.name, CurrentShowWindow->ScaleX, CurrentShowWindow->ScaleY).x + CurrentShowWindow->spacing * CurrentShowWindow->ScaleX * 4.7;
				Item.ItemRect.Height = CalcuTextSize(Item.name, CurrentShowWindow->ScaleX, CurrentShowWindow->ScaleY).y + CurrentShowWindow->spacing * CurrentShowWindow->ScaleY * 4.7;
				Item.ItemRect.Min = HoverTextRectPos;
				Item.ItemRect.Max = HoverTextRectPos + glm::vec2(Item.ItemRect.Width, Item.ItemRect.Height);

				//����Ҫ�����ж�����Ŀ���ı����Ƿ�Խ���������ı߽� �ü��� �ı䵥������Ŀ�ı������
				if (Item.ItemRect.Min.y> CurrentMenu->HoverRect.Min.y&& Item.ItemRect.Max.y< CurrentMenu->HoverRect.Max.y)//�����ǰ�����������ı���������λ�� ��Ҫ��֤�����������Ŀ���������������ڲ�
				{
					glm::vec4 HoverTextRextCol = io->Colors[HoverTextRectCol];//�����������ı�����ɫ
					if (Item.ItemRect.Contain(io->InputMessage.MousePos))//�����λ���ڵ����ı�������
					{
						HoverTextRextCol = glm::vec4(0.5,0.6,0,1);//�ı��� �������֮�ϵĸ�����ɫ
						if (io->InputMessage.MouseDown[GLFW_MOUSE_BUTTON_LEFT]) {//����������֮�������һ��
							if (Item.ShowOtherContext != nullptr)
								*Item.ShowOtherContext = true;//������ʾ�������ݵ�flag
						}	
					}
					// ���Ʋ˵�������Ŀ�ı���
					CurrentShowWindow->content.drawlist.AddRectFill(Item.ItemRect.Min, Item.ItemRect.Max, HoverTextRextCol, 9, 1);
					//���������������ı�λ��   �ı������������ٿճ��ı��߶ȼ�Ϊ�ı���ʼ��ʾ��λ��
					glm::vec2 HoverTextPos = HoverTextRectPos + glm::vec2(0, CalcuTextSize(Item.name, CurrentShowWindow->ScaleX, CurrentShowWindow->ScaleY).y)
						+ glm::vec2(CurrentShowWindow->spacing * 1.3, CurrentShowWindow->spacing * 1.3);
					//���Ʋ˵�������Ŀ�ı�
					CurrentShowWindow->content.drawlist.AddClipText(HoverTextPos, io->Colors[TextCol], Item.name, CurrentMenu->HoverRect, CurrentShowWindow->ScaleX, CurrentShowWindow->ScaleY);
				}
				
				HoverTextRectPos.y += CurrentShowWindow->spacing * 2 + Item.ItemRect.Height;//����������ƽ��
			}


		}
		else//���������ʾ��������������˵�������Ŀ��ƫ��
			CurrentMenu->MenuItemOffset = glm::vec2(0,0);
		  
	}
	//�����������߼��ͻ�������� ����ѭ�����µ��ڴ�
	CurrentMenu->MenuItemStack.clear();
	CurrentShowWindow->MenuStack.pop_back();
}
void UI::EndShowWindow()
{
	ShowWindowContainer* CurrentShowWindow = this->CurrentShowWindowStack.back();
	CurrentShowWindow->Flag.AllowResponseInputEvent = false;//ʹ��������ձ�־
	this->CurrentShowWindowStack.pop_back();
}
ShowWindowContainer* UI::CreateNewShowWindow(const char* name)
{
	ShowWindowContainer* newShowWindow = new ShowWindowContainer();//��ʼ�����������ɵĴ��ڵĲ���
	
	this->NameToShowWindowMap.insert(name, newShowWindow);
	if (this->LastShowWindow == nullptr)//���֮ǰû�д�����Ϊ��һ������ֱ��ȡ�м�λ����ʾ
		newShowWindow->Pos = glm::vec2(io->drawdata.DisplaySize.x / 2, io->drawdata.DisplaySize.y / 2);
	else//���֮ǰ�Ѿ��д��ھ͸������
		newShowWindow->Pos = this->LastShowWindow->Pos + glm::vec2(30, 60);
	this->LastShowWindow = newShowWindow;//��Ҫ��һ�δ��ڷ�����һ������ʱʹ��
	
	io->drawdata.DrawDataLayerBuffer.push_back(newShowWindow);//ֻ���ڵ�һ�δ���ʱ�Ų��� ���ڴ��ڶ�����Դ����ʱ�Ž��з��� ����Ͳ�push_back�� һֱ���ʻ��߽����޸�
	io->CreateShowWindow_glBuffer(newShowWindow);
	return newShowWindow;
}

ShowWindowContainer* UI::FindShowWindowByName(const char* name)
{
	if (this->NameToShowWindowMap.find(name))//����ܴ����ҵ���ֵ
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
	this->MenuMap.insert(name,newShowMenu);//ִֻ��һ��
	this->MenuArray.push_back(newShowMenu);//ִֻ��һ�η�������
	return newShowMenu;
}

ShowMenu* ShowWindowContainer::FindMenuByName(const char* name)
{
	if(this->MenuMap.find(name))
		return this->MenuMap[name];
	return nullptr;
}
