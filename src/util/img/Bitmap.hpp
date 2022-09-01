#ifndef HYPERION_V2_BITMAP_HPP
#define HYPERION_V2_BITMAP_HPP

#include <asset/ByteWriter.hpp>
#include <core/Containers.hpp>
#include <core/lib/String.hpp>
#include <core/lib/CMemory.hpp>
#include <math/Vector3.hpp>
#include <math/Vector4.hpp>
#include <util/ByteUtil.hpp>
#include <util/img/WriteBitmap.hpp>
#include <Types.hpp>

namespace hyperion::v2 {

template <UInt NumComponents>
struct Pixel
{
    static constexpr UInt byte_size = NumComponents;

    UByte bytes[byte_size];

    Pixel() = default;

    template <class Vector>
    Pixel(const Vector &color)
    {
        for (UInt i = 0; i < MathUtil::Min(byte_size, Vector::size); i++) {
            bytes[byte_size - i - 1] = static_cast<UByte>(color[i] * 255.0f);
        }
    }

    Pixel(const Pixel &other)
    {
        Memory::Copy(&bytes[0], &other.bytes[0], sizeof(bytes));
    }

    Pixel &operator=(const Pixel &other)
    {
        Memory::Copy(&bytes[0], &other.bytes[0], sizeof(bytes));

        return *this;
    }

    Pixel(Pixel &&other) noexcept
    {
        Memory::Copy(&bytes[0], &other.bytes[0], sizeof(bytes));
    }

    Pixel &operator=(Pixel &&other) noexcept
    {
        Memory::Copy(&bytes[0], &other.bytes[0], sizeof(bytes));

        return *this;
    }

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

class Bitmap
{
    using Pixel = Pixel<3>;

public:
    Bitmap(UInt width, UInt height)
        : m_width(width),
          m_height(height)
    {
        m_pixels.Resize(width * height);
    }

    UInt GetWidth() const
        { return m_width; }

    UInt GetHeight() const
        { return m_height; }

    Pixel &GetPixel(UInt x, UInt y)
    {
        const UInt index = ((x + m_width) % m_width)
                + (((m_height - y) + m_height) % m_height) * m_width;

        return m_pixels[index];
    }

    const Pixel &GetPixel(UInt x, UInt y) const
        { return const_cast<const Bitmap *>(this)->GetPixel(x, y); }

    Pixel &GetPixelAtIndex(UInt index)
        { return m_pixels[index]; }

    const Pixel &GetPixelAtIndex(UInt index) const
        { return m_pixels[index]; }

    void Write(const String &filepath)
    {
        DynArray<UByte> unpacked_bytes;
        unpacked_bytes.Resize(m_pixels.Size() * Pixel::byte_size);

        for (SizeType i = 0, j = 0; i < unpacked_bytes.Size(); i += Pixel::byte_size, j++) {
            for (UInt k = 0; k < Pixel::byte_size; k++) {
                unpacked_bytes[i + k] = m_pixels[j].bytes[k];
            }
        }

        WriteBitmap::Write(filepath.Data(), m_width, m_height, unpacked_bytes.Data());
    }

    void FlipVertical()
    {
        for (UInt x = 0; x < m_width; x++) {
            for (UInt y = 0; y < m_height / 2; y++) {
                auto temp = GetPixel(x, m_height - y - 1u);
                GetPixel(x, m_height - y - 1u) = GetPixel(x, y);
                GetPixel(x, y) = temp;
            }
        }
    }

    void FlipHorizontal()
    {
        for (UInt x = 0; x < m_width / 2; x++) {
            for (UInt y = 0; y < m_height; y++) {
                auto temp = GetPixel(m_width - x - 1u, y);
                GetPixel(m_width - x - 1u, y) = GetPixel(x, y);
                GetPixel(x, y) = temp;
            }
        }
    }

private:
    UInt m_width;
    UInt m_height;
    DynArray<Pixel> m_pixels;

};

} // namespace hyperion::v2

#endif
