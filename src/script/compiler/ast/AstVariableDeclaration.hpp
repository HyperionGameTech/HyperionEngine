#pragma once

#include <script/compiler/ast/AstDeclaration.hpp>
#include <script/compiler/ast/AstExpression.hpp>
#include <script/compiler/ast/AstParameter.hpp>
#include <script/compiler/ast/AstTypeSpecifier.hpp>
#include <script/compiler/Identifier.hpp>
#include <script/compiler/type-system/SymbolType.hpp>

#include <core/containers/String.hpp>

#include <memory>

namespace hyperion::compiler {

class AstVariableDeclaration : public AstDeclaration
{
public:
    AstVariableDeclaration(
        const String& name,
        const RC<AstTypeSpecifier>& proto,
        const RC<AstExpression>& assignment,
        IdentifierFlagBits flags,
        const SourceLocation& location);
    virtual ~AstVariableDeclaration() = default;

    const RC<AstTypeSpecifier>& GetTypeSpecifier() const
    {
        return m_proto;
    }

    void SetTypeSpecifier(const RC<AstTypeSpecifier>& proto)
    {
        m_proto = proto;
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

    bool IsConst() const
    {
        return m_flags & IdentifierFlags::FLAG_CONST;
    }

    bool IsRef() const
    {
        return m_flags & IdentifierFlags::FLAG_REF;
    }

    IdentifierFlagBits GetIdentifierFlags() const
    {
        return m_flags;
    }

    void SetIdentifierFlags(IdentifierFlagBits flags)
    {
        m_flags = flags;
    }

    void ApplyIdentifierFlags(IdentifierFlagBits flags, bool set = true)
    {
        if (set)
        {
            m_flags |= flags;
        }
        else
        {
            m_flags &= ~flags;
        }
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
        hc.Add(m_proto ? m_proto->GetHashCode() : HashCode());
        hc.Add(m_assignment ? m_assignment->GetHashCode() : HashCode());
        hc.Add(m_flags);

        return hc;
    }

protected:
    RC<AstTypeSpecifier> m_proto;
    RC<AstExpression> m_assignment;
    IdentifierFlagBits m_flags;

    // set while analyzing
    RC<AstExpression> m_realAssignment;

    SymbolTypeRef m_symbolType;

    RC<AstVariableDeclaration> CloneImpl() const
    {
        return RC<AstVariableDeclaration>(new AstVariableDeclaration(
            m_name,
            CloneAstNode(m_proto),
            CloneAstNode(m_assignment),
            m_flags,
            m_location));
    }
};

} // namespace hyperion::compiler
