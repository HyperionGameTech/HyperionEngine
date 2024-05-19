/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_BITMAP_HPP
#define HYPERION_BITMAP_HPP

#include <asset/ByteWriter.hpp>

#include <core/containers/Array.hpp>
#include <core/containers/String.hpp>
#include <core/Util.hpp>

#include <math/Vector3.hpp>
#include <math/Vector4.hpp>
#include <math/MathUtil.hpp>

#include <util/img/WriteBitmap.hpp>

#include <Types.hpp>

namespace hyperion {

template <class ComponentType, uint NumComponents>
struct Pixel
{
    static constexpr uint num_components = NumComponents;

    ComponentType components[NumComponents];

    Pixel() = default;

    Pixel(Vec2f rg)
    {
        if constexpr (std::is_same_v<ComponentType, ubyte>) {
            components[0] = ubyte(rg.x * 255.0f);

            if constexpr (num_components >= 2) {
                components[1] = ubyte(rg.y * 255.0f);
            }
        } else {
            components[0] = rg.x;

            if constexpr (num_components >= 2) {
                components[1] = rg.y;
            }
        }
    }

    Pixel(Vec3f rgb)
    {
        if constexpr (std::is_same_v<ComponentType, ubyte>) {
            components[0] = ubyte(rgb.x * 255.0f);

            if constexpr (num_components >= 2) {
                components[1] = ubyte(rgb.y * 255.0f);
            }

            if constexpr (num_components >= 3) {
                components[2] = ubyte(rgb.z * 255.0f);
            }
        } else {
            components[0] = rgb.x;

            if constexpr (num_components >= 2) {
                components[1] = rgb.y;
            }

            if constexpr (num_components >= 3) {
                components[2] = rgb.z;
            }
        }
    }

    Pixel(Vec4f rgba)
    {
        if constexpr (std::is_same_v<ComponentType, ubyte>) {
            components[0] = ubyte(rgba.x * 255.0f);

            if constexpr (num_components >= 2) {
                components[1] = ubyte(rgba.y * 255.0f);
            }

            if constexpr (num_components >= 3) {
                components[2] = ubyte(rgba.z * 255.0f);
            }

            if constexpr (num_components >= 4) {
                components[3] = ubyte(rgba.w * 255.0f);
            }
        } else {
            components[0] = rgba.x;

            if constexpr (num_components >= 2) {
                components[1] = rgba.y;
            }

            if constexpr (num_components >= 3) {
                components[2] = rgba.z;
            }

            if constexpr (num_components >= 4) {
                components[3] = rgba.w;
            }
        }
    }

    Pixel(ComponentType r)
    {
        components[0] = r;
    }

    Pixel(ComponentType r, ComponentType g)
    {
        components[0] = r;

        if constexpr (num_components >= 2) {
            components[1] = g;
        }
    }

    Pixel(ComponentType r, ComponentType g, ComponentType b)
    {
        components[0] = r;

        if constexpr (num_components >= 2) {
            components[1] = g;
        }

        if constexpr (num_components >= 3) {
            components[2] = b;
        }
    }

    Pixel(ComponentType r, ComponentType g, ComponentType b, ComponentType a)
    {
        components[0] = r;

        if constexpr (num_components >= 2) {
            components[1] = g;
        }

        if constexpr (num_components >= 3) {
            components[2] = b;
        }

        if constexpr (num_components >= 4) {
            components[3] = a;
        }
    }

    Pixel(const Pixel &other)                   = default;
    Pixel &operator=(const Pixel &other)        = default;
    Pixel(Pixel &&other) noexcept               = default;
    Pixel &operator=(Pixel &&other) noexcept    = default;
    ~Pixel()                                    = default;

    float GetComponent(uint index) const
    {
        if (index >= num_components) {
            return 0.0f;
        }

        if constexpr (std::is_same_v<ComponentType, ubyte>) {
            return float(components[index]) / 255.0f;
        } else {
            return components[index];
        }
    }

    void SetComponent(uint index, float value)
    {
        if (index >= num_components) {
            return;
        }

        if constexpr (std::is_same_v<ComponentType, ubyte>) {
            components[index] = ubyte(value * 255.0f);
        } else {
            components[index] = value;
        }
    }

    void SetR(float r)
    {
        if constexpr (std::is_same_v<ComponentType, ubyte>) {
            components[0] = ubyte(r * 255.0f);
        } else {
            components[0] = r;
        }
    }

