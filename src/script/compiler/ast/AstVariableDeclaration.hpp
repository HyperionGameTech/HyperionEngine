#pragma once

#include <script/compiler/ast/AstDeclaration.hpp>
#include <script/compiler/ast/AstExpression.hpp>
#include <script/compiler/ast/AstParameter.hpp>
#include <script/compiler/ast/AstTypeSpecifier.hpp>
#include <script/compiler/Identifier.hpp>
#include <script/compiler/type-system/SymbolType.hpp>

#include <core/containers/String.hpp>

#include <memory>

namespace hyperion {

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

    const RC<AstTypeSpecifier>& GetTypeSpecifier() const
    {
        return m_typeSpec;
    }

    void SetTypeSpecifier(const RC<AstTypeSpecifier>& typeSpec)
    {
        m_typeSpec = typeSpec;
    }

    const RC<AstExpression>& GetAssignment() const
    {
        return m_assignment;
    }

    void SetAssignment(const RC<AstExpression>& assignment)
    {
        m_assignment = assignment;
    }

    const RC<AstExpression>& GetRealAssignment() const
    {
        return m_realAssignment;
    }

    virtual void Visit(AstVisitor* visitor, Module* mod) override;
    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override;
    virtual void Optimize(AstVisitor* visitor, Module* mod) override;

    virtual RC<AstStatement> Clone() const override;

    SymbolTypeRef GetExprType() const
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

    SymbolTypeRef m_symbolType;

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

} // namespace hyperion
