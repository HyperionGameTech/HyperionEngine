#pragma once

#include <script/compiler/ast/AstIdentifier.hpp>
#include <core/containers/String.hpp>

namespace hyperion {

class AstModuleProperty : public AstExpression
{
public:
    AstModuleProperty(
        const String& fieldName,
        const SourceLocation& location);
    virtual ~AstModuleProperty() override = default;

    virtual void Visit(AstVisitor* visitor, Module* mod) override;
    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override;
    virtual void Optimize(AstVisitor* visitor, Module* mod) override;

    virtual RC<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool MayHaveSideEffects() const override;
    virtual SymbolTypeRef GetExprType() const override;

    virtual HashCode GetHashCode() const override
    {
        HashCode hc = AstExpression::GetHashCode().Add(TypeName<AstModuleProperty>());
        hc.Add(m_fieldName);

        return hc;
    }

protected:
    String m_fieldName;

    // set while analyzing
    SymbolTypeRef m_exprType;
    RC<AstExpression> m_exprValue;

    RC<AstModuleProperty> CloneImpl() const
    {
        return RC<AstModuleProperty>(new AstModuleProperty(
            m_fieldName,
            m_location));
    }
};

} // namespace hyperion
