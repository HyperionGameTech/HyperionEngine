/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/containers/Array.hpp>
#include <core/containers/String.hpp>

#include <core/memory/ByteBuffer.hpp>
#include <core/utilities/Span.hpp>

#include <core/Util.hpp>

#include <core/math/Color.hpp>
#include <core/math/Vector3.hpp>
#include <core/math/Vector4.hpp>
#include <core/math/MathUtil.hpp>
#include <core/math/Rect.hpp>

#include <util/img/WriteBitmap.hpp>

#include <core/Types.hpp>

#include <rendering/Shared.hpp>

namespace hyperion {

class ByteWriter;

template <class ComponentType, uint32 NumComponents, bool IsSrgb = false>
struct ConstPixelReference;

template <class ComponentType, uint32 NumComponents, bool IsSrgb = false>
struct PixelReference
{
    static constexpr uint32 numComponents = NumComponents;

    ubyte* byteOffset;

    HYP_FORCE_INLINE PixelReference() = default;

    HYP_FORCE_INLINE PixelReference(ubyte* byteOffset)
        : byteOffset(byteOffset)
    {
    }

    HYP_FORCE_INLINE PixelReference(const PixelReference& other) = default;
    HYP_FORCE_INLINE PixelReference& operator=(const PixelReference& other) = default;
    HYP_FORCE_INLINE PixelReference(PixelReference&& other) noexcept = default;
    HYP_FORCE_INLINE PixelReference& operator=(PixelReference&& other) noexcept = default;
    HYP_FORCE_INLINE ~PixelReference() = default;

    HYP_FORCE_INLINE ComponentType GetComponentRaw(uint32 index) const
    {
        if (index >= numComponents || !byteOffset)
        {
            return 0.0f;
        }

        return *reinterpret_cast<ComponentType*>(reinterpret_cast<uintptr_t>(byteOffset) + (sizeof(ComponentType) * index));
    }

    HYP_FORCE_INLINE float GetComponentFloat(uint32 index) const
    {
        if (index >= numComponents || !byteOffset)
        {
            return 0.0f;
        }

        float fv;

        if constexpr (std::is_same_v<ComponentType, ubyte>)
        {
            fv = float(*(byteOffset + index)) / 255.0f;
        }
        else
        {
            fv = float(*reinterpret_cast<ComponentType*>(reinterpret_cast<uintptr_t>(byteOffset) + (sizeof(ComponentType) * index)));
        }

        if constexpr (IsSrgb)
        {
            if (numComponents < 4 || index != 3)
            {
                // convert from sRGB to linear
                fv = MathUtil::Pow(fv, 2.2f);
            }
        }

        return fv;
    }

    HYP_FORCE_INLINE void SetComponentRaw(uint32 index, ComponentType value)
    {
        if (index >= numComponents || !byteOffset)
        {
            return;
        }

        *reinterpret_cast<ComponentType*>(reinterpret_cast<uintptr_t>(byteOffset) + (sizeof(ComponentType) * index)) = value;
    }

    HYP_FORCE_INLINE void SetComponentFloat(uint32 index, float value)
    {
        if (index >= numComponents || !byteOffset)
        {
            return;
        }

        if constexpr (IsSrgb)
        {
            // convert from linear to sRGB
            value = MathUtil::Pow(value, 1.0f / 2.2f);
        }

        if constexpr (std::is_same_v<ComponentType, ubyte>)
        {
            *(byteOffset + index) = ubyte(value * 255.0f);
        }
        else
        {
            *reinterpret_cast<ComponentType*>(reinterpret_cast<uintptr_t>(byteOffset) + (sizeof(ComponentType) * index)) = ComponentType(value);
        }
    }

    HYP_FORCE_INLINE float GetR() const
    {
        return GetComponentFloat(0);
    }

    HYP_FORCE_INLINE void SetR(float r)
    {
        SetComponentFloat(0, r);
    }

    HYP_FORCE_INLINE float GetG() const
    {
        return GetComponentFloat(1);
    }

    HYP_FORCE_INLINE void SetG(float g)
    {
        SetComponentFloat(1, g);
    }

    HYP_FORCE_INLINE float GetB() const
    {
        return GetComponentFloat(2);
    }

    HYP_FORCE_INLINE void SetB(float b)
    {
        SetComponentFloat(2, b);
    }

    HYP_FORCE_INLINE float GetA() const
    {
        return GetComponentFloat(3);
    }

    HYP_FORCE_INLINE void SetA(float a)
    {
        SetComponentFloat(3, a);
    }

