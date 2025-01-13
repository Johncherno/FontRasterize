#pragma once
#include<iostream>
//truetype��������
typedef unsigned char truetype_uint8;
typedef signed char truetype_int8;
typedef unsigned short truetype_uint16;
typedef signed short truetype_int16;
typedef unsigned int truetype_uint32;
typedef signed int truetype_int32;

truetype_uint16 GetPointerTwoBytes_uint16_Content(truetype_uint8* p) { return (p[0]<<8) | p[1]; }//���ֽںϲ����ݲ���
 truetype_int16 GetPointerTwoBytes_int16_Content(truetype_uint8* p) { return (p[0] << 8) | p[1]; }
 truetype_uint32 GetPointerFourBytes_uint32_Content(truetype_uint8* p) { return (p[0] <<24) |(p[1]<<16)|(p[2]<<8)|p[3]; }
 truetype_int32 GetPointerFourBytes_int32_Content(truetype_uint8* p) { return (p[0] << 24)| (p[1] << 16) | (p[2] << 8)|p[3]; }

#define tag4(p,t0,t1,t2,t3)  ((p)[0]==(t0)&&(p)[1]==(t1)&&(p)[2]==t2&&(p)[3]==(t3))
#define truetype_tag(p,str) tag4(p,str[0],str[1],str[2],str[3])

struct ttfBuildFontInfo
{
	unsigned char* data;//ttf�ļ��ĵ�ַ
	int fontstart;//���忪ʼ��λ��
	unsigned int numGlyphs;//���ٸ��ַ�

