#pragma once
#include<iostream>
#include"internal.h"
//�ַ���������
typedef unsigned short Char16;
typedef unsigned int Char32;

//�������ַ��������� 
struct FontGlyph
{
	float X0, Y0, X1, Y1;//���½� ���Ͻ�ɫ������
	float U0, V0, U1, V1;//������������ڣ�0-1��������
	float AdvanceX;//������һ���ַ��ľ���
	unsigned int CodePoint;
};


struct font_of_characters
{
	float Ascent = 0.0, Decent = 0.0f;//�������ˮƽ�߾���
	float fontScale = 0.0f;//�������Ŵ�С
	float advanceX = 0.0f;//����ֵ ������һ���ַ��ľ���
	DynamicArray<FontGlyph>Glyphs;//�洢һϵ������Ĳ�������
	DynamicArray<unsigned int>IndexSearchTable;//��ASCLL������ֵGlyph����������Ϣ
	void AddGlyph(float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1, float AdvanceX, unsigned int CharCodePoint);
	void buildLookUpTable_Glyph_CharCodePoint();//д���ַ���ASCLL�ַ����Glyph��������������Ķ�Ӧ��
	bool FindGlyph(unsigned int CodePoint, unsigned int& glyphID);
};
struct fontloadInfo
{
	void* FontData;//ttf font data     ����Ҫд��ĵ�ַ����
	unsigned int FontDataSize;//�����ļ���С
	Char16* SrcRangeForCharCodePoint = nullptr;//�ַ�ASCLL������Χ
	unsigned int fontId = 0;//index of ttf 
	float Fontsize = 0.0f;//�����С
};
struct outglTexInfo
{
	unsigned int* Texpixel;
	unsigned int width;
	unsigned int height;
};
struct FontAtlas//�洢��Ⱦ��������ͼ��
{
	void AddFont_FromFile(const char* filename, float size_pixels, Char16* GlyphRange);
	void Addfont_internal();//�����ڲ�������ļ�
	void AddFont(fontloadInfo* fontcfgInfo);//����µķ������
	void BuildFont();
	void WriteAlpha8_Data(unsigned int* glTexDataAddr);//����glTexData��ͼ���ַ
	void BuildWhiteRect(unsigned char* pixel, unsigned int width, unsigned int height, unsigned int stride_in_bytes);
	outglTexInfo GetTexAlpha32();
	Char16* Get_LatinGlyph_CodePointRange();
	Char16* Get_ChineseGlyph_CodePointRange();
	font_of_characters* glyphfonts;//������������ַ��Ĳ�������Glyphs��0-1��Χ��
	void* textureAddress;//��������ͼ���󶨵ĵ�ַ
	float WhiteU1, WhiteV1;
	float WhiteU2, WhiteV2;
	float WhiteHeight = 10, WhiteWidth = 10;//��һ��ɫ�Ĳ�������  ɫ���� ɫ��߶�
	unsigned char* TexAlpha8 = nullptr;//�洢������ɫALphaֵ 
	unsigned int TexWidth = 0;//��������ͼ���Ŀ��
	unsigned int TexHeight = 0;//��������ͼ���ĸ߶�
	bool(*FontBuild)(FontAtlas* fontAtlas);//����ָ�� ��ʱ��ʵ����FontAtlas fontatlas.FontBuild=TruetypeFontBuilder;//���彨��io��
	DynamicArray<fontloadInfo>FontConfigData;//���ص�����������Ϣ �п�����΢���ź� ���� ���������ļ� calivo Italy�����ļ�
	const float TexGlyphPadding = 2;//��װ���ַ�����ļ��
	float fontSize = 0;
};