    HYP_FORCE_INLINE Vec2f GetRG() const
    {
        Vec2f rg = Vec2f(0.0f, 0.0f);

        if (HYP_UNLIKELY(!byteOffset))
        {
            return rg;
        }

        if constexpr (std::is_same_v<ComponentType, ubyte>)
        {
            rg.x = float(*(byteOffset)) / 255.0f;

            if constexpr (numComponents >= 2)
            {
                rg.y = float(*(byteOffset + 1)) / 255.0f;
            }
        }
        else
        {
            rg.x = *reinterpret_cast<ComponentType*>(byteOffset);

            if constexpr (numComponents >= 2)
            {
                rg.y = *reinterpret_cast<ComponentType*>(reinterpret_cast<uintptr_t>(byteOffset) + sizeof(ComponentType));
            }
        }

        if constexpr (IsSrgb)
        {
            // convert from sRGB to linear
            rg = MathUtil::Pow(rg, 2.2f);
        }

        return rg;
    }

    HYP_FORCE_INLINE void SetRG(const Vec2f& rg)
    {
        SetRG(rg.x, rg.y);
    }

    HYP_FORCE_INLINE void SetRG(float r, float g)
    {
        if (HYP_UNLIKELY(!byteOffset))
        {
            return;
        }

        if constexpr (IsSrgb)
        {
            // convert from linear to sRGB
            r = MathUtil::Pow(r, 1.0f / 2.2f);
            g = MathUtil::Pow(g, 1.0f / 2.2f);
        }

        if constexpr (std::is_same_v<ComponentType, ubyte>)
        {
            *byteOffset = ubyte(r * 255.0f);

            if constexpr (numComponents >= 2)
            {
                *(byteOffset + 1) = ubyte(g * 255.0f);
            }
        }
        else
        {
            *reinterpret_cast<ComponentType*>(byteOffset) = ComponentType(r);

            if constexpr (numComponents >= 2)
            {
                *reinterpret_cast<ComponentType*>(reinterpret_cast<uintptr_t>(byteOffset) + sizeof(ComponentType)) = ComponentType(g);
            }
        }
    }

    HYP_FORCE_INLINE Vec3f GetRGB() const
    {
        Vec3f rgb = Vec3f(0.0f, 0.0f, 0.0f);

        if (HYP_UNLIKELY(!byteOffset))
        {
            return rgb;
        }

        if constexpr (std::is_same_v<ComponentType, ubyte>)
        {
            rgb.x = float(*(byteOffset)) / 255.0f;

            if constexpr (numComponents >= 2)
            {
                rgb.y = float(*(byteOffset + 1)) / 255.0f;
            }

            if constexpr (numComponents >= 3)
            {
                rgb.z = float(*(byteOffset + 2)) / 255.0f;
            }
        }
        else
        {
            rgb.x = *reinterpret_cast<ComponentType*>(byteOffset);

            if constexpr (numComponents >= 2)
            {
                rgb.y = *reinterpret_cast<ComponentType*>(reinterpret_cast<uintptr_t>(byteOffset) + sizeof(ComponentType));
            }

            if constexpr (numComponents >= 3)
            {
                rgb.z = *reinterpret_cast<ComponentType*>(reinterpret_cast<uintptr_t>(byteOffset) + sizeof(ComponentType) * 2);
            }
        }

        if constexpr (IsSrgb)
        {
            // convert from sRGB to linear
            rgb = MathUtil::Pow(rgb, 2.2f);
        }

        return rgb;
    }

    HYP_FORCE_INLINE void SetRGB(const Vec3f& rgb)
    {
        SetRGB(rgb.x, rgb.y, rgb.z);
    }

    HYP_FORCE_INLINE void SetRGB(float r, float g, float b)
    {
        if (HYP_UNLIKELY(!byteOffset))
        {
            return;
        }

        if constexpr (IsSrgb)
        {
            // convert from linear to sRGB
            r = MathUtil::Pow(r, 1.0f / 2.2f);
            g = MathUtil::Pow(g, 1.0f / 2.2f);
            b = MathUtil::Pow(b, 1.0f / 2.2f);
        }

        if constexpr (std::is_same_v<ComponentType, ubyte>)
        {
            *byteOffset = ubyte(r * 255.0f);

            if constexpr (numComponents >= 2)
            {
                *(byteOffset + 1) = ubyte(g * 255.0f);
            }

            if constexpr (numComponents >= 3)
            {
                *(byteOffset + 2) = ubyte(b * 255.0f);
            }
        }
        else
        {
            *reinterpret_cast<ComponentType*>(byteOffset) = ComponentType(r);

            if constexpr (numComponents >= 2)
            {
                *reinterpret_cast<ComponentType*>(reinterpret_cast<uintptr_t>(byteOffset) + sizeof(ComponentType)) = ComponentType(g);
            }

            if constexpr (numComponents >= 3)
            {
                *reinterpret_cast<ComponentType*>(reinterpret_cast<uintptr_t>(byteOffset) + sizeof(ComponentType) * 2) = ComponentType(b);
            }
        }
    }

