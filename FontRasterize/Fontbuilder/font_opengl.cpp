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

		// ����ַ��Ƿ�����Ч��Χ��
		if (C >= io->atlas.glyphfonts->IndexSearchTable.size()) {
			continue; // ��Ч�ַ�������
		}
		unsigned int glyphid = 0;
		if (glyphid >= io->atlas.glyphfonts->Glyphs.size()) {
			continue; // ����ID��Ч������
		}
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
		// ����ַ��Ƿ�����Ч��Χ��
		if (C >= io->atlas.glyphfonts->IndexSearchTable.size()) {
			continue; // ��Ч�ַ�������
		}
		unsigned int glyphid = 0;
		if (glyphid >= io->atlas.glyphfonts->Glyphs.size()) {
			continue; // ����ID��Ч������
		}
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

		// ����ַ��Ƿ�����Ч��Χ��
		if (C >= io->atlas.glyphfonts->IndexSearchTable.size()) {
			continue; // ��Ч�ַ�������
		}
		unsigned int glyphid = 0;
		if (glyphid >= io->atlas.glyphfonts->Glyphs.size()) {
			continue; // ����ID��Ч������
		}
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

		/*	std::cout << "clipRect.Min.x=" << clipRect.Min.x << std::endl;
			std::cout << "clipRect.Max.x="<< clipRect.Max.x << std::endl;
			std::cout << "x0="<<x0 << std::endl;
			std::cout << "x1=" << x1 << std::endl;
			std::cout << " " << std::endl;*/
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
	//����drawlist��������
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
		if (!DrawDataLayer->Flag.IsWindowVisible)//���������ڲ�������ʾ�Ͳ�ִ���������
		{
			DrawDataLayer->content.drawlist.Clear();//ͬʱ�����������Դ��������ʾ ֱ�������
			continue;
		}
			
		DrawList* cmdDrawlist = &DrawDataLayer->content.drawlist;//��ȡ����ÿһ��������ʾ���ڵĻ�������
		DynamicArray<DrawVert>& vtxBuffer = cmdDrawlist->_VtxBuffer;
		DynamicArray<unsigned int>& idxBuffer = cmdDrawlist->_IdxBuffer;
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


	//���ô��ڵ��ⲿ�ü����� ͬʱҲ�ܱ�֤�����ⲿ�ü����β���
	showWindow->OutterRect.Min = showWindow->Pos;
	showWindow->OutterRect.Max = showWindow->Pos +glm::vec2(460, 490);
	showWindow->content.drawlist.AddRectFill(showWindow->OutterRect.Min, showWindow->OutterRect.Max, showWindow->TitleColor, 9.0f, 1);
	showWindow->content.drawlist.AddRectBorder(showWindow->OutterRect.Min, showWindow->OutterRect.Max,glm::vec4(1,1,0,1),2.0f);
	//showWindow->drawlist.AddRectTriangleBorder(showWindow->OutterRect.Min, showWindow->OutterRect.Max, glm::vec4(1, 1, 0, 1), 1.3f);
	showWindow->content.drawlist.AddClipText(showWindow->Pos + glm::vec2(0, 53), glm::vec4(0, 1,1, 1), Text, showWindow->OutterRect,showWindow->ScaleX, showWindow->ScaleY);//���Ʊ���

	if (showWindow->OutterRect.Contain(io->InputMessage.MousePos) && showWindow->Flag.AllowResponseInputEvent)//�Ƿ�ǰ���ڵ�Rect�������λ�� ����������Ӧ�¼� 
	{
		showWindow->TitleColor = glm::vec4(0, 0.2, 0.2,1);
		if (io->InputMessage.KeyMesage.key == GLFW_KEY_LEFT_CONTROL && io->InputMessage.KeyMesage.press)
		{
			showWindow->ScaleX += io->InputMessage.Scroll * 0.05f; // �������������ƴ�С�ı仯����
			showWindow->ScaleY += io->InputMessage.Scroll * 0.05f;
		}
		if (io->InputMessage.KeyMesage.key == GLFW_KEY_T && io->InputMessage.KeyMesage.press)//���T�������»��������ı��Ķ���
		{
			
			showWindow->content.drawlist.AddText_VertTriangleBorder(showWindow->Pos + glm::vec2(0, 53), glm::vec4(1, 0, 1, 1), Text, showWindow->ScaleX, showWindow->ScaleY,1.1);
		}

		if (io->InputMessage.MouseDown[GLFW_MOUSE_BUTTON_LEFT])//����Ե�ǰ�����е��ִ���ö� ��ȡ���λ�ƽ��д���λ���ƶ�
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
	this->CurrentShowWindowStack.push_back(showWindow);//�����Ҫ�ڻ��Ƶ�ǰ���ڵ�����Ŀ�ı�ʱ��Ҫ�õ���ǰ���ڵ���Ҫλ�� ��Ҫ���ڸ�����ȥ
	
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

