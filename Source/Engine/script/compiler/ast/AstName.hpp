#pragma once

#include <script/compiler/ast/AstConstant.hpp>
#include <Core/containers/String.hpp>
#include <Core/Name.hpp>

namespace Hyperion {

class AstCallExpression;

class AstName final : public AstConstant
{
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