    HYP_FORCE_INLINE Vec4f GetRGBA() const
    {
        Vec4f rgba = Vec4f(0.0f, 0.0f, 0.0f, 1.0f);

        if (HYP_UNLIKELY(!byteOffset))
        {
            return rgba;
        }

        if constexpr (std::is_same_v<ComponentType, ubyte>)
        {
            rgba.x = float(*(byteOffset)) / 255.0f;

            if constexpr (numComponents >= 2)
            {
                rgba.y = float(*(byteOffset + 1)) / 255.0f;
            }

            if constexpr (numComponents >= 3)
            {
                rgba.z = float(*(byteOffset + 2)) / 255.0f;
            }

            if constexpr (numComponents >= 4)
            {
                rgba.w = float(*(byteOffset + 3)) / 255.0f;
            }
        }
        else
        {
            rgba.x = *reinterpret_cast<ComponentType*>(byteOffset);

            if constexpr (numComponents >= 2)
            {
                rgba.y = *reinterpret_cast<ComponentType*>(reinterpret_cast<uintptr_t>(byteOffset) + sizeof(ComponentType));
            }

            if constexpr (numComponents >= 3)
            {
                rgba.z = *reinterpret_cast<ComponentType*>(reinterpret_cast<uintptr_t>(byteOffset) + sizeof(ComponentType) * 2);
            }

            if constexpr (numComponents >= 4)
            {
                rgba.w = *reinterpret_cast<ComponentType*>(reinterpret_cast<uintptr_t>(byteOffset) + sizeof(ComponentType) * 3);
            }
        }

        if constexpr (IsSrgb)
        {
            // convert from sRGB to linear
            Vec3f linear = MathUtil::Pow(rgba.GetXYZ(), 2.2f);
            rgba.x = linear.x;
            rgba.y = linear.y;
            rgba.z = linear.z;
        }

        return rgba;
    }

    HYP_FORCE_INLINE void SetRGBA(const Vec4f& rgba)
    {
        SetRGBA(rgba.x, rgba.y, rgba.z, rgba.w);
    }

    HYP_FORCE_INLINE void SetRGBA(float r, float g, float b, float a)
    {
        if (HYP_UNLIKELY(!byteOffset))
        {
            return;
        }

        if constexpr (IsSrgb)
        {
            // convert from linear to sRGB
            r = MathUtil::Pow(r, 1.0f / 2.2f);
            g = MathUtil::Pow(g, 1.0f / 2.2f);
            b = MathUtil::Pow(b, 1.0f / 2.2f);
            // alpha is not converted to sRGB
        }

        if constexpr (std::is_same_v<ComponentType, ubyte>)
        {
            *byteOffset = ubyte(r * 255.0f);

            if constexpr (numComponents >= 2)
            {
                *(byteOffset + 1) = ubyte(g * 255.0f);
            }

            if constexpr (numComponents >= 3)
            {
                *(byteOffset + 2) = ubyte(b * 255.0f);
            }

            if constexpr (numComponents >= 4)
            {
                *(byteOffset + 3) = ubyte(a * 255.0f);
            }
        }
        else
        {
            *reinterpret_cast<ComponentType*>(byteOffset) = ComponentType(r);

            if constexpr (numComponents >= 2)
            {
                *reinterpret_cast<ComponentType*>(reinterpret_cast<uintptr_t>(byteOffset) + sizeof(ComponentType)) = ComponentType(g);
            }

            if constexpr (numComponents >= 3)
            {
                *reinterpret_cast<ComponentType*>(reinterpret_cast<uintptr_t>(byteOffset) + sizeof(ComponentType) * 2) = ComponentType(b);
            }

            if constexpr (numComponents >= 4)
            {
                *reinterpret_cast<ComponentType*>(reinterpret_cast<uintptr_t>(byteOffset) + sizeof(ComponentType) * 3) = ComponentType(a);
            }
        }
    }

    HYP_FORCE_INLINE void SetScalar(float scalar)
    {
        SetRGBA(Vec4f(scalar));
    }

    /*HYP_FORCE_INLINE PixelReference& operator=(float scalar)
    {
        SetRGBA(Vec4f(scalar));
        return *this;
    }

    HYP_FORCE_INLINE PixelReference& operator=(const Vec2f& rg)
    {
        SetRG(rg);
        return *this;
    }

    HYP_FORCE_INLINE PixelReference& operator=(const Vec3f& rgb)
    {
        SetRGB(rgb);
        return *this;
    }

    HYP_FORCE_INLINE PixelReference& operator=(const Vec4f& rgba)
    {
        SetRGBA(rgba);
        return *this;
    }

    HYP_FORCE_INLINE explicit operator float() const
    {
        return GetR();
    }

    HYP_FORCE_INLINE operator Vec2f() const
    {
        return GetRG();
    }

    HYP_FORCE_INLINE operator Vec3f() const
    {
        return GetRGB();
    }

    HYP_FORCE_INLINE operator Vec4f() const
    {
        return GetRGBA();
    }*/
};

template <class ComponentType, uint32 NumComponents, bool IsSrgb>
struct ConstPixelReference
{
    static constexpr uint32 numComponents = NumComponents;

