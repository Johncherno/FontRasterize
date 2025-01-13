#pragma once
#include<iostream>
#include"internal.h"
//字符索引类型
typedef unsigned short Char16;
typedef unsigned int Char32;

//采样的字符纹理坐标 
struct FontGlyph
{
	float X0, Y0, X1, Y1;//左下角 右上角色块坐标
	float U0, V0, U1, V1;//纹理采样坐标在（0-1）区间内
	float AdvanceX;//到达下一个字符的距离
	unsigned int CodePoint;
};


struct font_of_characters
{
	float Ascent = 0.0, Decent = 0.0f;//字体距离水平线距离
	float fontScale = 0.0f;//字体缩放大小
	float advanceX = 0.0f;//步进值 到达下一个字符的距离
	DynamicArray<FontGlyph>Glyphs;//存储一系列字体的采样坐标
	DynamicArray<unsigned int>IndexSearchTable;//用ASCLL码索引值Glyph采样坐标信息
	void AddGlyph(float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1, float AdvanceX, unsigned int CharCodePoint);
	void buildLookUpTable_Glyph_CharCodePoint();//写入字符串ASCLL字符码和Glyph采样坐标的索引的对应表
	bool FindGlyph(unsigned int CodePoint, unsigned int& glyphID);
};
struct fontloadInfo
{
	void* FontData;//ttf font data     即将要写入的地址内容
	unsigned int FontDataSize;//字体文件大小
	Char16* SrcRangeForCharCodePoint = nullptr;//字符ASCLL索引范围
	unsigned int fontId = 0;//index of ttf 
	float Fontsize = 0.0f;//字体大小
};
struct outglTexInfo
{
	unsigned int* Texpixel;
	unsigned int width;
	unsigned int height;
};
struct FontAtlas//存储渲染字体纹理图集
{
	void AddFont_FromFile(const char* filename, float size_pixels, Char16* GlyphRange);
	void Addfont_internal();//加载内部字体的文件
	void AddFont(fontloadInfo* fontcfgInfo);//添加新的风格字体
	void BuildFont();
	void WriteAlpha8_Data(unsigned int* glTexDataAddr);//传给glTexData的图像地址
	void BuildWhiteRect(unsigned char* pixel, unsigned int width, unsigned int height, unsigned int stride_in_bytes);
	outglTexInfo GetTexAlpha32();
	Char16* Get_LatinGlyph_CodePointRange();
	Char16* Get_ChineseGlyph_CodePointRange();
	font_of_characters* glyphfonts;//这里存着所有字符的采样坐标Glyphs在0-1范围内
	void* textureAddress;//字体纹理图集绑定的地址
	float WhiteU1, WhiteV1;
	float WhiteU2, WhiteV2;
	float WhiteHeight = 10, WhiteWidth = 10;//单一颜色的采样坐标  色块宽度 色块高度
	unsigned char* TexAlpha8 = nullptr;//存储字体颜色ALpha值 
	unsigned int TexWidth = 0;//字体纹理图集的宽度
	unsigned int TexHeight = 0;//字体纹理图集的高度
	bool(*FontBuild)(FontAtlas* fontAtlas);//函数指针 到时候实例化FontAtlas fontatlas.FontBuild=TruetypeFontBuilder;//字体建立io口
	DynamicArray<fontloadInfo>FontConfigData;//加载的字体配置信息 有可能是微软雅黑 宋体 黑体字体文件 calivo Italy字体文件
	const float TexGlyphPadding = 2;//包装的字符方块的间距
	float fontSize = 0;
};
