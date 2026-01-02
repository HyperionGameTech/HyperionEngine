struct R11G11B10F
{
    uint32 value;

    R11G11B10F() = default;

    constexpr R11G11B10F(uint32 packedValue)
        : value(packedValue)
    {
    }

    constexpr R11G11B10F(float r, float g, float b)
        : value(0)
    {
        value = EncodeFloat11(MathUtil::Max(r, 0.0f))
            | (EncodeFloat11(MathUtil::Max(g, 0.0f)) << 11)
            | (EncodeFloat10(MathUtil::Max(b, 0.0f)) << 22);
    }

    constexpr R11G11B10F(const Vec3f& rgb)
        : R11G11B10F(rgb.x, rgb.y, rgb.z)
    {
    }

    HYP_FORCE_INLINE constexpr float GetR() const
    {
        return DecodeFloat11(value & 0x7FF);
    }

    HYP_FORCE_INLINE constexpr float GetG() const
    {
        return DecodeFloat11((value >> 11) & 0x7FF);
    }

    HYP_FORCE_INLINE constexpr float GetB() const
    {
        return DecodeFloat10((value >> 22) & 0x3FF);
    }

    HYP_FORCE_INLINE constexpr Vec3f GetRGB() const
    {
        return Vec3f(GetR(), GetG(), GetB());
    }

    HYP_FORCE_INLINE constexpr void SetR(float r)
    {
        value = (value & ~0x7FF) | EncodeFloat11(MathUtil::Max(r, 0.0f));
    }

    HYP_FORCE_INLINE constexpr void SetG(float g)
    {
        value = (value & ~(0x7FF << 11)) | (EncodeFloat11(MathUtil::Max(g, 0.0f)) << 11);
    }

    HYP_FORCE_INLINE constexpr void SetB(float b)
    {
        value = (value & ~(0x3FF << 22)) | (EncodeFloat10(MathUtil::Max(b, 0.0f)) << 22);
    }

    HYP_FORCE_INLINE constexpr void SetRGB(float r, float g, float b)
    {
        value = EncodeFloat11(MathUtil::Max(r, 0.0f))
            | (EncodeFloat11(MathUtil::Max(g, 0.0f)) << 11)
            | (EncodeFloat10(MathUtil::Max(b, 0.0f)) << 22);
    }

    HYP_FORCE_INLINE constexpr void SetRGB(const Vec3f& rgb)
    {
        SetRGB(rgb.x, rgb.y, rgb.z);
    }

private:
    // Encode a float to an 11-bit unsigned float (5 exponent, 6 mantissa)
    static constexpr uint32 EncodeFloat11(float value)
    {
        if (value <= 0.0f)
        {
            return 0;
        }

        const uint32 floatBits = std::bit_cast<uint32>(value);
        const int32 exponent = int32((floatBits >> 23) & 0xFF) - 127;
        const uint32 mantissa = floatBits & 0x7FFFFF;

        // Bias to 11-bit float exponent (bias = 15)
        int32 newExponent = exponent + 15;

        if (newExponent >= 31)
        {
            // Infinity/overflow
            return 0x7C0; // Infinity representation (exponent = 31, mantissa = 0)
        }
        
        if (newExponent <= 0)
        {
            // Denormalized or zero
            if (newExponent < -6)
            {
                return 0;
            }
            
            // Denormalized
            uint32 denormMantissa = (mantissa | 0x800000) >> (1 - newExponent + 17);
            return denormMantissa & 0x3F;
        }

        // Normalized: 5 bits exponent, 6 bits mantissa
        return (uint32(newExponent) << 6) | ((mantissa >> 17) & 0x3F);
    }

    // Encode a float to a 10-bit unsigned float (5 exponent, 5 mantissa)
    static constexpr uint32 EncodeFloat10(float value)
    {
        if (value <= 0.0f)
        {
            return 0;
        }

        const uint32 floatBits = std::bit_cast<uint32>(value);
        const int32 exponent = int32((floatBits >> 23) & 0xFF) - 127;
        const uint32 mantissa = floatBits & 0x7FFFFF;

        // Bias to 10-bit float exponent (bias = 15)
        int32 newExponent = exponent + 15;

        if (newExponent >= 31)
        {
            // Infinity/overflow
            return 0x3E0; // Infinity representation (exponent = 31, mantissa = 0)
        }
        
        if (newExponent <= 0)
        {
            // Denormalized or zero
            if (newExponent < -5)
            {
                return 0;
            }
            
            // Denormalized
            uint32 denormMantissa = (mantissa | 0x800000) >> (1 - newExponent + 18);
            return denormMantissa & 0x1F;
        }

        // Normalized: 5 bits exponent, 5 bits mantissa
        return (uint32(newExponent) << 5) | ((mantissa >> 18) & 0x1F);
    }

    // Decode an 11-bit unsigned float to a 32-bit float
    static constexpr float DecodeFloat11(uint32 value)
    {
        const uint32 exponent = (value >> 6) & 0x1F;
        const uint32 mantissa = value & 0x3F;

        if (exponent == 0)
        {
            if (mantissa == 0)
            {
                return 0.0f;
            }
            
            // Denormalized
            return std::bit_cast<float>((mantissa << 17) | ((127 - 14) << 23));
        }
        
        if (exponent == 31)
        {
            // Infinity
            return std::bit_cast<float>(0x7F800000);
        }

        // Normalized
        const uint32 newExponent = exponent - 15 + 127;
        return std::bit_cast<float>((newExponent << 23) | (mantissa << 17));
    }

    // Decode a 10-bit unsigned float to a 32-bit float
    static constexpr float DecodeFloat10(uint32 value)
    {
        const uint32 exponent = (value >> 5) & 0x1F;
        const uint32 mantissa = value & 0x1F;

        if (exponent == 0)
        {
            if (mantissa == 0)
            {
                return 0.0f;
            }
            
            // Denormalized
            return std::bit_cast<float>((mantissa << 18) | ((127 - 14) << 23));
        }
        
        if (exponent == 31)
        {
            // Infinity
            return std::bit_cast<float>(0x7F800000);
        }

        // Normalized
        const uint32 newExponent = exponent - 15 + 127;
        return std::bit_cast<float>((newExponent << 23) | (mantissa << 18));
    }
};