    const ubyte* byteOffset;

    HYP_FORCE_INLINE ConstPixelReference()
        : byteOffset(nullptr)
    {
    }

    HYP_FORCE_INLINE ConstPixelReference(const ubyte* byteOffset)
        : byteOffset(byteOffset)
    {
    }

    HYP_FORCE_INLINE ConstPixelReference(const ConstPixelReference<ComponentType, NumComponents, IsSrgb>& other) = default;
    HYP_FORCE_INLINE ConstPixelReference& operator=(const ConstPixelReference<ComponentType, NumComponents, IsSrgb>& other) = default;
    HYP_FORCE_INLINE ConstPixelReference(ConstPixelReference<ComponentType, NumComponents, IsSrgb>&& other) noexcept = default;
    HYP_FORCE_INLINE ConstPixelReference& operator=(ConstPixelReference<ComponentType, NumComponents, IsSrgb>&& other) noexcept = default;

    HYP_FORCE_INLINE ConstPixelReference(const PixelReference<ComponentType, NumComponents, IsSrgb>& other)
        : byteOffset(other.byteOffset)
    {
    }

    HYP_FORCE_INLINE ConstPixelReference& operator=(const PixelReference<ComponentType, NumComponents, IsSrgb>& other)
    {
        byteOffset = other.byteOffset;
        return *this;
    }

    HYP_FORCE_INLINE ComponentType GetComponentRaw(uint32 index) const
    {
        if (index >= numComponents || !byteOffset)
        {
            return 0.0f;
        }

        return *reinterpret_cast<ComponentType*>(reinterpret_cast<uintptr_t>(byteOffset) + (sizeof(ComponentType) * index));
    }

    HYP_FORCE_INLINE float GetComponentFloat(uint32 index) const
    {
        if (index >= numComponents || !byteOffset)
        {
            return 0.0f;
        }

        float fv;

        if constexpr (std::is_same_v<ComponentType, ubyte>)
        {
            fv = float(*(byteOffset + index)) / 255.0f;
        }
        else
        {
            fv = float(*reinterpret_cast<ComponentType*>(reinterpret_cast<uintptr_t>(byteOffset) + (sizeof(ComponentType) * index)));
        }

        if constexpr (IsSrgb)
        {
            if (numComponents < 4 || index != 3)
            {
                // convert from sRGB to linear
                fv = MathUtil::Pow(fv, 2.2f);
            }
        }

        return fv;
    }


    HYP_FORCE_INLINE float GetR() const
    {
        return GetComponentFloat(0);
    }

    HYP_FORCE_INLINE float GetG() const
    {
        return GetComponentFloat(1);
    }

    HYP_FORCE_INLINE float GetB() const
    {
        return GetComponentFloat(2);
    }

    HYP_FORCE_INLINE float GetA() const
    {
        return GetComponentFloat(3);
    }

    HYP_FORCE_INLINE Vec2f GetRG() const
    {
        Vec2f rg = Vec2f(0.0f, 0.0f);

        if (HYP_UNLIKELY(!byteOffset))
        {
            return rg;
        }

        if constexpr (std::is_same_v<ComponentType, ubyte>)
        {
            rg.x = float(*(byteOffset)) / 255.0f;

            if constexpr (numComponents >= 2)
            {
                rg.y = float(*(byteOffset + 1)) / 255.0f;
            }
        }
        else
        {
            rg.x = *reinterpret_cast<const ComponentType*>(byteOffset);

            if constexpr (numComponents >= 2)
            {
                rg.y = *reinterpret_cast<const ComponentType*>(reinterpret_cast<uintptr_t>(byteOffset) + sizeof(ComponentType));
            }
        }

        if constexpr (IsSrgb)
        {
            // convert from sRGB to linear
            rg = MathUtil::Pow(rg, 2.2f);
        }

        return rg;
    }

    HYP_FORCE_INLINE Vec3f GetRGB() const
    {
        Vec3f rgb = Vec3f(0.0f, 0.0f, 0.0f);

        if (HYP_UNLIKELY(!byteOffset))
        {
            return rgb;
        }

        if constexpr (std::is_same_v<ComponentType, ubyte>)
        {
            rgb.x = float(*(byteOffset)) / 255.0f;

            if constexpr (numComponents >= 2)
            {
                rgb.y = float(*(byteOffset + 1)) / 255.0f;
            }

            if constexpr (numComponents >= 3)
            {
                rgb.z = float(*(byteOffset + 2)) / 255.0f;
            }
        }
        else
        {
            rgb.x = *reinterpret_cast<const ComponentType*>(byteOffset);

            if constexpr (numComponents >= 2)
            {
                rgb.y = *reinterpret_cast<const ComponentType*>(reinterpret_cast<uintptr_t>(byteOffset) + sizeof(ComponentType));
            }

            if constexpr (numComponents >= 3)
            {
                rgb.z = *reinterpret_cast<const ComponentType*>(reinterpret_cast<uintptr_t>(byteOffset) + sizeof(ComponentType) * 2);
            }
        }

        if constexpr (IsSrgb)
        {
            // convert from sRGB to linear
            rgb = MathUtil::Pow(rgb, 2.2f);
        }

        return rgb;
    }