	unsigned int headFormatMap, Spacing_AdvanceX, ascent_descent, MaxCharNum, Unicode_localndexmap, glyphCurve_loca, glyphCurve,PlatForm_map;
	unsigned int UnicodeIndex_To_glyphLocaFormat;
	//....................
};//ttf������Ϣ
truetype_int32 Truetype_find_tagName(unsigned char*data,unsigned int fontstart,const char*tag)
{
	const unsigned int scalar_Bytes = 4, num_tables_Bytes = 2, searchRange_Bytes = 2, entryselector_Bytes = 2, rangeshift_bytes = 2,tag_nameBytes=4,CheckSumByte=4,singleTableInfoBytes=16;
	unsigned int num_tables = GetPointerTwoBytes_uint16_Content(data + fontstart + scalar_Bytes);
	unsigned int tabledir = fontstart + scalar_Bytes+ num_tables_Bytes+ searchRange_Bytes+ entryselector_Bytes+ rangeshift_bytes;//��Ҫ����4���ֽڵ�scalarֵ ����numtablesֵ���ֽ��� searchRange��2���ֽ��� entryselector��2���ֽ��� rangeshift��2���ֽ���
	
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

	unsigned short format = GetPointerTwoBytes_uint16_Content(fontInfo->data + fontInfo->Unicode_localndexmap);//�����������������ָ�ʽ��
	//���������ʽ���ж�
	// Apple��ʽ
	//  | Offset | ���� | ����                          |
	//	|---------- - | ------------------------ | ------------------------------ - |
	//	| +0 | format = 0 | ��ʽ��                       |
	//	| +2 | tablelength| ��ĳ���                     |
	//	| +4 | language   | ���� ID                      |
	//	| +6 | glyphIdArray[256] | ÿ���ַ���Ӧ����������    ���ַ��������� ��Ϊ��A���ַ���Ϊ65 glyphIdArray[65]�����ݾ�Ϊ������������ֵ
	if (format==0)
	{
		unsigned int tablelength = GetPointerTwoBytes_uint16_Content(fontInfo->data + fontInfo->Unicode_localndexmap+2);//��Ҫ�����洢��ʽ�ŵ��ֽ�
		if (CharCodePoint< tablelength-6)//-6����Ϊ��Ҫȥ�� ��ʽ�ŵĴ洢ֵ�ĳ��ȡ��洢���ȵ�ֵ���ȡ�����iD��ֵ���� 2+2+2=6
		{
			return (fontInfo->data + fontInfo->Unicode_localndexmap + 6)[CharCodePoint];
		}
		return 0;
	}
	/*    | Offset | ���� | ����                          |
		|---------- - | ---------------------- | ------------------------------ - |
		| +0 | format = 6 | ��ʽ��                       |
		| +2 | length | ��ĳ���                     |
		| +4 | language | ���� ID                      |
		| +6 | firstCode | ��Χ��ʼ Unicode ���        |
		| +8 | entryCount | ��Χ�ڵ��ַ�����             |
		| +10 | glyphIdArray | ��Χ�ڵ������������� |
		+0x0A | glyphIdArray[0]->GlyphIndex(firstCode)
		+ 0x0C | glyphIdArray[1]->GlyphIndex(firstCode + 1)
		...
		+ N | glyphIdArray[N]->GlyphIndex(firstCode + N)*/
	else if (format == 6)
	{
		unsigned int firstCode = GetPointerTwoBytes_uint16_Content(fontInfo->data + fontInfo->Unicode_localndexmap +6);
		unsigned int Num_Code= GetPointerTwoBytes_uint16_Content(fontInfo->data + fontInfo->Unicode_localndexmap + 8);
		if (firstCode<=CharCodePoint&& CharCodePoint< firstCode+ Num_Code)//��� unicode_codepoint �Ƿ��� [firstCode, firstCode + Num_Code) ��Χ�ڣ�Ȼ�����ƫ����������������
		{
			return GetPointerTwoBytes_uint16_Content(fontInfo->data + fontInfo->Unicode_localndexmap + 10+(CharCodePoint- firstCode)*2);
		}
		return 0;
	}
	/*  | Offset | ���� | ����                          |
		|---------- - | ---------------------- | ------------------------------ - |
		| +0 | format = 12 / 13 | ��ʽ��                       |
		| +2 | reserved | �����ֶ�                     |
		| +4 | length | ��ĳ���                     |
		| +8 | language | ���� ID                      |
		| +12 | nGroups | ��������                     |
		| +16 | groups[nGroups] | ÿ���������ʼ�ͽ������ |*/
     //memory:
     //	+0x00 | format : 12
     //		+ 0x04 | reserved
     //		+ 0x08 | length : ����ܳ���
     //		+ 0x0C | language : ���� ID
     //		+ 0x10 | nGroups : ��������
     //		+ 0x14 | Group[0] -> {startCharCode, endCharCode, startGlyphId}
     //	+ 0x20 | Group[1] -> {startCharCode, endCharCode, startGlyphId}
     //	...
	else if (format == 12|| format == 13)
	{
		const unsigned int group_bytes = 12,startCharCodeOffset=0,endCharCodeOffset=4,startCharglyfIdOffset=8;
		//��ʼ���ֲ���
		unsigned int nGroups = GetPointerFourBytes_uint32_Content(fontInfo->data + fontInfo->Unicode_localndexmap+12);//������{��ʼunicode ����unicode ��ʼunicode����������}
		unsigned int low=0, high= nGroups;
		while (low<high)//ֻҪlow������С��high������
		{
			unsigned int  Middle = low + (high - low) / 2;//�м�����
			unsigned int startUnicode = GetPointerFourBytes_uint32_Content(fontInfo->data + fontInfo->Unicode_localndexmap + 12+Middle*group_bytes+startCharCodeOffset);//(fontInfo->data + fontInfo->Unicode_localndexmap+12)[Middle].startCharCode
			unsigned int endUnicode= GetPointerFourBytes_uint32_Content(fontInfo->data + fontInfo->Unicode_localndexmap + 12 + Middle * group_bytes + endCharCodeOffset);//(fontInfo->data + fontInfo->Unicode_localndexmap+12)[Middle].endCharCode
			if (CharCodePoint < startUnicode)//���С˵�������
			{
				high = Middle;//��ʱ��ΧҪ�ڣ�group[0]-group[0+end/2]��
			}
			else if (CharCodePoint> endUnicode)//�����˵�����ұ�
			{
				low = Middle + 1;//��ʱ��ΧҪ�ڣ�group[0+end/2+1]-group[end]��
			}
			else//˵���������Χ�� �ҳ������Χ�п�ʼunicode���������� �������Χ����������unicode����������
			{
				unsigned int startUnicode_glyphId= GetPointerFourBytes_uint32_Content(fontInfo->data + fontInfo->Unicode_localndexmap + 12 + Middle * group_bytes + startCharglyfIdOffset);//(fontInfo->data + fontInfo->Unicode_localndexmap+12)[Middle].startGlyphId
				if (format == 12)
					return startUnicode_glyphId + CharCodePoint - startUnicode;
				else//13
					return startUnicode_glyphId;
			}
		}
	}
	/*| Offset | ���� | ����                          |
		|---------- - | ---------------------- | ------------------------------ - |
		| +0 | format = 4 | ��ʽ��                       |
		| +2 | length | ��ĳ���                     |
		| +4 | language | ���� ID                      |
		| +6 | segCountX2 | ������ * 2                   |
		| +8 | searchRange | ������Χ                     |
		| +10 | entrySelector | ���������ĵ�������           |
		| +12 | rangeShift | ������Χƫ����               |
		| +14 | endCode[segCount] | ÿ���εĽ������             |
		| +XX | reservedPad | �������                     |
		| +XX | startCode[segCount] | ÿ���ε���ʼ���             |
		| +XX | idDelta[segCount] | ƫ��������                   |
		| +XX | idRangeOffset[segCount] | ƫ�Ʊ����� |*/
  //   memory:
	 //    +0x00 | format : 4
		//+ 0x02 | length : ����ܳ���
		//+ 0x04 | language : ���� ID
		//+ 0x06 | segCountX2 : ������ * 2
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
		//+ offset | locaId[i]{ startCode ,endCode }->locaӳ������� iΪ�ڼ����startcode-endcode
	else if (format == 4) { // standard mapping for windows fonts: binary search collection of ranges
		const unsigned int reservedPad_ValueBytes = 2;
		const unsigned int Bytes = 2;//�����������ֽ���
		truetype_uint16 segcount = GetPointerTwoBytes_uint16_Content(fontInfo->data + fontInfo->Unicode_localndexmap + 6) >> 1;
		truetype_uint16 searchRange = GetPointerTwoBytes_uint16_Content(fontInfo->data + fontInfo->Unicode_localndexmap + 8) >> 1;
		truetype_uint16 loopCount = GetPointerTwoBytes_uint16_Content(fontInfo->data + fontInfo->Unicode_localndexmap + 10);//���ѭ����������
		truetype_uint16 rangeShift = GetPointerTwoBytes_uint16_Content(fontInfo->data + fontInfo->Unicode_localndexmap + 12) >> 1;//������Χ��������
		// do a binary search of the segments
		const unsigned int baseOffset = fontInfo->Unicode_localndexmap + 14;//��׼ƫ��
		truetype_uint32 search = baseOffset;//search��������ָ��

		if (CharCodePoint > 0xffff)
			return 0;
		//���ַ�Ѱ��
		// they lie from endCount .. endCount + segCount
		// but searchRange is the nearest power of two, so...
		if (CharCodePoint >= GetPointerTwoBytes_uint16_Content(fontInfo->data + baseOffset + rangeShift * Bytes))
			search += rangeShift * Bytes;

		search -= Bytes;
		while (loopCount) {
			truetype_uint16 end;
			searchRange >>= 1;
			end = GetPointerTwoBytes_uint16_Content(fontInfo->data + search + searchRange * Bytes);
			if (CharCodePoint > end)//���������ַ������һ��Χ�Ľ����뻹�� �Ǿ�˵���ں�����ƥ��� searchָ�������ƶ���£Ŀ������
				search += searchRange * Bytes;//�������unicode��С��ô������Χ��ͣ��Сһֱ��ȡsearch+������Χ��endֱֵ���ƽ����unicode����search�������search���������С�շ�Χ    �������unicode�ܴ���ôsearch��ͣ�����Ʊƽ���
			--loopCount;
		}
		search += Bytes;//����search�ټ���Bytes����Ϊ����֮ǰ���ŵ��Ǳ���С��unicode �������������������
		
		{
			truetype_uint16 offset, startCharUnicode,endCharUnicode;
			truetype_uint16 index = (truetype_uint16)((search - baseOffset) / Bytes);//endcount����fontInfo->Unicode_localndexmap + 14 index�൱�ڴ������鷶Χ��ƫ������ >>1�൱�ڳ���ÿ���ṹ�����ĳ���Ϊ2���ֽ�

			startCharUnicode = GetPointerTwoBytes_uint16_Content(fontInfo->data + baseOffset+ segcount * Bytes*1 + reservedPad_ValueBytes + Bytes * index);
			endCharUnicode =   GetPointerTwoBytes_uint16_Content(fontInfo->data + baseOffset+ Bytes * index);
			if (CharCodePoint< startCharUnicode || CharCodePoint> endCharUnicode)
				return 0;

			offset = GetPointerTwoBytes_uint16_Content(fontInfo->data + baseOffset + segcount * Bytes*3 + reservedPad_ValueBytes + Bytes * index);
			if (offset == 0)
				return (truetype_uint16)(CharCodePoint + GetPointerTwoBytes_int16_Content(fontInfo->data + baseOffset + segcount * Bytes * 2 + reservedPad_ValueBytes + Bytes * index));
			//�����׼ƫ��baseOffset  ������endcode startcode iddelta���ֶκͶ����ֽ�segcount * Bytes * 3 + reservedPad_ValueBytes  �ٵ�ƫ������ľ������� ��ƫ��offset���洢���unicode��Χ��loca���������������unicode�뿪ʼstartunicode��ֵ�ҳ�����
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
		g1 = fontInfo->glyphCurve + GetPointerTwoBytes_uint16_Content(fontInfo->data + fontInfo->glyphCurve_loca + idx * TwoBytes) * TwoBytes;//����2��Ŀ������Ϊ��ʱ��ʽ�����ε�λ������2���ֽڴ洢��
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
	else//yֵ�ڸ�����
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

	fontInfo->PlatForm_map = Truetype_find_tagName(srcData, offset,"cmap");//�жϱ�
	fontInfo->headFormatMap = Truetype_find_tagName(srcData, offset, "head");//����ַ�ӳ�䵽��������λ�ñ�
	fontInfo->MaxCharNum = Truetype_find_tagName(srcData, offset, "maxp"); //��Ŷ��ٸ��ַ�
	fontInfo->glyphCurve= Truetype_find_tagName(srcData, offset, "glyf");//��������������߶����λ��
	fontInfo->glyphCurve_loca= Truetype_find_tagName(srcData, offset, "loca");//�����������������λ��
	fontInfo->Spacing_AdvanceX=Truetype_find_tagName(srcData, offset, "hmtx");//�����ַ���� �������� ��� �ҿ�
	fontInfo->ascent_descent= Truetype_find_tagName(srcData, offset, "hhea");//���±��
	unsigned int skipBytes = 4;
	if (fontInfo->MaxCharNum)//��ȡ���ٸ��ַ�
		fontInfo->numGlyphs = GetPointerTwoBytes_uint16_Content(srcData + fontInfo->MaxCharNum + skipBytes);
	else
		fontInfo->numGlyphs = 0xffff;
	skipBytes = 50;
	//��ȡת���ĸ�ʽ
	fontInfo->UnicodeIndex_To_glyphLocaFormat= GetPointerTwoBytes_uint16_Content(srcData+ fontInfo->headFormatMap+skipBytes);


	fontInfo->Unicode_localndexmap = 0;
	skipBytes = 8;
	fontInfo->Unicode_localndexmap = fontInfo->PlatForm_map + GetPointerFourBytes_uint32_Content(srcData+ fontInfo->PlatForm_map+skipBytes);//�ַ�unicode ����������ӳ���

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