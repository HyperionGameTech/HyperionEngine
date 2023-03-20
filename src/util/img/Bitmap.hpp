#ifndef HYPERION_V2_BITMAP_HPP
#define HYPERION_V2_BITMAP_HPP

#include <asset/ByteWriter.hpp>
#include <core/Containers.hpp>
#include <core/lib/String.hpp>
#include <math/Vector3.hpp>
#include <math/Vector4.hpp>
#include <util/ByteUtil.hpp>
#include <util/img/WriteBitmap.hpp>
#include <Types.hpp>

#include <cmath>

namespace hyperion::v2 {

template <UInt NumComponents>
struct Pixel
{
    static constexpr UInt byte_size = NumComponents;

    UByte bytes[byte_size];

    Pixel() = default;

    template <class ValueType, typename = std::enable_if_t<std::is_same_v<std::true_type, std::conditional_t<(NumComponents > 0), std::true_type, std::false_type>>>>
    Pixel(ValueType value)
    {
        for (int i = 0; i < NumComponents; i++) {
            if constexpr (std::is_same_v<ValueType, Float32>) {
                bytes[i] = static_cast<UByte>(value * 255.0f);
            } else if constexpr (std::is_same_v<ValueType, UByte>) {
                bytes[i] = value;
            } else {
                static_assert(resolution_failure<ValueType>, "Invalid type to pass to Pixel constructor");
            }
        }

    }

    template <class Vector, typename = std::enable_if_t<std::is_same_v<std::true_type, std::conditional_t<(NumComponents == Vector::size), std::true_type, std::false_type>>>>
    Pixel(const Vector &color)
    {
        for (UInt i = 0; i < MathUtil::Min(byte_size, Vector::size); i++) {
            bytes[byte_size - i - 1] = static_cast<UByte>(color[i] * 255.0f);
        }
    }

    Pixel(const Pixel &other) = default;
    Pixel &operator=(const Pixel &other) = default;
    Pixel(Pixel &&other) noexcept = default;
    Pixel &operator=(Pixel &&other) noexcept = default;
    ~Pixel() = default;

    Vector3 GetRGB() const
    {
        return Vector3(
            static_cast<Float>(bytes[2]) / 255.0f,
            static_cast<Float>(bytes[1]) / 255.0f,
            static_cast<Float>(bytes[0]) / 255.0f
        );
    }

    Vector4 GetRGBA() const
    {
        if constexpr (byte_size < 4) {
            return Vector4(
                static_cast<Float>(bytes[2]) / 255.0f,
                static_cast<Float>(bytes[1]) / 255.0f,
                static_cast<Float>(bytes[0]) / 255.0f,
                1.0f
            );
        } else {
            return Vector4(
                static_cast<Float>(bytes[3]) / 255.0f,
                static_cast<Float>(bytes[2]) / 255.0f,
                static_cast<Float>(bytes[1]) / 255.0f,
                static_cast<Float>(bytes[0]) / 255.0f
            );
        }
    }
};

template <UInt NumComponents>
class Bitmap {
    using Pixel = Pixel<NumComponents>;

public:
    Bitmap()
        : m_width(0u),
          m_height(0u)
    {
    }

    Bitmap(UInt width, UInt height)
        : m_width(width),
          m_height(height)
    {
        m_pixels.Resize(width * height);
    }

    Bitmap(const Bitmap &other)
        : m_width(other.m_width),
          m_height(other.m_height),
          m_pixels(other.m_pixels)
    {
    }

    Bitmap &operator=(const Bitmap &other)
    {
        m_width = other.m_width;
        m_height = other.m_height;
        m_pixels = other.m_pixels;

        return *this;
    }

    Bitmap(Bitmap &&other) noexcept
        : m_width(other.m_width),
          m_height(other.m_height),
          m_pixels(std::move(other.m_pixels))
    {
    }

    Bitmap &operator=(Bitmap &&other) noexcept
    {
        m_width = other.m_width;
        m_height = other.m_height;
        m_pixels = std::move(other.m_pixels);

        return *this;
    }

    ~Bitmap() = default;

    HYP_PACK_BEGIN
    struct Header {
        // Bitmap signature 'B', 'M'
        UInt16 signature;
        UInt file_size;
        UInt _reserved;
        UInt data_start;
    }
    HYP_PACK_END;

    HYP_PACK_BEGIN
    struct Info {
        UInt header_size;
        UInt width;
        UInt height;
        /* Number of colour planes */
        UInt16 color_planes;
        UInt16 bits_per_pixel;

        /* Compression parameters */
        UInt compression;
        UInt image_size;  /* Ignored without compression */

        /* Image resolution */
        UInt horizontal_resolution;
        UInt vertical_resolution;

        /* Colour palette */
        UInt color_count; /* zero defaults to 2^n */
        UInt important_colors; /* Number of important colours used, zero is every colour. */
    }
    HYP_PACK_END;


    UInt GetWidth() const
    { return m_width; }

    UInt GetHeight() const
    { return m_height; }

    UInt GetStride() const
    { return m_width * NumComponents; }

    SizeType GetByteSize() const
    {
        return static_cast<SizeType>(m_width)
               * static_cast<SizeType>(m_height)
               * static_cast<SizeType>(NumComponents);
    }

    Pixel &GetPixel(UInt x, UInt y)
    {
        const UInt index = ((x + m_width) % m_width)
                           + (((m_height - y) + m_height) % m_height) * m_width;

        return m_pixels[index];
    }

    constexpr Int GetPaddedPixelSize(int width)
    {
        // width is already a multiple of 4
        if (width % 4 == 0)
            return width;

        // get next multiple
        return (width - (width % 4)) + 4;
    }

