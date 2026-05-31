#pragma once

#include <Lang/compiler/ast/AstStatement.hpp>
#include <Lang/compiler/type-system/SymbolType.hpp>
#include <Lang/compiler/Enums.hpp>
#include <Lang/Tribool.hpp>

#include <Core/utilities/Optional.hpp>
#include <Core/utilities/Variant.hpp>

#include <Core/HashCode.hpp>

namespace Hyperion {

using ExprAccess = uint32;

enum ExprAccessBits : ExprAccess
{
    EXPR_ACCESS_NONE = 0x0,
    EXPR_ACCESS_PUBLIC = 0x1,
    EXPR_ACCESS_PRIVATE = 0x2,
    EXPR_ACCESS_PROTECTED = 0x4
};

using ExpressionFlags = uint32;

enum ExpressionFlagBits : ExpressionFlags
{
    EXPR_FLAGS_NONE = 0x0,
    EXPR_FLAGS_CONSTRUCTOR_DEFINITION = 0x1
};

enum InvalidConstantNumberTag
{
    INVALID_CONSTANT_NUMBER
};

struct ConstantValue
{
    Variant<int64, uint64, float64, bool> value;
    ConstantBitSize bitSize;

    explicit ConstantValue(InvalidConstantNumberTag)
        : value(),
          bitSize(CBS_INVALID)
    {
    }

    ConstantValue(int64 value, ConstantBitSize bitSize)
        : value(value),
          bitSize(bitSize)
    {
    }

    ConstantValue(uint64 value, ConstantBitSize bitSize)
        : value(value),
          bitSize(bitSize)
    {
    }

    ConstantValue(float64 value, ConstantBitSize bitSize)
        : value(value),
          bitSize(bitSize)
    {
    }

    ConstantValue(bool value, ConstantBitSize bitSize)
        : value(value),
          bitSize(bitSize)
    {
    }

    explicit operator bool() const
    {
        return value.HasValue() && bitSize != CBS_INVALID;
    }

    bool operator!() const
    {
        return !operator bool();
    }

    bool operator==(const ConstantValue& other) const
    {
        return value == other.value && bitSize == other.bitSize;
    }

    bool operator!=(const ConstantValue& other) const
    {
        return !operator==(other);
    }

    HYP_FORCE_INLINE bool IsValid() const
    {
        return value.HasValue() && bitSize != CBS_INVALID;
    }

    HYP_FORCE_INLINE bool IsInt() const
    {
        return value.Is<int64>();
    }

    HYP_FORCE_INLINE bool IsUInt() const
    {
        return value.Is<uint64>();
    }

    HYP_FORCE_INLINE bool IsFloat() const
    {
        return value.Is<float64>();
    }

    HYP_FORCE_INLINE bool IsBool() const
    {
        return value.Is<bool>();
    }

    HYP_FORCE_INLINE bool IsNumber() const
    {
        return IsInt() || IsUInt() || IsFloat();
    }

    int64 AsInt() const
    {
        int64 i = 0;

        value.Visit([&](auto&& arg)
            {
                i = int64(arg);
            });

        return i;
    }

    uint64 AsUInt() const
    {
        uint64 u = 0;

        value.Visit([&](auto&& arg)
            {
                u = uint64(arg);
            });

        return u;
    }

    float64 AsFloat() const
    {
        float64 f = 0.0;

        value.Visit([&](auto&& arg)
            {
                f = float64(arg);
            });

        return f;
    }

    bool AsBool() const
    {
        bool b = false;

        value.Visit([&](auto&& arg)
            {
                b = !!arg;
            });

        return b;
    }

    HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(value);
        hc.Add(bitSize);

        return hc;
    }
};

class AstExpression : public AstStatement
{
public:
    AstExpression(
        const SourceLocation& location,
        int accessOptions);
    virtual ~AstExpression() = default;

    HYP_FORCE_INLINE int GetAccessOptions() const
    {
        return m_accessOptions;
    }

    HYP_FORCE_INLINE AccessMode GetAccessMode() const
    {
        return m_accessMode;
    }

    HYP_FORCE_INLINE void SetAccessMode(AccessMode accessMode)
    {
        m_accessMode = accessMode;
    }

    virtual void Visit(AstVisitor* visitor, Module* mod) override = 0;
    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override = 0;
    virtual void Optimize(AstVisitor* visitor, Module* mod) override = 0;

    virtual RC<AstStatement> Clone() const override = 0;

    /**
     * Overridden by derived classes to allow "constexpr"-type functionality.
     */
    virtual bool IsLiteral() const
    {
        return false;
    }

    virtual const AstExpression* GetValueOf() const
    {
        return this;
    }

    virtual const AstExpression* GetDeepValueOf() const
    {
        return GetValueOf();
    }

    virtual AstExpression* GetTarget() const
    {
        return nullptr;
    }

    virtual ConstantValue GetConstantValue() const
    {
        return ConstantValue(INVALID_CONSTANT_NUMBER);
    }

    /** Determine whether the expression would evaluate to true.
        Returns -1 if it cannot be evaluated at compile time.
    */
    virtual Tribool IsTrue() const = 0;

    /** Determine whether or not there is a possibility of side effects. */
    virtual bool MayHaveSideEffects() const = 0;
    virtual const SymbolType* GetExprType() const = 0;
    virtual const SymbolType* GetHeldType() const
    {
        return nullptr;
    }

    virtual const SymbolType* GetTargetType() const
    {
        const AstExpression* target = GetTarget();
        if (target == nullptr)
        {
            return nullptr;
        }

        return target->GetHeldType();
    }

    virtual HashCode GetHashCode() const override
    {
        return HashCode().Add(TypeName<AstExpression>());
    }

    virtual bool IsMutable() const
    {
        return false;
    }

    ExpressionFlags GetExpressionFlags() const
    {
        return m_expressionFlags;
    }

    void SetExpressionFlags(ExpressionFlags expressionFlags)
    {
        m_expressionFlags = expressionFlags;
    }

    void ApplyExpressionFlags(ExpressionFlags expressionFlags, bool set = true)
    {
        if (set)
        {
            m_expressionFlags |= expressionFlags;
        }
        else
        {
            m_expressionFlags &= ~expressionFlags;
        }
    }

protected:
    AccessMode m_accessMode;
    int m_accessOptions;
    ExpressionFlags m_expressionFlags = EXPR_FLAGS_NONE;
};

} // namespace Hyperion
