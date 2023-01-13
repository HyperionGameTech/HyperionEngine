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

    template <class ValueType, typename = std::enable_if_t<std::is_same_v<std::true_type, std::conditional_t<(NumComponents > 0), std::true_type, std::false_type>>>>
    Pixel(ValueType value)
    {
        if (NumComponents == 1) {
            if constexpr (std::is_same_v<ValueType, Float32>) {
                bytes[0] = static_cast<UByte>(value * 255.0f);
            } else if constexpr (std::is_same_v<ValueType, UByte>) {
                bytes[0] = value;
            } else {
                static_assert(resolution_failure<ValueType>, "Invalid type to pass to Pixel constructor");
            }
        } else {
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
class Bitmap
{
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

    Pixel &GetPixel(UInt x, UInt y)
    {
        const UInt index = ((x + m_width) % m_width)
            + (((m_height - y) + m_height) % m_height) * m_width;

        return m_pixels[index];
    }

    void SetPixelsFromMemory(UInt stride, UByte *buffer, SizeType pixel_count)
    {
        AssertThrowMsg((m_pixels.Size() >= (pixel_count)), "Pixel buffer size not large enough or component mismatch");
        AssertThrowMsg((pixel_count % stride) == 0, "Pixel buffer size is not divisible by bitmap stride!\n");

        for (SizeType index = 0; index < pixel_count; index++) {
            for (Int p_index = 0; p_index < stride; p_index++) {
                m_pixels[index].bytes[p_index] = buffer[index * stride + p_index];
            }
        }
    }

    const Pixel &GetPixel(UInt x, UInt y) const
        { return const_cast<const Bitmap *>(this)->GetPixel(x, y); }

    void GetUnpackedBytes(Array<UByte> &out)
    {
        out.Resize(m_pixels.Size() * Pixel::byte_size);

        for (SizeType i = 0, j = 0; i < out.Size() && j < m_pixels.Size(); i += Pixel::byte_size, j++) {
            for (UInt k = 0; k < Pixel::byte_size; k++) {
                out[i + k] = m_pixels[j].bytes[k];
            }
        }
    }

    void GetUnpackedFloats(Array<Float> &out)
    {
        out.Resize(m_pixels.Size() * Pixel::byte_size);

        for (SizeType i = 0, j = 0; i < out.Size() && j < m_pixels.Size(); i += Pixel::byte_size, j++) {
            for (UInt k = 0; k < Pixel::byte_size; k++) {
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

private:
    UInt m_width;
    UInt m_height;
    Array<Pixel> m_pixels;

};

} // namespace hyperion::v2

#endif
