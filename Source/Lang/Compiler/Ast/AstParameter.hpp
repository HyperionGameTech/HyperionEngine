#pragma once

#include <Lang/Compiler/Ast/AstDeclaration.hpp>
#include <Lang/Compiler/Ast/AstTypeSpecifier.hpp>
#include <Lang/Compiler/Ast/AstExpression.hpp>
#include <Core/Containers/String.hpp>

namespace Hyperion {

HYP_CLASS()
class AstParameter : public AstDeclaration
{
    HYP_OBJECT_BODY(AstParameter);

public:
    AstParameter(
        const String& name,
        const RC<AstTypeSpecifier>& typeSpec,
        const RC<AstExpression>& defaultParam,
        bool isVariadic,
        EnumFlags<IdentifierFlags> flags,
        const SourceLocation& location);

    virtual ~AstParameter() override = default;

    HYP_FORCE_INLINE const RC<AstExpression>& GetDefaultValue() const
    {
        return m_defaultParam;
    }

    HYP_FORCE_INLINE void SetDefaultValue(const RC<AstExpression>& defaultParam)
    {
        m_defaultParam = defaultParam;
    }

    HYP_FORCE_INLINE bool IsVariadic() const
    {
        return m_isVariadic;
    }

    // used by AstTemplateExpression
    HYP_FORCE_INLINE const RC<AstTypeSpecifier>& GetTypeSpecifier() const
    {
        return m_typeSpec;
    }

    HYP_FORCE_INLINE void SetTypeSpecifier(const RC<AstTypeSpecifier>& typeSpec)
    {
        m_typeSpec = typeSpec;
    }

    HYP_FORCE_INLINE const SymbolType* GetSymbolType() const
    {
        return m_symbolType;
    }

    virtual void Visit(AstVisitor* visitor, Module* mod) override;
    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override;
    virtual void Optimize(AstVisitor* visitor, Module* mod) override;

    virtual RC<AstStatement> Clone() const override;

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
    const SymbolType* m_symbolType;
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

} // namespace Hyperion
