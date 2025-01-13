#pragma once
#include<iostream>
//truetype数据类型
typedef unsigned char truetype_uint8;
typedef signed char truetype_int8;
typedef unsigned short truetype_uint16;
typedef signed short truetype_int16;
typedef unsigned int truetype_uint32;
typedef signed int truetype_int32;

truetype_uint16 GetPointerTwoBytes_uint16_Content(truetype_uint8* p) { return (p[0]<<8) | p[1]; }//多字节合并数据操作
 truetype_int16 GetPointerTwoBytes_int16_Content(truetype_uint8* p) { return (p[0] << 8) | p[1]; }
 truetype_uint32 GetPointerFourBytes_uint32_Content(truetype_uint8* p) { return (p[0] <<24) |(p[1]<<16)|(p[2]<<8)|p[3]; }
 truetype_int32 GetPointerFourBytes_int32_Content(truetype_uint8* p) { return (p[0] << 24)| (p[1] << 16) | (p[2] << 8)|p[3]; }

#define tag4(p,t0,t1,t2,t3)  ((p)[0]==(t0)&&(p)[1]==(t1)&&(p)[2]==t2&&(p)[3]==(t3))
#define truetype_tag(p,str) tag4(p,str[0],str[1],str[2],str[3])

struct ttfBuildFontInfo
{
	unsigned char* data;//ttf文件的地址
	int fontstart;//字体开始的位置
	unsigned int numGlyphs;//多少个字符

	unsigned int headFormatMap, Spacing_AdvanceX, ascent_descent, MaxCharNum, Unicode_localndexmap, glyphCurve_loca, glyphCurve,PlatForm_map;
	unsigned int UnicodeIndex_To_glyphLocaFormat;
	//....................
};//ttf字体信息
truetype_int32 Truetype_find_tagName(unsigned char*data,unsigned int fontstart,const char*tag)
{
	const unsigned int scalar_Bytes = 4, num_tables_Bytes = 2, searchRange_Bytes = 2, entryselector_Bytes = 2, rangeshift_bytes = 2,tag_nameBytes=4,CheckSumByte=4,singleTableInfoBytes=16;
	unsigned int num_tables = GetPointerTwoBytes_uint16_Content(data + fontstart + scalar_Bytes);
	unsigned int tabledir = fontstart + scalar_Bytes+ num_tables_Bytes+ searchRange_Bytes+ entryselector_Bytes+ rangeshift_bytes;//需要跳过4个字节的scalar值 跳过numtables值的字节数 searchRange的2个字节数 entryselector的2个字节数 rangeshift的2个字节数
	
	for (unsigned int i = 0; i < num_tables; ++i)
	{
		unsigned int loc = tabledir + singleTableInfoBytes * i;
		if (truetype_tag(data + loc + 0, tag))
			return GetPointerFourBytes_uint32_Content(data + loc + tag_nameBytes+ CheckSumByte);
	}
	return 0;
}

