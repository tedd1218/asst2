#ifndef CAMERA_PIPE_IMAGE_H
#define CAMERA_PIPE_IMAGE_H

#include "CoreLib/LibIO.h"

using namespace CoreLib::Basic;
using namespace CoreLib::IO;

union Color4ub
{
	struct
	{
		unsigned char r, g, b, a;
	};
	unsigned int color;
};

class Image
{
public:
	int Width;
	int Height;
	List<Color4ub> Pixels;
	Image()
	{
		Width = Height = 0;
	}
	void Save(const String & fileName)
	{
		FILE *f = 0;
		int filesize = 54 + 3 * Width*Height;
		List<unsigned char> img;
		img.SetSize(3 * Width*Height);
		memset(img.Buffer(), 0, 3 * Width*Height);
		for (int j = 0; j<Height; j++)
		{
			auto * scanLine = Pixels.Buffer() + j*Width;
			for (int i = 0; i<Width; i++)
			{
				int x = i;
				int y = (Height - 1) - j;
				img[(x + y*Width) * 3 + 2] = scanLine[i].r;
				img[(x + y*Width) * 3 + 1] = scanLine[i].g;
				img[(x + y*Width) * 3 + 0] = scanLine[i].b;
			}
		}

		unsigned char bmpfileheader[14] = { 'B', 'M', 0, 0, 0, 0, 0, 0, 0, 0, 54, 0, 0, 0 };
		unsigned char bmpinfoheader[40] = { 40, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 24, 0 };
		unsigned char bmppad[3] = { 0, 0, 0 };

		bmpfileheader[2] = (unsigned char)(filesize);
		bmpfileheader[3] = (unsigned char)(filesize >> 8);
		bmpfileheader[4] = (unsigned char)(filesize >> 16);
		bmpfileheader[5] = (unsigned char)(filesize >> 24);

		bmpinfoheader[4] = (unsigned char)(Width);
		bmpinfoheader[5] = (unsigned char)(Width >> 8);
		bmpinfoheader[6] = (unsigned char)(Width >> 16);
		bmpinfoheader[7] = (unsigned char)(Width >> 24);
		bmpinfoheader[8] = (unsigned char)(Height);
		bmpinfoheader[9] = (unsigned char)(Height >> 8);
		bmpinfoheader[10] = (unsigned char)(Height >> 16);
		bmpinfoheader[11] = (unsigned char)(Height >> 24);

		fopen_s(&f, fileName.ToMultiByteString(), "wb");
		if (f)
		{
			fwrite(bmpfileheader, 1, 14, f);
			fwrite(bmpinfoheader, 1, 40, f);
			for (int i = 0; i<Height; i++)
			{
				fwrite(img.Buffer() + (Width*i * 3), 3, Width, f);
				fwrite(bmppad, 1, (4 - (Width * 3) % 4) % 4, f);
			}
			fclose(f);
		}
		else
		{
			throw IOException(L"Failed to open file for writing the bitmap.");
		}
	}
};

#endif