    void SetRG(float r, float g)
    {
        if constexpr (std::is_same_v<ComponentType, ubyte>) {
            components[0] = ubyte(r * 255.0f);

            if constexpr (num_components >= 2) {
                components[1] = ubyte(g * 255.0f);
            }
        } else {
            components[0] = r;

            if constexpr (num_components >= 2) {
                components[1] = g;
            }
        }
    }

    void SetRGB(float r, float g, float b)
    {
        if constexpr (std::is_same_v<ComponentType, ubyte>) {
            components[0] = ubyte(r * 255.0f);

            if constexpr (num_components >= 2) {
                components[1] = ubyte(g * 255.0f);
            }

            if constexpr (num_components >= 3) {
                components[2] = ubyte(b * 255.0f);
            }
        } else {
            components[0] = r;

            if constexpr (num_components >= 2) {
                components[1] = g;
            }

            if constexpr (num_components >= 3) {
                components[2] = b;
            }
        }
    }

    void SetRGBA(float r, float g, float b, float a)
    {
        if constexpr (std::is_same_v<ComponentType, ubyte>) {
            components[0] = ubyte(r * 255.0f);

            if constexpr (num_components >= 2) {
                components[1] = ubyte(g * 255.0f);
            }

            if constexpr (num_components >= 3) {
                components[2] = ubyte(b * 255.0f);
            }

            if constexpr (num_components >= 4) {
                components[3] = ubyte(a * 255.0f);
            }
        } else {
            components[0] = r;

            if constexpr (num_components >= 2) {
                components[1] = g;
            }

            if constexpr (num_components >= 3) {
                components[2] = b;
            }

            if constexpr (num_components >= 4) {
                components[3] = a;
            }
        }
    }

    Vec3f GetRGB() const
    {
        if constexpr (std::is_same_v<ComponentType, ubyte>) {
            return Vec3f(
                float(components[0]) / 255.0f,
                float(components[1]) / 255.0f,
                float(components[2]) / 255.0f
            );
        } else {
            return Vec3f(
                components[0],
                components[1],
                components[2]
            );
        }
    }

    void SetRGB(const Vec3f &rgb)
    {
        if constexpr (std::is_same_v<ComponentType, ubyte>) {
            for (uint i = 0; i < MathUtil::Min(NumComponents, 3); i++) {
                components[i] = ubyte(rgb[i] * 255.0f);
            }
        } else {
            for (uint i = 0; i < MathUtil::Min(NumComponents, 3); i++) {
                components[i] = rgb[i];
            }
        }
    }

    HYP_FORCE_INLINE
    operator Vec3f() const
        { return GetRGB(); }

    Vec4f GetRGBA() const
    {
        if constexpr (std::is_same_v<ComponentType, ubyte>) {
            if constexpr (NumComponents < 4) {
                return Vec4f(
                    float(components[0]) / 255.0f,
                    float(components[1]) / 255.0f,
                    float(components[2]) / 255.0f,
                    1.0f
                );
            } else {
                return Vec4f(
                    float(components[0]) / 255.0f,
                    float(components[1]) / 255.0f,
                    float(components[2]) / 255.0f,
                    float(components[3]) / 255.0f
                );
            }
        } else {
            if constexpr (NumComponents < 4) {
                return Vec4f(
                    components[0],
                    components[1],
                    components[2],
                    1.0f
                );
            } else {
                return Vec4f(
                    components[0],
                    components[1],
                    components[2],
                    components[3]
                );
            }
        }
    }

    void SetRGBA(const Vec4f &rgba)
    {
        if constexpr (std::is_same_v<ComponentType, ubyte>) {
            for (uint i = 0; i < MathUtil::Min(NumComponents, 4); i++) {
                components[i] = ubyte(rgba[i] * 255.0f);
            }
        } else {
            for (uint i = 0; i < MathUtil::Min(NumComponents, 4); i++) {
                components[i] = rgba[i];
            }
        }
    }

    HYP_FORCE_INLINE
    operator Vec4f() const
        { return GetRGBA(); }
};

template <uint NumComponents, class PixelComponentType = ubyte>
class Bitmap
{
public:
    using PixelType = Pixel<PixelComponentType, NumComponents>;

    Bitmap()
        : m_width(0),
          m_height(0)
    {
    }

