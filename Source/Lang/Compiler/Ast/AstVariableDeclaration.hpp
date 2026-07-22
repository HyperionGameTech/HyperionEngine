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

HYP_CLASS()
class AstVariableDeclaration : public AstDeclaration
{
    HYP_OBJECT_BODY(AstVariableDeclaration);

public:
    AstVariableDeclaration(
        const String& name,
        const Handle<AstTypeSpecifier>& typeSpec,
        const Handle<AstExpression>& assignment,
        EnumFlags<IdentifierFlags> flags,
        const SourceLocation& location);

    virtual ~AstVariableDeclaration() = default;

    HYP_FORCE_INLINE const Handle<AstTypeSpecifier>& GetTypeSpecifier() const
    {
        return m_typeSpec;
    }

    HYP_FORCE_INLINE void SetTypeSpecifier(const Handle<AstTypeSpecifier>& typeSpec)
    {
        m_typeSpec = typeSpec;
    }

    HYP_FORCE_INLINE const Handle<AstExpression>& GetAssignment() const
    {
        return m_assignment;
    }

    HYP_FORCE_INLINE void SetAssignment(const Handle<AstExpression>& assignment)
    {
        m_assignment = assignment;
    }

    HYP_FORCE_INLINE const Handle<AstExpression>& GetRealAssignment() const
    {
        return m_realAssignment;
    }

    virtual void Visit(AstVisitor* visitor, Module* mod) override;
    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override;
    virtual void Optimize(AstVisitor* visitor, Module* mod) override;

    virtual Handle<AstStatement> Clone() const override;

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
    Handle<AstTypeSpecifier> m_typeSpec;
    Handle<AstExpression> m_assignment;

    // set while analyzing
    Handle<AstExpression> m_realAssignment;

    const SymbolType* m_symbolType;

    Handle<AstVariableDeclaration> CloneImpl() const
    {
        return MakeHandle<AstVariableDeclaration>(
            m_name,
            CloneAstNode(m_typeSpec),
            CloneAstNode(m_assignment),
            m_flags,
            m_location);
    }
};

} // namespace Hyperion
