#pragma once

#include <Lang/Compiler/Ast/AstConstant.hpp>
#include <Core/Containers/String.hpp>
#include <Core/Name/Name.hpp>

namespace Hyperion {

class AstCallExpression;

HYP_CLASS()
class AstName final : public AstConstant
{
    HYP_OBJECT_BODY(AstName);

public:
    AstName(const String& value, const SourceLocation& location);

    const String& GetValue() const
    {
        return m_value;
    }

    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override;

    virtual RC<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool IsNumber() const override;

    virtual const SymbolType* GetExprType() const override;

    virtual bool MayHaveSideEffects() const override
    {
        // AstName always compiles to a Name::FromString call at runtime,
        // so it always has side effects regardless of whether m_callExpr
        // has been set yet (clones won't have it, but still have side effects).
        return true;
    }

    virtual HashCode GetHashCode() const override
    {
        HashCode hc = AstConstant::GetHashCode().Add(TypeName<AstName>());
        hc.Add(m_value);

        return hc;
    }

    virtual String ToString() const override
    {
        bool containsWhitespace = false;
        for (const auto& ch : m_value)
        {
            if (utf::IsWhitespace(ch))
            {
                containsWhitespace = true;
                break;
            }
        }

        return ":" + (containsWhitespace ? "\"" + m_value + "\"" : m_value);
    }

private:
    String m_value;
    RC<AstCallExpression> m_callExpr;

    virtual void Visit(AstVisitor* visitor, Module* mod) override;

    RC<AstName> CloneImpl() const
    {
        return RC<AstName>(new AstName(
            m_value,
            m_location));
    }
};

} // namespace Hyperion
