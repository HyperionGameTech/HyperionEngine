#pragma once

#include <script/compiler/ast/AstConstant.hpp>
#include <core/containers/String.hpp>

#include <string>

namespace hyperion {

class AstString : public AstConstant
{
public:
    AstString(const String& value, const SourceLocation& location);

    const String& GetValue() const
    {
        return m_value;
    }

    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override;

    virtual RC<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool IsNumber() const override;

    virtual Optional<hyperion::int64> IntValue() const override
    {
        return {};
    }

    virtual SymbolTypeRef GetExprType() const override;

    virtual RC<AstConstant> HandleOperator(Operators opType, const AstConstant* right) const override;

    virtual HashCode GetHashCode() const override
    {
        HashCode hc = AstConstant::GetHashCode().Add(TypeName<AstString>());
        hc.Add(m_value);

        return hc;
    }

    virtual String ToString() const override
    {
        return "\"" + m_value + "\"";
    }

private:
    String m_value;

    RC<AstString> CloneImpl() const
    {
        return RC<AstString>(new AstString(
            m_value,
            m_location));
    }
};

} // namespace hyperion
