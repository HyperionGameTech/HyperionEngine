#ifndef HYPERION_V2_BITMAP_HPP
#define HYPERION_V2_BITMAP_HPP

#include <asset/ByteWriter.hpp>
#include <core/Containers.hpp>
#include <core/lib/String.hpp>
#include <math/Vector3.hpp>
#include <math/Vector4.hpp>
#include <math/MathUtil.hpp>
#include <util/ByteUtil.hpp>
#include <util/img/WriteBitmap.hpp>
#include <Types.hpp>

namespace hyperion::v2 {

template <UInt NumComponents>
struct Pixel
{
    static constexpr UInt byte_size = NumComponents > 1 ? NumComponents : 1;

    UByte bytes[byte_size];

    Pixel() = default;

    void SetR(Float r)
    {
        bytes[0] = static_cast<UByte>(r * 255.0f);
    }

    void SetRG(Float r, Float g)
    {
        bytes[0] = static_cast<UByte>(r * 255.0f);

        if constexpr (byte_size >= 2) {
            bytes[1] = static_cast<UByte>(g * 255.0f);
        }
    }

    void SetRGB(Float r, Float g, Float b)
    {
        bytes[0] = static_cast<UByte>(r * 255.0f);

        if constexpr (byte_size >= 2) {
            bytes[1] = static_cast<UByte>(g * 255.0f);
        }

        if constexpr (byte_size >= 3) {
            bytes[2] = static_cast<UByte>(b * 255.0f);
        }
    }

    void SetRGBA(Float r, Float g, Float b, Float a)
    {
        bytes[0] = static_cast<UByte>(r * 255.0f);

        if constexpr (byte_size >= 2) {
            bytes[1] = static_cast<UByte>(g * 255.0f);
        }

        if constexpr (byte_size >= 3) {
            bytes[2] = static_cast<UByte>(b * 255.0f);
        }

        if constexpr (byte_size >= 4) {
            bytes[3] = static_cast<UByte>(a * 255.0f);
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

    void SetRGB(const Vector3 &rgb)
    {
        for (UInt i = 0; i < MathUtil::Min(byte_size, 3); i++) {
            bytes[byte_size - i - 1] = static_cast<UByte>(rgb[i] * 255.0f);
        }
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

    void SetRGBA(const Vector4 &rgba)
    {
        for (UInt i = 0; i < MathUtil::Min(byte_size, 4); i++) {
            bytes[byte_size - i - 1] = static_cast<UByte>(rgba[i] * 255.0f);
        }
    }
};

template <UInt NumComponents>
class Bitmap
{
    using PixelType = Pixel<NumComponents>;

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

    UInt GetWidth() const
        { return m_width; }

    UInt GetHeight() const
        { return m_height; }

    SizeType GetByteSize() const
    {
        return static_cast<SizeType>(m_width)
            * static_cast<SizeType>(m_height)
            * static_cast<SizeType>(NumComponents);
    }

    PixelType &GetPixelAtIndex(UInt index)
        { return m_pixels[index]; }

    const PixelType &GetPixelAtIndex(UInt index) const
        { return m_pixels[index]; }

    PixelType &GetPixel(UInt x, UInt y)
    {
        const UInt index = ((x + m_width) % m_width)
            + (((m_height - y) + m_height) % m_height) * m_width;

        return m_pixels[index];
    }

    const PixelType &GetPixel(UInt x, UInt y) const
        { return const_cast<const Bitmap *>(this)->GetPixel(x, y); }

    ByteBuffer ToByteBuffer() const
    {
        ByteBuffer byte_buffer;
        byte_buffer.SetSize(m_pixels.Size() * PixelType::byte_size);

        for (SizeType i = 0, j = 0; i < byte_buffer.Size() && j < m_pixels.Size(); i += PixelType::byte_size, j++) {
            for (UInt k = 0; k < PixelType::byte_size; k++) {
                byte_buffer.Data()[i + k] = m_pixels[j].bytes[k];
            }
        }

        return byte_buffer;
    }

    void GetUnpackedBytes(Array<UByte> &out)
    {
        out.Resize(m_pixels.Size() * PixelType::byte_size);

        for (SizeType i = 0, j = 0; i < out.Size() && j < m_pixels.Size(); i += PixelType::byte_size, j++) {
            for (UInt k = 0; k < PixelType::byte_size; k++) {
                out[i + k] = m_pixels[j].bytes[k];
            }
        }
    }

    void GetUnpackedFloats(Array<Float> &out)
    {
        out.Resize(m_pixels.Size() * PixelType::byte_size);

        for (SizeType i = 0, j = 0; i < out.Size() && j < m_pixels.Size(); i += PixelType::byte_size, j++) {
            for (UInt k = 0; k < PixelType::byte_size; k++) {
                out[i + k] = static_cast<Float>(m_pixels[j].bytes[k]) / 255.0f;
            }
        }
    }

    void Write(const String &filepath)
    {
        Array<UByte> unpacked_bytes;
        GetUnpackedBytes(unpacked_bytes);

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
    UInt                m_width;
    UInt                m_height;
    Array<PixelType>    m_pixels;

};

} // namespace hyperion::v2

#endif