    HYP_FORCE_INLINE Vec4f GetRGBA() const
    {
        Vec4f rgba = Vec4f(0.0f, 0.0f, 0.0f, 1.0f);

        if (HYP_UNLIKELY(!byteOffset))
        {
            return rgba;
        }

        if constexpr (std::is_same_v<ComponentType, ubyte>)
        {
            rgba.x = float(*(byteOffset)) / 255.0f;

            if constexpr (numComponents >= 2)
            {
                rgba.y = float(*(byteOffset + 1)) / 255.0f;
            }

            if constexpr (numComponents >= 3)
            {
                rgba.z = float(*(byteOffset + 2)) / 255.0f;
            }

            if constexpr (numComponents >= 4)
            {
                rgba.w = float(*(byteOffset + 3)) / 255.0f;
            }
        }
        else
        {
            rgba.x = *reinterpret_cast<const ComponentType*>(byteOffset);

            if constexpr (numComponents >= 2)
            {
                rgba.y = *reinterpret_cast<const ComponentType*>(reinterpret_cast<uintptr_t>(byteOffset) + sizeof(ComponentType));
            }

            if constexpr (numComponents >= 3)
            {
                rgba.z = *reinterpret_cast<const ComponentType*>(reinterpret_cast<uintptr_t>(byteOffset) + sizeof(ComponentType) * 2);
            }

            if constexpr (numComponents >= 4)
            {
                rgba.w = *reinterpret_cast<const ComponentType*>(reinterpret_cast<uintptr_t>(byteOffset) + sizeof(ComponentType) * 3);
            }
        }

        if constexpr (IsSrgb)
        {
            // convert from sRGB to linear
            Vec3f linear = MathUtil::Pow(rgba.GetXYZ(), 2.2f);
            rgba.x = linear.x;
            rgba.y = linear.y;
            rgba.z = linear.z;
        }

        return rgba;
    }

    /*

    HYP_FORCE_INLINE explicit operator float() const
    {
        return GetR();
    }

    HYP_FORCE_INLINE operator Vec2f() const
    {
        return GetRG();
    }

    HYP_FORCE_INLINE operator Vec3f() const
    {
        return GetRGB();
    }

    HYP_FORCE_INLINE operator Vec4f() const
    {
        return GetRGBA();
    }*/
};

template <TextureFormat Format>
class Bitmap
{
public:
    using PixelComponentType = typename TextureFormatHelper<Format>::ElementType;
    static constexpr uint32 numComponents = TextureFormatHelper<Format>::numComponents;
    static constexpr bool isSrgb = TextureFormatHelper<Format>::isSrgb;

    using PixelReferenceType = PixelReference<PixelComponentType, numComponents, isSrgb>;
    using ConstPixelReferenceType = ConstPixelReference<PixelComponentType, numComponents, isSrgb>;

    Bitmap()
        : m_width(0),
          m_height(0)
    {
    }

    Bitmap(Span<const PixelComponentType> pixelData, uint32 width, uint32 height)
        : m_width(width),
          m_height(height)
    {
        m_buffer.SetSize(GetByteSize());

        const SizeType numPixels = m_width * m_height;

        HYP_CORE_ASSERT((pixelData.Size() * sizeof(PixelComponentType) == GetByteSize()), "Bad pixel data!");

        for (SizeType i = 0, j = 0; i < pixelData.Size() && j < numPixels; i += numComponents, j++)
        {
            for (uint32 k = 0; k < numComponents; k++)
            {
                GetPixelReference(j).SetComponentRaw(k, pixelData[i + k]);
            }
        }
    }

    Bitmap(uint32 width, uint32 height)
        : m_width(width),
          m_height(height)
    {
        m_buffer.SetSize(GetByteSize());
    }

    Bitmap(const Bitmap& other)
        : m_width(other.m_width),
          m_height(other.m_height),
          m_buffer(other.m_buffer)
    {
    }

    Bitmap& operator=(const Bitmap& other)
    {
        if (this == &other)
        {
            return *this;
        }

        m_width = other.m_width;
        m_height = other.m_height;
        m_buffer = other.m_buffer;

        return *this;
    }