float Truetype_Getscale_pixel(ttfBuildFontInfo* fontInfo,float wantPixelHeight)
{
	int fheight = GetPointerTwoBytes_int16_Content(fontInfo->data + fontInfo->ascent_descent + 4) - GetPointerTwoBytes_int16_Content(fontInfo->data + fontInfo->ascent_descent + 6);
	return wantPixelHeight / fheight;
}
int ttfFontFindGlyphIndex(ttfBuildFontInfo* fontInfo, unsigned int CharCodePoint)
{

	unsigned short format = GetPointerTwoBytes_uint16_Content(fontInfo->data + fontInfo->Unicode_localndexmap);//读出来的数据是哪种格式的
	//根据这个格式来判断
	// Apple格式
	//  | Offset | 内容 | 描述                          |
	//	|---------- - | ------------------------ | ------------------------------ - |
	//	| +0 | format = 0 | 格式号                       |
	//	| +2 | tablelength| 表的长度                     |
	//	| +4 | language   | 语言 ID                      |
	//	| +6 | glyphIdArray[256] | 每个字符对应的字形索引    即字符索引数组 当为‘A’字符是为65 glyphIdArray[65]的内容就为它的字形索引值
	if (format==0)
	{
		unsigned int tablelength = GetPointerTwoBytes_uint16_Content(fontInfo->data + fontInfo->Unicode_localndexmap+2);//需要跳过存储格式号的字节
		if (CharCodePoint< tablelength-6)//-6是因为是要去掉 格式号的存储值的长度、存储表长度的值长度、语言iD的值长度 2+2+2=6
		{
			return (fontInfo->data + fontInfo->Unicode_localndexmap + 6)[CharCodePoint];
		}
		return 0;
	}
	/*    | Offset | 内容 | 描述                          |
		|---------- - | ---------------------- | ------------------------------ - |
		| +0 | format = 6 | 格式号                       |
		| +2 | length | 表的长度                     |
		| +4 | language | 语言 ID                      |
		| +6 | firstCode | 范围起始 Unicode 码点        |
		| +8 | entryCount | 范围内的字符数量             |
		| +10 | glyphIdArray | 范围内的字形索引数组 |
		+0x0A | glyphIdArray[0]->GlyphIndex(firstCode)
		+ 0x0C | glyphIdArray[1]->GlyphIndex(firstCode + 1)
		...
		+ N | glyphIdArray[N]->GlyphIndex(firstCode + N)*/
	else if (format == 6)
	{
		unsigned int firstCode = GetPointerTwoBytes_uint16_Content(fontInfo->data + fontInfo->Unicode_localndexmap +6);
		unsigned int Num_Code= GetPointerTwoBytes_uint16_Content(fontInfo->data + fontInfo->Unicode_localndexmap + 8);
		if (firstCode<=CharCodePoint&& CharCodePoint< firstCode+ Num_Code)//检查 unicode_codepoint 是否在 [firstCode, firstCode + Num_Code) 范围内，然后根据偏移量查找字形索引
		{
			return GetPointerTwoBytes_uint16_Content(fontInfo->data + fontInfo->Unicode_localndexmap + 10+(CharCodePoint- firstCode)*2);
		}
		return 0;
	}
	/*  | Offset | 内容 | 描述                          |
		|---------- - | ---------------------- | ------------------------------ - |
		| +0 | format = 12 / 13 | 格式号                       |
		| +2 | reserved | 保留字段                     |
		| +4 | length | 表的长度                     |
		| +8 | language | 语言 ID                      |
		| +12 | nGroups | 段组数量                     |
		| +16 | groups[nGroups] | 每个段组的起始和结束码点 |*/
     //memory:
     //	+0x00 | format : 12
     //		+ 0x04 | reserved
     //		+ 0x08 | length : 表的总长度
     //		+ 0x0C | language : 语言 ID
     //		+ 0x10 | nGroups : 段组数量
     //		+ 0x14 | Group[0] -> {startCharCode, endCharCode, startGlyphId}
     //	+ 0x20 | Group[1] -> {startCharCode, endCharCode, startGlyphId}
     //	...
	else if (format == 12|| format == 13)
	{
		const unsigned int group_bytes = 12,startCharCodeOffset=0,endCharCodeOffset=4,startCharglyfIdOffset=8;
		//开始二分查找
		unsigned int nGroups = GetPointerFourBytes_uint32_Content(fontInfo->data + fontInfo->Unicode_localndexmap+12);//多少组{开始unicode 结束unicode 开始unicode的字形索引}
		unsigned int low=0, high= nGroups;
		while (low<high)//只要low的索引小于high的索引
		{
			unsigned int  Middle = low + (high - low) / 2;//中间索引
			unsigned int startUnicode = GetPointerFourBytes_uint32_Content(fontInfo->data + fontInfo->Unicode_localndexmap + 12+Middle*group_bytes+startCharCodeOffset);//(fontInfo->data + fontInfo->Unicode_localndexmap+12)[Middle].startCharCode
			unsigned int endUnicode= GetPointerFourBytes_uint32_Content(fontInfo->data + fontInfo->Unicode_localndexmap + 12 + Middle * group_bytes + endCharCodeOffset);//(fontInfo->data + fontInfo->Unicode_localndexmap+12)[Middle].endCharCode
			if (CharCodePoint < startUnicode)//如果小说明在左边
			{
				high = Middle;//此时范围要在（group[0]-group[0+end/2]）
			}
			else if (CharCodePoint> endUnicode)//如果大说明在右边
			{
				low = Middle + 1;//此时范围要在（group[0+end/2+1]-group[end]）
			}
			else//说明在这个范围内 找出这个范围中开始unicode的字形索引 在这个范围计算出输入的unicode的字形索引
			{
				unsigned int startUnicode_glyphId= GetPointerFourBytes_uint32_Content(fontInfo->data + fontInfo->Unicode_localndexmap + 12 + Middle * group_bytes + startCharglyfIdOffset);//(fontInfo->data + fontInfo->Unicode_localndexmap+12)[Middle].startGlyphId
				if (format == 12)
					return startUnicode_glyphId + CharCodePoint - startUnicode;
				else//13
					return startUnicode_glyphId;
			}
		}
	}
	/*| Offset | 内容 | 描述                          |
		|---------- - | ---------------------- | ------------------------------ - |
		| +0 | format = 4 | 格式号                       |
		| +2 | length | 表的长度                     |
		| +4 | language | 语言 ID                      |
		| +6 | segCountX2 | 段数量 * 2                   |
		| +8 | searchRange | 搜索范围                     |
		| +10 | entrySelector | 二分搜索的迭代次数           |
		| +12 | rangeShift | 搜索范围偏移量               |
		| +14 | endCode[segCount] | 每个段的结束码点             |
		| +XX | reservedPad | 对齐填充                     |
		| +XX | startCode[segCount] | 每个段的起始码点             |
		| +XX | idDelta[segCount] | 偏移量数组                   |
		| +XX | idRangeOffset[segCount] | 偏移表数组 |*/
  //   memory:
	 //    +0x00 | format : 4
		//+ 0x02 | length : 表的总长度
		//+ 0x04 | language : 语言 ID
		//+ 0x06 | segCountX2 : 段数量 * 2
		//+ 0x08 | searchRange
		//+ 0x0A | entrySelector
		//+ 0x0C | rangeShift
		//+ 0x0E | endCode[0]->Segment 0 End
		//+ 0x10 | endCode[1]->Segment 1 End
		//...                                          -----endCode[i]->Segment i End    ------------search-baseOffset
		//+ N | startCode[0]->Segment 0 Start
		//+ N + 2 | startCode[1]->Segment 1 Start
		//...                                         -----startCode[i]->Segment 0 Start  ------------search-baseOffset
		//+ M | idDelta[0]->Segment 0 Delta
		//+ M + 2 | idDelta[1]->Segment 1 Delta
		//...
		//+ P | idRangeOffset[0]->Segment 0 Range Offset
		//+ P + 2 | idRangeOffset[1]->Segment 1 Range Offset
  //       ......                                        ---------idRangeOffset[i]->Segment i Range Offset------------search-baseOffset
		//+ offset | locaId[i]{ startCode ,endCode }->loca映射表索引 i为第几组的startcode-endcode
	else if (format == 4) { // standard mapping for windows fonts: binary search collection of ranges
		const unsigned int reservedPad_ValueBytes = 2;
		const unsigned int Bytes = 2;//单个变量的字节数
		truetype_uint16 segcount = GetPointerTwoBytes_uint16_Content(fontInfo->data + fontInfo->Unicode_localndexmap + 6) >> 1;
		truetype_uint16 searchRange = GetPointerTwoBytes_uint16_Content(fontInfo->data + fontInfo->Unicode_localndexmap + 8) >> 1;
		truetype_uint16 loopCount = GetPointerTwoBytes_uint16_Content(fontInfo->data + fontInfo->Unicode_localndexmap + 10);//最大循环迭代次数
		truetype_uint16 rangeShift = GetPointerTwoBytes_uint16_Content(fontInfo->data + fontInfo->Unicode_localndexmap + 12) >> 1;//搜索范围步进距离
		// do a binary search of the segments
		const unsigned int baseOffset = fontInfo->Unicode_localndexmap + 14;//基准偏移
		truetype_uint32 search = baseOffset;//search代表搜索指针

		if (CharCodePoint > 0xffff)
			return 0;
		//二分法寻找
		// they lie from endCount .. endCount + segCount
		// but searchRange is the nearest power of two, so...
		if (CharCodePoint >= GetPointerTwoBytes_uint16_Content(fontInfo->data + baseOffset + rangeShift * Bytes))
			search += rangeShift * Bytes;

		search -= Bytes;
		while (loopCount) {
			truetype_uint16 end;
			searchRange >>= 1;
			end = GetPointerTwoBytes_uint16_Content(fontInfo->data + search + searchRange * Bytes);
			if (CharCodePoint > end)//如果输入的字符码比这一范围的结束码还大 那就说明在后面有匹配的 search指针往右移动靠拢目标索引
				search += searchRange * Bytes;//如果输入unicode稍小那么搜索范围不停缩小一直读取search+搜索范围的end值直到逼近这个unicode索引search不动最后search加上最后缩小终范围    如果输入unicode很大那么search不停往右移逼近它
			--loopCount;
		}
		search += Bytes;//这里search再加上Bytes是因为不加之前存着的是比他小的unicode 后面才是它真正的索引
		
		{
			truetype_uint16 offset, startCharUnicode,endCharUnicode;
			truetype_uint16 index = (truetype_uint16)((search - baseOffset) / Bytes);//endcount等于fontInfo->Unicode_localndexmap + 14 index相当于从有数组范围的偏移索引 >>1相当于除以每个结构变量的长度为2个字节

			startCharUnicode = GetPointerTwoBytes_uint16_Content(fontInfo->data + baseOffset+ segcount * Bytes*1 + reservedPad_ValueBytes + Bytes * index);
			endCharUnicode =   GetPointerTwoBytes_uint16_Content(fontInfo->data + baseOffset+ Bytes * index);
			if (CharCodePoint< startCharUnicode || CharCodePoint> endCharUnicode)
				return 0;

			offset = GetPointerTwoBytes_uint16_Content(fontInfo->data + baseOffset + segcount * Bytes*3 + reservedPad_ValueBytes + Bytes * index);
			if (offset == 0)
				return (truetype_uint16)(CharCodePoint + GetPointerTwoBytes_int16_Content(fontInfo->data + baseOffset + segcount * Bytes * 2 + reservedPad_ValueBytes + Bytes * index));
			//跨过基准偏移baseOffset  跨过存放endcode startcode iddelta的字段和对齐字节segcount * Bytes * 3 + reservedPad_ValueBytes  再到偏移数组的具体索引 再偏移offset到存储这个unicode范围的loca索引表利用输入的unicode与开始startunicode插值找出索引
			return GetPointerTwoBytes_uint16_Content(fontInfo->data + baseOffset  +segcount * Bytes * 3 + reservedPad_ValueBytes +Bytes * index + offset + (CharCodePoint - startCharUnicode) * Bytes);
		}
		}
	
	return 0;
}
int ttfFontGetGlyphOffset(ttfBuildFontInfo* fontInfo, unsigned int idx)
{
	int g1, g2;
	const unsigned int FourBytes=4, TwoBytes = 2, OneByte = 1;

	if (idx >= fontInfo->numGlyphs) return -1; // glyph index out of range
	if (fontInfo->UnicodeIndex_To_glyphLocaFormat >= 2)    return -1; // unknown index->glyph map format

	if (fontInfo->UnicodeIndex_To_glyphLocaFormat == 0) {
		g1 = fontInfo->glyphCurve + GetPointerTwoBytes_uint16_Content(fontInfo->data + fontInfo->glyphCurve_loca + idx * TwoBytes) * TwoBytes;//乘以2的目的是因为此时格式的字形单位变量是2个字节存储的
		g2 = fontInfo->glyphCurve + GetPointerTwoBytes_uint16_Content(fontInfo->data + fontInfo->glyphCurve_loca + idx * TwoBytes + TwoBytes) * TwoBytes;
	}
	else {
		g1 = fontInfo->glyphCurve + GetPointerFourBytes_uint32_Content(fontInfo->data + fontInfo->glyphCurve_loca + idx * FourBytes)* OneByte;
		g2 = fontInfo->glyphCurve + GetPointerFourBytes_uint32_Content(fontInfo->data + fontInfo->glyphCurve_loca + idx * FourBytes + FourBytes) * OneByte;
	}

	return g1 == g2 ? -1 : g1; // if length is 0, return -1
}
static int TruetypeGetGlyphBoundingBox(ttfBuildFontInfo* fontInfo, int glyphIndex, int* x0, int* y0, int* x1, int* y1)
{
	int offset=ttfFontGetGlyphOffset(fontInfo, glyphIndex);
	if (offset<0)return 0;
	if (x0) *x0 = GetPointerTwoBytes_int16_Content(fontInfo->data+ offset+2);
	if (y0) *y0 = GetPointerTwoBytes_int16_Content(fontInfo->data + offset+4);
	if (x1) *x1 = GetPointerTwoBytes_int16_Content(fontInfo->data + offset+6);
	if (y0) *y1 = GetPointerTwoBytes_int16_Content(fontInfo->data + offset+8);
	return 1;
}
void TruetypeGetGlyphBitmapBox(ttfBuildFontInfo* fontInfo,int glyphIndex,float scale,int*x0, int* y0, int* x1, int* y1)
{
	int X0 = 0, Y0= 0, X1=0, Y1=0;
	if (!TruetypeGetGlyphBoundingBox(fontInfo, glyphIndex, &X0, &Y0, &X1, &Y1))
	{
		if(x0)*x0 = 0;
		if(y0)*y0 = 0;
		if(x1)*x1 = 0;
		if(y1)*y1 = 0;
	}
	else//y值在负半轴
	{
		if(x0)*x0 = X0 * scale;
		if(y0)*y0 = -Y1 * scale;
		if(x1)*x1 = X1 * scale;
		if(y1)*y1 = -Y0 * scale;
	}
}
void TruetypeGetMetric_spacing(ttfBuildFontInfo* fontInfo, int glyphIndex, int* advanceX)
{
	unsigned int numOfLongHorMetrics = GetPointerTwoBytes_uint16_Content(fontInfo->data + fontInfo->ascent_descent + 34);
	if (glyphIndex < numOfLongHorMetrics) {
		if (advanceX)     *advanceX = GetPointerTwoBytes_int16_Content(fontInfo->data + fontInfo->Spacing_AdvanceX + 4 * glyphIndex);
	}
	else {
		if (advanceX)     *advanceX = GetPointerTwoBytes_int16_Content(fontInfo->data + fontInfo->Spacing_AdvanceX + 4 * (numOfLongHorMetrics - 1));
	}
}
bool initTTFfontInfo(ttfBuildFontInfo*fontInfo,unsigned char*srcData, unsigned int offset)
{
	fontInfo->data = srcData;
	fontInfo->fontstart = offset;

	fontInfo->PlatForm_map = Truetype_find_tagName(srcData, offset,"cmap");//判断表
	fontInfo->headFormatMap = Truetype_find_tagName(srcData, offset, "head");//存放字符映射到字形索引位置表
	fontInfo->MaxCharNum = Truetype_find_tagName(srcData, offset, "maxp"); //存放多少个字符
	fontInfo->glyphCurve= Truetype_find_tagName(srcData, offset, "glyf");//存放字体字形曲线顶点的位置
	fontInfo->glyphCurve_loca= Truetype_find_tagName(srcData, offset, "loca");//存放字体字形索引的位置
	fontInfo->Spacing_AdvanceX=Truetype_find_tagName(srcData, offset, "hmtx");//各个字符间距 步进距离 左宽 右宽
	fontInfo->ascent_descent= Truetype_find_tagName(srcData, offset, "hhea");//上下标距
	unsigned int skipBytes = 4;
	if (fontInfo->MaxCharNum)//读取多少个字符
		fontInfo->numGlyphs = GetPointerTwoBytes_uint16_Content(srcData + fontInfo->MaxCharNum + skipBytes);
	else
		fontInfo->numGlyphs = 0xffff;
	skipBytes = 50;
	//读取转换的格式
	fontInfo->UnicodeIndex_To_glyphLocaFormat= GetPointerTwoBytes_uint16_Content(srcData+ fontInfo->headFormatMap+skipBytes);


	fontInfo->Unicode_localndexmap = 0;
	skipBytes = 8;
	fontInfo->Unicode_localndexmap = fontInfo->PlatForm_map + GetPointerFourBytes_uint32_Content(srcData+ fontInfo->PlatForm_map+skipBytes);//字符unicode 到字形索引映射表

	if (fontInfo->Unicode_localndexmap == 0)
		return false;
	return true;
}

 int ttfFontGetfontOffset_Index(unsigned char* srcData, unsigned int idx)
{

	if (truetype_tag(srcData, "ttcf")) {
		// version 1?
		if (GetPointerFourBytes_int32_Content(srcData + 4) == 0x00010000 || GetPointerFourBytes_int32_Content(srcData + 4) == 0x00020000) {
			truetype_int32 n = GetPointerFourBytes_int32_Content(srcData + 8);
			if (idx >= n)
				return 0;
			return GetPointerFourBytes_int32_Content(srcData + 12 + idx * 4);
		}
	}
	return 0;
}
void ttfFontGetVMetrics(ttfBuildFontInfo* fontInfo, int* ascent)
{
	if (ascent) *ascent = GetPointerTwoBytes_uint16_Content(fontInfo->data + fontInfo->ascent_descent + 4);
}