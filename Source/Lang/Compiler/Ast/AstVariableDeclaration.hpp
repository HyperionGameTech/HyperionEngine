#pragma once

#include <Lang/Compiler/Ast/AstDeclaration.hpp>
#include <Lang/Compiler/Ast/AstExpression.hpp>
#include <Lang/Compiler/Ast/AstParameter.hpp>
#include <Lang/Compiler/Ast/AstTypeSpecifier.hpp>
#include <Lang/Compiler/Identifier.hpp>
#include <Lang/Compiler/TypeSystem/SymbolType.hpp>

#include <Core/Containers/String.hpp>

#include <memory>

namespace Hyperion {

class AstVariableDeclaration : public AstDeclaration
{
public:
    AstVariableDeclaration(
        const String& name,
        const RC<AstTypeSpecifier>& typeSpec,
        const RC<AstExpression>& assignment,
        EnumFlags<IdentifierFlags> flags,
        const SourceLocation& location);

    virtual ~AstVariableDeclaration() = default;

    HYP_FORCE_INLINE const RC<AstTypeSpecifier>& GetTypeSpecifier() const
    {
        return m_typeSpec;
    }

    HYP_FORCE_INLINE void SetTypeSpecifier(const RC<AstTypeSpecifier>& typeSpec)
    {
        m_typeSpec = typeSpec;
    }

    HYP_FORCE_INLINE const RC<AstExpression>& GetAssignment() const
    {
        return m_assignment;
    }

    HYP_FORCE_INLINE void SetAssignment(const RC<AstExpression>& assignment)
    {
        m_assignment = assignment;
    }

    HYP_FORCE_INLINE const RC<AstExpression>& GetRealAssignment() const
    {
        return m_realAssignment;
    }

    virtual void Visit(AstVisitor* visitor, Module* mod) override;
    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override;
    virtual void Optimize(AstVisitor* visitor, Module* mod) override;

    virtual RC<AstStatement> Clone() const override;

    HYP_FORCE_INLINE const SymbolType* GetSymbolType() const
    {
        return m_symbolType;
    }

    virtual HashCode GetHashCode() const override
    {
        HashCode hc = AstDeclaration::GetHashCode().Add(TypeName<AstVariableDeclaration>());
        hc.Add(AstDeclaration::GetHashCode());
        hc.Add(m_typeSpec ? m_typeSpec->GetHashCode() : HashCode());
        hc.Add(m_assignment ? m_assignment->GetHashCode() : HashCode());

        return hc;
    }

protected:
    RC<AstTypeSpecifier> m_typeSpec;
    RC<AstExpression> m_assignment;

    // set while analyzing
    RC<AstExpression> m_realAssignment;

    const SymbolType* m_symbolType;

    RC<AstVariableDeclaration> CloneImpl() const
    {
        return RC<AstVariableDeclaration>(new AstVariableDeclaration(
            m_name,
            CloneAstNode(m_typeSpec),
            CloneAstNode(m_assignment),
            m_flags,
            m_location));
    }
};

} // namespace Hyperion