    Bitmap(Bitmap&& other) noexcept
        : m_width(other.m_width),
          m_height(other.m_height),
          m_buffer(std::move(other.m_buffer))
    {
    }

    Bitmap& operator=(Bitmap&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        m_width = other.m_width;
        m_height = other.m_height;
        m_buffer = std::move(other.m_buffer);

        return *this;
    }

    ~Bitmap() = default;

    HYP_FORCE_INLINE static constexpr TextureFormat GetFormat()
    {
        return Format;
    }

    HYP_FORCE_INLINE static constexpr uint32 GetNumComponents()
    {
        return numComponents;
    }

    HYP_FORCE_INLINE uint32 GetWidth() const
    {
        return m_width;
    }

    HYP_FORCE_INLINE uint32 GetHeight() const
    {
        return m_height;
    }

    HYP_FORCE_INLINE SizeType GetByteSize() const
    {
        return SizeType(m_width)
            * SizeType(m_height)
            * SizeType(numComponents)
            * sizeof(PixelComponentType);
    }

    // Get reference to the pixel at the given 1-dimensional index
    HYP_FORCE_INLINE PixelReferenceType GetPixelReference(uint32 idx)
    {
        if (HYP_UNLIKELY(m_width * m_height == 0))
        {
            return PixelReferenceType(nullptr);
        }

        const SizeType byteIndex = (SizeType(idx) % (m_width * m_height)) * numComponents * sizeof(PixelComponentType);

        PixelReferenceType pixelReference { m_buffer.Data() + byteIndex };

        return pixelReference;
    }

    // Get reference to pixel at x,y
    HYP_FORCE_INLINE PixelReferenceType GetPixelReference(uint32 x, uint32 y)
    {
        const SizeType index = ((SizeType(y) + m_height) % m_height) * m_width
            + ((SizeType(x) + m_width) % m_width);

        PixelReferenceType pixelReference { m_buffer.Data() + index * numComponents * sizeof(PixelComponentType) };

        return pixelReference;
    }

    // Get reference to the pixel at the given 1-dimensional index
    HYP_FORCE_INLINE ConstPixelReferenceType GetPixelReference(uint32 idx) const
    {
        return const_cast<Bitmap<Format>*>(this)->GetPixelReference(idx);
    }

    HYP_FORCE_INLINE ConstPixelReferenceType GetPixelReference(uint32 x, uint32 y) const
    {
        return const_cast<Bitmap<Format>*>(this)->GetPixelReference(x, y);
    }

    HYP_FORCE_INLINE void SetPixel(uint32 x, uint32 y, const Vec4f& rgba)
    {
        GetPixelReference(x, y).SetRGBA(rgba);
    }

    void SetPixels(const ByteBuffer& byteBuffer)
    {
        HYP_CORE_ASSERT(byteBuffer.Size() == GetByteSize(), "Byte buffer size does not match bitmap size! (%zu != %zu)", byteBuffer.Size(), GetByteSize());

        m_buffer = byteBuffer;
    }

    void SetPixels(ByteBuffer&& byteBuffer)
    {
        HYP_CORE_ASSERT(byteBuffer.Size() == GetByteSize(), "Byte buffer size does not match bitmap size! (%zu != %zu)", byteBuffer.Size(), GetByteSize());

        m_buffer = std::move(byteBuffer);
    }

    ByteView ToByteView()
    {
        return m_buffer.ToByteView();
    }

    ConstByteView ToByteView() const
    {
        return m_buffer.ToByteView();
    }

    /*! \brief Get data as 1 byte per component (e.g RGBA8) */
    ByteBuffer GetUnpackedBytes(uint32 bytesPerPixel = numComponents) const
    {
        ByteBuffer byteBuffer;
        byteBuffer.SetSize((m_width * m_height) * bytesPerPixel);

        ubyte* bytes = byteBuffer.Data();

        if (bytesPerPixel == 1)
        {
            for (uint32 x = 0; x < m_width; x++)
            {
                for (uint32 y = 0; y < m_height; y++)
                {
                    ConstPixelReferenceType pixelReference = GetPixelReference(x, y);
                    Vec4f rgba = pixelReference.GetRGBA();

                    const Color color { rgba };

                    for (uint32 j = 0; j < MathUtil::Min(numComponents, bytesPerPixel); j++)
                    {
                        bytes[((m_height - y - 1) * m_width + x) * bytesPerPixel + j] = color.bytes[j];
                    }
                }
            }
        }
        else
        {
            for (uint32 x = 0; x < m_width; x++)
            {
                for (uint32 y = 0; y < m_height; y++)
                {
                    ConstPixelReferenceType pixelReference = GetPixelReference(x, y);

                    for (uint32 j = 0; j < MathUtil::Min(numComponents, bytesPerPixel); j++)
                    {
                        bytes[((m_height - y - 1) * m_width + x) * bytesPerPixel + j] = ubyte(pixelReference.GetComponentFloat(j) * 255.0f);
                    }
                }
            }
        }

        return byteBuffer;
    }

