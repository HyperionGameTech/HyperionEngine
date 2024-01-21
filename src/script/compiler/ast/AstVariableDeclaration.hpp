#ifndef AST_VARIABLE_DECLARATION_HPP
#define AST_VARIABLE_DECLARATION_HPP

#include <script/compiler/ast/AstDeclaration.hpp>
#include <script/compiler/ast/AstExpression.hpp>
#include <script/compiler/ast/AstParameter.hpp>
#include <script/compiler/ast/AstPrototypeSpecification.hpp>
#include <script/compiler/Identifier.hpp>
#include <script/compiler/type-system/SymbolType.hpp>

#include <core/lib/String.hpp>

#include <memory>

namespace hyperion::compiler {

class AstVariableDeclaration : public AstDeclaration
{
public:
    AstVariableDeclaration(
        const String &name,
        const RC<AstPrototypeSpecification> &proto,
        const RC<AstExpression> &assignment,
        IdentifierFlagBits flags,
        const SourceLocation &location
    );
    virtual ~AstVariableDeclaration() = default;

    const RC<AstPrototypeSpecification> &GetPrototypeSpecification() const
        { return m_proto; }

    void SetPrototypeSpecification(const RC<AstPrototypeSpecification> &proto)
        { m_proto = proto; }

    const RC<AstExpression> &GetAssignment() const
        { return m_assignment; }

    void SetAssignment(const RC<AstExpression> &assignment)
        { m_assignment = assignment; }

    const RC<AstExpression> &GetRealAssignment() const
        { return m_real_assignment; }

    bool IsConst() const { return m_flags & IdentifierFlags::FLAG_CONST; }
    bool IsRef() const { return m_flags & IdentifierFlags::FLAG_REF; }
    bool IsGeneric() const { return m_flags & IdentifierFlags::FLAG_GENERIC; }

    IdentifierFlagBits GetIdentifierFlags() const
        { return m_flags; }
    
    void SetIdentifierFlags(IdentifierFlagBits flags)
        { m_flags = flags; }

    void ApplyIdentifierFlags(IdentifierFlagBits flags, bool set = true)
    {
        if (set) {
            m_flags |= flags;
        } else {
            m_flags &= ~flags;
        }
    }

    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override;
    
    virtual RC<AstStatement> Clone() const override;

    SymbolTypePtr_t GetExprType() const
        { return m_symbol_type; }

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
    RC<AstPrototypeSpecification>   m_proto;
    RC<AstExpression>               m_assignment;
    IdentifierFlagBits              m_flags;

    // set while analyzing
    RC<AstExpression>               m_real_assignment;

    SymbolTypePtr_t                 m_symbol_type;

    RC<AstVariableDeclaration> CloneImpl() const
    {
        return RC<AstVariableDeclaration>(new AstVariableDeclaration(
            m_name,
            CloneAstNode(m_proto),
            CloneAstNode(m_assignment),
            m_flags,
            m_location
        ));
    }
};

} // namespace hyperion::compiler

#endif
