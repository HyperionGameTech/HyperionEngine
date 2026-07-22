#pragma once

#include <Lang/Compiler/Ast/AstConstant.hpp>
#include <Core/Containers/String.hpp>

#include <string>

namespace Hyperion {

HYP_CLASS()
class AstString final : public AstConstant
{
    HYP_OBJECT_BODY(AstString);

public:
    AstString(const String& value, const SourceLocation& location);

    const String& GetValue() const
    {
        return m_value;
    }

    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override;

    virtual Handle<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool IsNumber() const override;

    virtual const SymbolType* GetExprType() const override;

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

    Handle<AstString> CloneImpl() const
    {
        return MakeHandle<AstString>(
            m_value,
            m_location);
    }
};

} // namespace Hyperion