    /*! \brief Get data as raw float array (pixels are converted to 32-bit float */
    Array<float> GetUnpackedFloats() const
    {
        Array<float> floats;
        floats.Resize(m_width * m_height * numComponents);

        for (uint32 x = 0; x < m_width; x++)
        {
            for (uint32 y = 0; y < m_height; y++)
            {
                ConstPixelReferenceType pixelReference = GetPixelReference(x, y);

                for (uint32 j = 0; j < numComponents; j++)
                {
                    floats[(y * m_width + x) * numComponents + j] = pixelReference.GetComponentFloat(j);
                }
            }
        }

        return floats;
    }

    /*! \brief Writes the Bitmap's data as a BMP file to the output stream */
    bool Write(ByteWriter* byteWriter) const
    {
        // WriteBitmap uses 3 bytes per pixel
        ByteBuffer unpackedBytes = GetUnpackedBytes(3);

        // BMP stores in BGR format, so swap R and B
        for (SizeType i = 0; i < unpackedBytes.Size(); i += 3)
        {
            Swap(unpackedBytes.Data()[i], unpackedBytes.Data()[i + 2]);
        }

        return WriteBitmap::Write(byteWriter, m_width, m_height, unpackedBytes.Data());
    }

    void FlipVertical()
    {
        for (uint32 x = 0; x < m_width; x++)
        {
            for (uint32 y = 0; y < m_height / 2; y++)
            {
                const Vec4f temp = GetPixelReference(x, m_height - y - 1u).GetRGBA();
                GetPixelReference(x, m_height - y - 1u).SetRGBA(GetPixelReference(x, y).GetRGBA());
                GetPixelReference(x, y).SetRGBA(temp);
            }
        }
    }

    void FlipHorizontal()
    {
        for (uint32 x = 0; x < m_width / 2; x++)
        {
            for (uint32 y = 0; y < m_height; y++)
            {
                const Vec4f temp = GetPixelReference(m_width - x - 1u, y).GetRGBA();
                GetPixelReference(m_width - x - 1u, y).SetRGBA(GetPixelReference(x, y).GetRGBA());
                GetPixelReference(x, y).SetRGBA(temp);
            }
        }
    }

    void FillRectangle(Vec2i p0, Vec2i p1, const Vec4f& color)
    {
        for (int y = p0.y; y <= p1.y; y++)
        {
            for (int x = p0.x; x <= p1.x; x++)
            {
                SetPixel(x, y, color);
            }
        }
    }

    // https://github.com/ssloy/tinyrenderer/wiki/Lesson-2:-Triangle-rasterization-and-back-face-culling
    void FillTriangle(Vec2i t0, Vec2i t1, Vec2i t2, const Vec4f& color)
    {
        if (t0.y > t1.y)
        {
            std::swap(t0, t1);
        }

        if (t0.y > t2.y)
        {
            std::swap(t0, t2);
        }

        if (t1.y > t2.y)
        {
            std::swap(t1, t2);
        }

        int totalHeight = t2.y - t0.y;

        if (totalHeight == 0)
        {
            return;
        }

        for (int y = t0.y; y <= t1.y; y++)
        {
            int segmentHeight = t1.y - t0.y + 1;
            float alpha = (float)(y - t0.y) / totalHeight;
            float beta = (float)(y - t0.y) / segmentHeight;

            Vec2i a = t0 + (t2 - t0) * alpha;
            Vec2i b = t0 + (t1 - t0) * beta;

            if (a.x > b.x)
            {
                std::swap(a, b);
            }

            for (int j = a.x; j <= b.x; j++)
            {
                SetPixel(j, y, color);
            }
        }

        for (int y = t1.y; y <= t2.y; y++)
        {
            int segmentHeight = t2.y - t1.y + 1;
            float alpha = (float)(y - t0.y) / totalHeight;
            float beta = (float)(y - t1.y) / segmentHeight;

            Vec2i a = t0 + (t2 - t0) * alpha;
            Vec2i b = t1 + (t2 - t1) * beta;

            if (a.x > b.x)
            {
                std::swap(a, b);
            }

            for (int j = a.x; j <= b.x; j++)
            {
                SetPixel(j, y, color);
            }
        }
    }

    void DrawLine(uint32 x0, uint32 y0, uint32 x1, uint32 y1, const Vec4f& color)
    {
        const int dx = MathUtil::Abs(int(x1) - int(x0));
        const int dy = MathUtil::Abs(int(y1) - int(y0));

        const int sx = x0 < x1 ? 1 : -1;
        const int sy = y0 < y1 ? 1 : -1;

        int err = dx - dy;

        while (true)
        {
            GetPixelReference(x0, y0).SetRGBA(color);

            if (x0 == x1 && y0 == y1)
            {
                break;
            }

            int e2 = 2 * err;

            if (e2 > -dy)
            {
                err -= dy;
                x0 += sx;
            }

            if (e2 < dx)
            {
                err += dx;
                y0 += sy;
            }
        }
    }