    const Pixel &GetPixel(UInt x, UInt y) const
    { return const_cast<const Bitmap *>(this)->GetPixel(x, y); }

    void SetPixelsFromMemory(UInt stride, UByte *buffer, SizeType pixel_count)
    {
        AssertThrowMsg((m_pixels.Size() >= (pixel_count)), "Pixel buffer size not large enough or component mismatch");
        AssertThrowMsg((pixel_count % stride) == 0, "Pixel buffer size is not divisible by bitmap stride!\n");

        const SizeType rows = pixel_count / stride;

        for (SizeType row = 0; row < rows; row++) {
            for (SizeType column = 0; column < stride * 3; column += NumComponents) {
                const SizeType index = row * stride + column;
                Int p_index = 0;
                for (p_index = 0; p_index < NumComponents; p_index++) {
                    m_pixels[index].bytes[p_index] = buffer[index + p_index];
                }
                for (; p_index < 4 - NumComponents; p_index++) {
                    m_pixels[index].bytes[p_index] = 0;
                }
            }
        }
    }

    Header CreateHeader(int padding)
    {
        const UInt stride = GetStride();

        AssertThrow((sizeof(Header) == 14));

        Header header;
        header.signature = (('M' << 8) | 'B');
        header._reserved = 0;
        header.file_size = sizeof(Header) + sizeof(Info) + ((padding) * m_height);
        header.data_start = sizeof(Header) + sizeof(Info);

        return header;
    }

    Info CreateInfo()
    {
        Info info;
        /* size of info header, located after bootstart header */
        info.header_size = sizeof(Info);
        AssertThrow((info.header_size == 40));
        info.width = m_width;
        info.height = m_height;
        info.color_planes = 1;
        info.bits_per_pixel = NumComponents * 8;
        info.compression = 0;
        info.image_size = 0;
        info.horizontal_resolution = 0;
        info.vertical_resolution = 0;
        info.color_count = 0;
        info.important_colors = 0;

        DebugLog(LogType::Debug, "Bits per pixel: %d\n", info.bits_per_pixel);

        return info;
    }

    void SetColourTable(const ByteBuffer &colors)
    {
        m_colors = colors;
    }

    ByteBuffer GenerateColorRamp()
    {
        const int bits_per_pixel = NumComponents * 8;
        SizeType size = std::pow<SizeType>(2, bits_per_pixel) - 1;
        ByteBuffer buffer(size * 4);

        for (SizeType i = 0; i < size; i++) {
            buffer.GetInternalArray().Set(i * 3, 255-i);
            buffer.GetInternalArray().Set(i * 3 + 1, 255-i);
            buffer.GetInternalArray().Set(i * 3 + 2, 255-i);
            buffer.GetInternalArray().Set(i * 3 + 3, 0);
        }
        return buffer;
    }

    void Write(const String &filepath)
    {
        Array<UByte> data;
        GetUnpackedBytes(data);

        UInt8 padding[] = { 0, 0, 0 };
        Int padding_size = GetPaddedPixelSize(GetStride());
        DebugLog(LogType::Debug, "Padded pixel size: %d\n", padding_size);

        Int bitmap_size = m_height * padding_size * NumComponents;

        Header header = CreateHeader(padding_size);
        Info info = CreateInfo();

        // We have a valid colour table, set our info table to contain it
        if (m_colors.Size() > 0) {
            info.color_count = m_colors.Size() / 4;
            DebugLog(LogType::Debug, "ColorCount: %d\n", info.color_count);
            info.important_colors = 0;

            // our data will start later now, adjust our index accordingly
            header.data_start += m_colors.Size();
            header.file_size += m_colors.Size();
        }

        std::FILE *image_fp = std::fopen(filepath.Data(), "wb");

        AssertThrowMsg((image_fp), "Could not open image file for writing!\n");

        AssertThrow((sizeof(Header)) == 14);
        AssertThrow((sizeof(Info)) == 40);
        std::fwrite((UInt8 *)&header, 1, sizeof(Header), image_fp);
        std::fwrite((UInt8 *)&info, 1, sizeof(Info), image_fp);

        // We have a valid colour table to be written
        if (m_colors.Size() > 0) {
            std::fwrite(m_colors.Data(), 1, m_colors.Size(), image_fp);
        }

        for (UInt index = 0; index < m_height; index++) {
            std::fwrite(data.Data() + ((m_height - index - 1) * GetStride()), NumComponents, m_width, image_fp);
            //std::fwrite(padding, 1, padding_size, image_fp);
        }

        std::fclose(image_fp);
    }

    void GetUnpackedBytes(Array<UByte> &out)
    {
        out.Resize(m_pixels.Size() * NumComponents);

        for (SizeType i = 0, j = 0; i < out.Size() && j < m_pixels.Size(); i += Pixel::byte_size, j++) {
            for (UInt k = 0; k < NumComponents; k++) {
                out[i + k] = m_pixels[j].bytes[k];
            }
        }
    }

    void GetUnpackedFloats(Array<Float> &out)
    {
        out.Resize(m_pixels.Size() * NumComponents);

        for (SizeType i = 0, j = 0; i < out.Size() && j < m_pixels.Size(); i += Pixel::byte_size, j++) {
            for (UInt k = 0; k < NumComponents; k++) {
                out[i + k] = static_cast<Float>(m_pixels[j].bytes[k]) / 255.0f;
            }
        }
    }
private:

    UInt m_width;
    UInt m_height;
    Array<Pixel> m_pixels;
    ByteBuffer m_colors;
};

} // namespace hyperion::v2

#endif
