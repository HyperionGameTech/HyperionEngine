#include <util/img/WriteBitmap.hpp>

#include <core/io/ByteWriter.hpp>

#include <stdio.h>

namespace hyperion {

static constexpr int BytesPerPixel = 3;
static constexpr int FileHeaderSize = 14;
static constexpr int InfoHeaderSize = 40;

void GenerateBitmapImage(unsigned char* image, int height, int width, char* imageFileName);
unsigned char* CreateBitmapFileHeader(int height, int stride);
unsigned char* CreateBitmapInfoHeader(int height, int width);

unsigned char* CreateBitmapFileHeader(int height, int stride)
{
    int fileSize = FileHeaderSize + InfoHeaderSize + (stride * height);

    static unsigned char fileHeader[] = {
        0, 0,       /// signature
        0, 0, 0, 0, /// image file size in bytes
        0, 0, 0, 0, /// reserved
        0, 0, 0, 0, /// start of pixel array
    };

    fileHeader[0] = (unsigned char)('B');
    fileHeader[1] = (unsigned char)('M');
    fileHeader[2] = (unsigned char)(fileSize);
    fileHeader[3] = (unsigned char)(fileSize >> 8);
    fileHeader[4] = (unsigned char)(fileSize >> 16);
    fileHeader[5] = (unsigned char)(fileSize >> 24);
    fileHeader[10] = (unsigned char)(FileHeaderSize + InfoHeaderSize);

    return fileHeader;
}

unsigned char* CreateBitmapInfoHeader(int height, int width)
{
    static unsigned char infoHeader[] = {
        0, 0, 0, 0, /// header size
        0, 0, 0, 0, /// image width
        0, 0, 0, 0, /// image height
        0, 0,       /// number of color planes
        0, 0,       /// bits per pixel
        0, 0, 0, 0, /// compression
        0, 0, 0, 0, /// image size
        0, 0, 0, 0, /// horizontal resolution
        0, 0, 0, 0, /// vertical resolution
        0, 0, 0, 0, /// colors in color table
        0, 0, 0, 0, /// important color count
    };

    infoHeader[0] = (unsigned char)(InfoHeaderSize);
    infoHeader[4] = (unsigned char)(width);
    infoHeader[5] = (unsigned char)(width >> 8);
    infoHeader[6] = (unsigned char)(width >> 16);
    infoHeader[7] = (unsigned char)(width >> 24);
    infoHeader[8] = (unsigned char)(height);
    infoHeader[9] = (unsigned char)(height >> 8);
    infoHeader[10] = (unsigned char)(height >> 16);
    infoHeader[11] = (unsigned char)(height >> 24);
    infoHeader[12] = (unsigned char)(1);
    infoHeader[14] = (unsigned char)(BytesPerPixel * 8);

    return infoHeader;
}

bool WriteBitmap::Write(
    ByteWriter* byteWriter,
    int width,
    int height,
    unsigned char* bytes)
{
    if (!byteWriter)
    {
        return false;
    }

    int widthInBytes = width * BytesPerPixel;

    unsigned char padding[3] = { 0, 0, 0 };
    int paddingSize = (4 - (widthInBytes) % 4) % 4;

    int stride = (widthInBytes) + paddingSize;

    unsigned char* fileHeader = CreateBitmapFileHeader(height, stride);
    byteWriter->Write(fileHeader, FileHeaderSize);

    unsigned char* infoHeader = CreateBitmapInfoHeader(height, width);
    byteWriter->Write(infoHeader, InfoHeaderSize);

    int i;
    for (i = 0; i < height; i++)
    {
        byteWriter->Write(bytes + (i * widthInBytes), BytesPerPixel * width);
        byteWriter->Write(padding, paddingSize);
    }

    byteWriter->Close();

    return true;
}
} // namespace hyperion