    /*! \brief Blit another bitmap onto this bitmap at the specified offset.
     *
     * \param other The bitmap to blit.
     * \param offset The offset to blit the other bitmap at.
     * \param extent The dimensions of the area to blit.
     */
    template <TextureFormat OtherFormat>
    void Blit(const Bitmap<OtherFormat>& other, Rect<uint32> srcRect, Rect<uint32> dstRect)
    {
        const int dstStartX = dstRect.x0;
        const int dstEndX = dstRect.x1;

        const int dstStartY = dstRect.y0;
        const int dstEndY = dstRect.y1;

        const int dstWidth = MathUtil::Abs(dstEndX - dstStartX);
        const int dstHeight = MathUtil::Abs(dstEndY - dstStartY);

        const int dstStepX = (dstEndX > dstStartX) ? 1 : -1;
        const int dstStepY = (dstEndY > dstStartY) ? 1 : -1;

        const float srcStartX = float(srcRect.x0);
        const float srcEndX = float(srcRect.x1);

        const float srcStartY = float(srcRect.y0);
        const float srcEndY = float(srcRect.y1);

        const float srcWidth = srcEndX - srcStartX;
        const float srcHeight = srcEndY - srcStartY;

        for (int j = 0; j < dstHeight; j++)
        {
            const float srcY = srcStartY + ((float(j) + 0.5f) / float(dstHeight)) * srcHeight;

            int sy0 = int(MathUtil::Floor(srcY));
            int sy1 = sy0 + 1;

            const float ty = srcY - float(sy0);

            sy0 = MathUtil::Max(sy0, 0);
            sy1 = MathUtil::Max(sy1, 0);

            for (int i = 0; i < dstWidth; i++)
            {
                const float srcX = srcStartX + ((float(i) + 0.5f) / float(dstWidth)) * srcWidth;

                int sx0 = int(MathUtil::Floor(srcX));
                int sx1 = sx0 + 1;

                const float tx = srcX - float(sx0);

                sx0 = MathUtil::Max(sx0, 0);
                sx1 = MathUtil::Max(sx1, 0);

                const Vec4f c00 = sx0 < other.GetWidth() && sy0 < other.GetHeight() ? other.GetPixelReference(sx0, sy0).GetRGBA() : Vec4f(0.0f, 0.0f, 0.0f, 1.0f);
                const Vec4f c10 = sx1 < other.GetWidth() && sy0 < other.GetHeight() ? other.GetPixelReference(sx1, sy0).GetRGBA() : Vec4f(0.0f, 0.0f, 0.0f, 1.0f);
                const Vec4f c01 = sx0 < other.GetWidth() && sy1 < other.GetHeight() ? other.GetPixelReference(sx0, sy1).GetRGBA() : Vec4f(0.0f, 0.0f, 0.0f, 1.0f);
                const Vec4f c11 = sx1 < other.GetWidth() && sy1 < other.GetHeight() ? other.GetPixelReference(sx1, sy1).GetRGBA() : Vec4f(0.0f, 0.0f, 0.0f, 1.0f);

                const Vec4f c0 = MathUtil::Lerp(c00, c10, tx);
                const Vec4f c1 = MathUtil::Lerp(c01, c11, tx);

                Vec4f resultColor = MathUtil::Lerp(c0, c1, ty);

                GetPixelReference(uint32(dstStartX + i * dstStepX), uint32(dstStartY + j * dstStepY)).SetRGBA(resultColor);
            }
        }
    }

private:
    uint32 m_width;
    uint32 m_height;
    ByteBuffer m_buffer;
};

using Bitmap_RGBA8_SRGB = Bitmap<TF_RGBA8_SRGB>;
using Bitmap_RGBA8 = Bitmap<TF_RGBA8>;
using Bitmap_RGB8 = Bitmap<TF_RGB8>;
using Bitmap_RG8 = Bitmap<TF_RG8>;
using Bitmap_R8 = Bitmap<TF_R8>;
using Bitmap_RGBA16F = Bitmap<TF_RGBA16F>;
using Bitmap_RGB16F = Bitmap<TF_RGB16F>;
using Bitmap_RG16F = Bitmap<TF_RG16F>;
using Bitmap_R16F = Bitmap<TF_R16F>;
using Bitmap_RGBA32F = Bitmap<TF_RGBA32F>;
using Bitmap_RGB32F = Bitmap<TF_RGB32F>;
using Bitmap_RG32F = Bitmap<TF_RG32F>;
using Bitmap_R32F = Bitmap<TF_R32F>;

} // namespace hyperion
