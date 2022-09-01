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

    Pixel &GetPixel(UInt x, UInt y)
    {
        const UInt index = ((x + m_width) % m_width)
                + (((m_height - y) + m_height) % m_height) * m_width;

        return m_pixels[index];
    }

    const Pixel &GetPixel(UInt x, UInt y) const
        { return const_cast<const Bitmap *>(this)->GetPixel(x, y); }

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

private:
    UInt m_width;
    UInt m_height;
    DynArray<Pixel> m_pixels;

};

} // namespace hyperion::v2

#endif
