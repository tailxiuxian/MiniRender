#include <stdio.h>
#include <stdlib.h> 
#include "bmpReader.h"

#define BITMAPFILEHEADERLENGTH 14
#define BM 19778
#define NORMAL_BFOFFSET 54

bool bmpFileTest(FILE* fpbmp);
void bmpFileHeader(FILE* fpbmp, unsigned int& bfOffBits);
void bmpInfoHeader(FILE* fpbmp, long& width, long& height, unsigned short& bitCount);
void bmpDataPart(FILE* fpbmp, unsigned int offset, long width, long height, unsigned int* pPixelData, unsigned short bitPerPixel);

unsigned int* read_bmp(const char* szPath, long& width, long& height)
{
	FILE *fpbmp = fopen(szPath, "r");
	if (NULL == fpbmp)
	{
		printf("%s:bmp file not exist\n",__FUNCTION__);
		return NULL;
	}

	if (bmpFileTest(fpbmp) == false)
	{
		printf("%s:file is not bmp file\n", __FUNCTION__);
		fclose(fpbmp);
		return NULL;
	}

	unsigned int bfOffBits = 0;
	bmpFileHeader(fpbmp, bfOffBits);
	if (bfOffBits != NORMAL_BFOFFSET) // 有调色板 要另外处理
	{
		return NULL;
	}

	unsigned short bitCount = 0;
	bmpInfoHeader(fpbmp, width, height, bitCount);
	if (bitCount == 0)
	{
		return NULL;
	}

	unsigned short bitPerPixel = bitCount / 8;

	unsigned int* pPixelImg = new unsigned int[width * height];
	if (NULL == pPixelImg)
	{
		return NULL;
	}

	bmpDataPart(fpbmp,bfOffBits,width,height,pPixelImg,bitPerPixel);

	fclose(fpbmp);
	return pPixelImg;
}

bool bmpFileTest(FILE* fpbmp)
{
	unsigned short bfType = 0;
	fseek(fpbmp, 0L, SEEK_SET);
	fread(&bfType, sizeof(char), 2, fpbmp);
	if (BM != bfType)
	{
		return false;
	}
	return true;
}

void bmpFileHeader(FILE* fpbmp, unsigned int& bfOffBits)
{
	unsigned short bfType; 
	unsigned int   bfSize; 
	unsigned short bfReserved1;  
	unsigned short bfReserved2;   

	fseek(fpbmp, 0L, SEEK_SET);

	fread(&bfType, sizeof(char), 2, fpbmp);
	fread(&bfSize, sizeof(char), 4, fpbmp);
	fread(&bfReserved1, sizeof(char), 2, fpbmp);
	fread(&bfReserved2, sizeof(char), 2, fpbmp);
	fread(&bfOffBits, sizeof(char), 4, fpbmp);
}

void bmpInfoHeader(FILE* fpbmp, long& width, long& height, unsigned short& bitCount)
{
	unsigned int biSize;             // DWORD        biSize;  
	unsigned short biPlanes;               // WORD         biPlanes;  
	unsigned int biCompression;          // DWORD        biCompression;  
	unsigned int biSizeImage;            // DWORD        biSizeImage;  
	long         biXPelsPerMerer;        // LONG         biXPelsPerMerer;  
	long         biYPelsPerMerer;        // LONG         biYPelsPerMerer;  
	unsigned int biClrUsed;              // DWORD        biClrUsed;  
	unsigned int biClrImportant;         // DWORD        biClrImportant;  

	fseek(fpbmp, 14L, SEEK_SET);

	fread(&biSize, sizeof(char), 4, fpbmp);
	fread(&width, sizeof(char), 4, fpbmp);
	fread(&height, sizeof(char), 4, fpbmp);
	fread(&biPlanes, sizeof(char), 2, fpbmp);
	fread(&bitCount, sizeof(char), 2, fpbmp);
	fread(&biCompression, sizeof(char), 4, fpbmp);
	fread(&biSizeImage, sizeof(char), 4, fpbmp);
	fread(&biXPelsPerMerer, sizeof(char), 4, fpbmp);
	fread(&biYPelsPerMerer, sizeof(char), 4, fpbmp);
	fread(&biClrUsed, sizeof(char), 4, fpbmp);
	fread(&biClrImportant, sizeof(char), 4, fpbmp);
}

void bmpDataPart(FILE* fpbmp, unsigned int offset, long width, long height, unsigned int* pPixelData, unsigned short bitPerPixel)
{
	int i, j, k;
	fseek(fpbmp, offset, SEEK_SET);

	unsigned char* pBitMap = new unsigned char[width * height * bitPerPixel];
	if (NULL == pBitMap)
	{
		return;
	}
	fread(pBitMap, sizeof(char), width * height * bitPerPixel, fpbmp);

	for (i = 0; i < height; i++)
	{
		for (j = 0; j < width; j++)
		{
			int iBitStartIndex = (i * width + j) * bitPerPixel;
			unsigned char bitBlue = pBitMap[iBitStartIndex];
			unsigned char bitGreen = pBitMap[iBitStartIndex+1];
			unsigned char bitRed = pBitMap[iBitStartIndex+2];
			unsigned char bitAlpha = 0xFF;
			if (bitPerPixel == 4)
			{
				bitAlpha = pBitMap[iBitStartIndex + 3];
			}

			//fread(&bitBlue, sizeof(char), 1, fpbmp);
			//fread(&bitGreen, sizeof(char), 1, fpbmp);
			//fread(&bitRed, sizeof(char), 1, fpbmp);
			//if (bitPerPixel == 4)
			//{
			//	fread(&bitAlpha, sizeof(char), 1, fpbmp);
			//}

			int PixelIndex = i * width + j;
			pPixelData[PixelIndex] = bitRed;
			pPixelData[PixelIndex] = (pPixelData[PixelIndex] << 8) + bitGreen;
			pPixelData[PixelIndex] = (pPixelData[PixelIndex] << 8) + bitBlue;
			pPixelData[PixelIndex] = (pPixelData[PixelIndex] << 8) + bitAlpha;
			
			//if (bitBlue != 0 || bitGreen != 0 || bitRed != 0)
			//{
			//	printf("R: %d G: %d B: %d A: %d\n", bitRed, bitGreen, bitBlue, bitAlpha);
			//}
		}
	}

	delete[] pBitMap;
}