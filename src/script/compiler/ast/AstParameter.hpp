#pragma once

#include <script/compiler/ast/AstDeclaration.hpp>
#include <script/compiler/ast/AstTypeSpecifier.hpp>
#include <script/compiler/ast/AstExpression.hpp>
#include <core/containers/String.hpp>

namespace hyperion {

class AstParameter : public AstDeclaration
{
public:
    AstParameter(
        const String& name,
        const RC<AstTypeSpecifier>& typeSpec,
        const RC<AstExpression>& defaultParam,
        bool isVariadic,
        IdentifierFlagBits flags,
        const SourceLocation& location);

    virtual ~AstParameter() override = default;

    const RC<AstExpression>& GetDefaultValue() const
    {
        return m_defaultParam;
    }

    void SetDefaultValue(const RC<AstExpression>& defaultParam)
    {
        m_defaultParam = defaultParam;
    }

    virtual void Visit(AstVisitor* visitor, Module* mod) override;
    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override;
    virtual void Optimize(AstVisitor* visitor, Module* mod) override;

    virtual RC<AstStatement> Clone() const override;

    bool IsVariadic() const
    {
        return m_isVariadic;
    }

    // used by AstTemplateExpression
    const RC<AstTypeSpecifier>& GetTypeSpecifier() const
    {
        return m_typeSpec;
    }

    void SetTypeSpecifier(const RC<AstTypeSpecifier>& typeSpec)
    {
        m_typeSpec = typeSpec;
    }

    SymbolTypeRef GetExprType() const;

    virtual HashCode GetHashCode() const override
    {
        HashCode hc = AstDeclaration::GetHashCode().Add(TypeName<AstParameter>());
        hc.Add(m_typeSpec ? m_typeSpec->GetHashCode() : HashCode());
        hc.Add(m_defaultParam ? m_defaultParam->GetHashCode() : HashCode());
        hc.Add(m_isVariadic);

        return hc;
    }

private:
    RC<AstTypeSpecifier> m_typeSpec;
    RC<AstExpression> m_defaultParam;
    bool m_isVariadic : 1;

    // Set while analyzing
    SymbolTypeRef m_symbolType;
    RC<AstExpression> m_varargsTypeSpec;

    RC<AstParameter> CloneImpl() const
    {
        return RC<AstParameter>(new AstParameter(
            m_name,
            CloneAstNode(m_typeSpec),
            CloneAstNode(m_defaultParam),
            m_isVariadic,
            m_flags,
            m_location));
    }
};

} // namespace hyperion