    Bitmap(const Array<float> &floats, uint width, uint height)
        : m_width(width),
          m_height(height)
    {
        m_pixels.Resize(width * height);

        const uint num_components = PixelType::num_components;

        for (uint i = 0, j = 0; i < floats.Size() && j < m_pixels.Size(); i += num_components, j++) {
            for (uint k = 0; k < num_components; k++) {
                m_pixels[j].SetComponent(k, floats[i + k]);
            }
        }
    }

    Bitmap(uint width, uint height)
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
        if (this == &other) {
            return *this;
        }

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
        if (this == &other) {
            return *this;
        }

        m_width = other.m_width;
        m_height = other.m_height;
        m_pixels = std::move(other.m_pixels);

        return *this;
    }

    ~Bitmap() = default;

    [[nodiscard]]
    HYP_FORCE_INLINE
    uint GetWidth() const
        { return m_width; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    uint GetHeight() const
        { return m_height; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    SizeType GetByteSize() const
    {
        return SizeType(m_width)
            * SizeType(m_height)
            * SizeType(PixelType::num_components)
            * sizeof(PixelComponentType);
    }

    [[nodiscard]]
    HYP_FORCE_INLINE
    PixelType &GetPixelAtIndex(uint index)
        { return m_pixels[index]; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    const PixelType &GetPixelAtIndex(uint index) const
        { return m_pixels[index]; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    PixelType &GetPixel(uint x, uint y)
    {
        const uint index = ((x + m_width) % m_width)
            + (((m_height - y) + m_height) % m_height) * m_width;

        return m_pixels[index];
    }

    [[nodiscard]]
    HYP_FORCE_INLINE
    const PixelType &GetPixel(uint x, uint y) const
    {
        const uint index = ((x + m_width) % m_width)
            + (((m_height - y) + m_height) % m_height) * m_width;

        return m_pixels[index];
    }

    HYP_FORCE_INLINE
    void SetPixel(uint x, uint y, PixelType pixel)
    {
        const uint index = ((x + m_width) % m_width)
            + (((m_height - y) + m_height) % m_height) * m_width;

        m_pixels[index] = pixel;
    }

    void SetPixels(const ByteBuffer &byte_buffer)
    {
        AssertThrowMsg(byte_buffer.Size() == GetByteSize(), "Byte buffer size does not match bitmap size! (%u != %u)", byte_buffer.Size(), GetByteSize());

        const uint num_components = PixelType::num_components;

        m_pixels.Resize(byte_buffer.Size() / num_components);

        for (SizeType i = 0, j = 0; i < byte_buffer.Size() && j < m_pixels.Size(); i += num_components, j++) {
            for (uint k = 0; k < num_components; k++) {
                m_pixels[j].SetComponent(k, byte_buffer.Data()[i + k]);
            }
        }
    }

    ByteBuffer ToByteBuffer() const
    {
        ByteBuffer byte_buffer;
        byte_buffer.SetSize(GetByteSize());

        for (SizeType i = 0, j = 0; i < byte_buffer.Size() && j < m_pixels.Size(); i += PixelType::num_components * sizeof(PixelComponentType), j++) {
            for (uint k = 0; k < PixelType::num_components; k++) {
                Memory::MemCpy(&byte_buffer.Data()[i + k * sizeof(PixelComponentType)], &m_pixels[j].components[k], sizeof(PixelComponentType));
            }
        }

        return byte_buffer;
    }

    Array<ubyte> GetUnpackedBytes(uint bytes_per_pixel) const
    {
        Array<ubyte> bytes;
        bytes.Resize(m_pixels.Size() * bytes_per_pixel);

        for (uint i = 0; i < m_pixels.Size(); i++) {
            for (uint j = 0; j < MathUtil::Min(PixelType::num_components, bytes_per_pixel); j++) {
                bytes[i * bytes_per_pixel + j] = ubyte(m_pixels[i].GetComponent(j) * 255.0f);
            }
        }

        return bytes;
    }

    Array<float> GetUnpackedFloats() const
    {
        Array<float> floats;
        floats.Resize(m_pixels.Size() * PixelType::num_components);

        for (uint i = 0; i < m_pixels.Size(); i++) {
           for (uint j = 0; j < PixelType::num_components; j++) {
               floats[i * PixelType::num_components + j] = m_pixels[i].GetComponent(j);
           }
        }

        return floats;
    }

    bool Write(const String &filepath) const
    {
        Array<ubyte> unpacked_bytes = GetUnpackedBytes(3 /* WriteBitmap uses 3 bytes per pixel */);

        // BMP stores in BGR format, so swap R and B
        for (uint i = 0; i < unpacked_bytes.Size(); i += 3) {
            Swap(unpacked_bytes[i], unpacked_bytes[i + 2]);
        }

        return WriteBitmap::Write(filepath.Data(), m_width, m_height, unpacked_bytes.Data());
    }

    void FlipVertical()
    {
        for (uint x = 0; x < m_width; x++) {
            for (uint y = 0; y < m_height / 2; y++) {
                auto temp = GetPixel(x, m_height - y - 1u);
                GetPixel(x, m_height - y - 1u) = GetPixel(x, y);
                GetPixel(x, y) = temp;
            }
        }
    }

    void FlipHorizontal()
    {
        for (uint x = 0; x < m_width / 2; x++) {
            for (uint y = 0; y < m_height; y++) {
                auto temp = GetPixel(m_width - x - 1u, y);
                GetPixel(m_width - x - 1u, y) = GetPixel(x, y);
                GetPixel(x, y) = temp;
            }
        }
    }

    ByteBuffer GenerateColorRamp() const
    {
        const int bits_per_pixel = PixelType::num_components * 8;
        const SizeType size = (bits_per_pixel * bits_per_pixel) - 1;

        ByteBuffer buffer(size * 4);

        for (SizeType i = 0; i < size; i++) {
            buffer.GetInternalArray().Set(i * 3, 255-i);
            buffer.GetInternalArray().Set(i * 3 + 1, 255-i);
            buffer.GetInternalArray().Set(i * 3 + 2, 255-i);
            buffer.GetInternalArray().Set(i * 3 + 3, 0);
        }

        return buffer;
    }

    // https://github.com/ssloy/tinyrenderer/wiki/Lesson-2:-Triangle-rasterization-and-back-face-culling
    void FillTriangle(
        Vec2i t0,
        Vec2i t1,
        Vec2i t2,
        PixelType color
    )
    {
        if (t0.y > t1.y) {
            std::swap(t0, t1);
        }

        if (t0.y > t2.y) {
            std::swap(t0, t2);
        }

        if (t1.y > t2.y) {
            std::swap(t1, t2);
        }

        int total_height = t2.y - t0.y;

        if (total_height == 0) {
            return;
        }

        for (int y = t0.y; y <= t1.y; y++) { 
            int segment_height = t1.y - t0.y + 1; 
            float alpha = (float)(y - t0.y) / total_height;
            float beta = (float)(y - t0.y) / segment_height;

            Vec2i A = t0 + (t2 - t0) * alpha;
            Vec2i B = t0 + (t1 - t0) * beta;

            if (A.x > B.x) {
                std::swap(A, B);
            }

            for (int j = A.x; j <= B.x; j++) {
                SetPixel(j, y, color);
            }
        }

        for (int y = t1.y; y <= t2.y; y++) {
            int segment_height = t2.y - t1.y + 1;
            float alpha = (float)(y - t0.y) / total_height;
            float beta = (float)(y - t1.y) / segment_height;

            Vec2i A = t0 + (t2 - t0) * alpha;
            Vec2i B = t1 + (t2 - t1) * beta;

            if (A.x > B.x) {
                std::swap(A, B);
            }

            for (int j = A.x; j <= B.x; j++) {
                SetPixel(j, y, color);
            }
        }
    }

    void DrawLine(uint x0, uint y0, uint x1, uint y1, PixelType color)
    {
        const int dx = MathUtil::Abs(int(x1) - int(x0));
        const int dy = MathUtil::Abs(int(y1) - int(y0));

        const int sx = x0 < x1 ? 1 : -1;
        const int sy = y0 < y1 ? 1 : -1;

        int err = dx - dy;

        while (true) {
            GetPixel(x0, y0) = color;

            if (x0 == x1 && y0 == y1) {
                break;
            }

            int e2 = 2 * err;

            if (e2 > -dy) {
                err -= dy;
                x0 += sx;
            }

            if (e2 < dx) {
                err += dx;
                y0 += sy;
            }
        }
    }

private:
    uint                m_width;
    uint                m_height;
    Array<PixelType>    m_pixels;

};

} // namespace hyperion

#endif
