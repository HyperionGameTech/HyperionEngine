#ifndef AST_MEMBER_HPP
#define AST_MEMBER_HPP

#include <script/compiler/ast/AstIdentifier.hpp>
#include <core/lib/String.hpp>

namespace hyperion::compiler {

class AstMember : public AstExpression
{
public:
    AstMember(
        const String &field_name,
        const RC<AstExpression> &target,
        const SourceLocation &location
    );
    virtual ~AstMember() = default;
    
    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override;
    
    virtual RC<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool MayHaveSideEffects() const override;

    virtual SymbolTypePtr_t GetExprType() const override;
    virtual SymbolTypePtr_t GetHeldType() const override;
  
    virtual const AstExpression *GetValueOf() const override;
    virtual const AstExpression *GetDeepValueOf() const override;
    virtual AstExpression *GetTarget() const override;
    virtual bool IsMutable() const override;

    virtual HashCode GetHashCode() const override
    {
        HashCode hc = AstExpression::GetHashCode().Add(TypeName<AstMember>());
        hc.Add(m_field_name);
        hc.Add(m_target ? m_target->GetHashCode() : HashCode());

        return hc;
    }

protected:
    String              m_field_name;
    RC<AstExpression>   m_target;

    // set while analyzing
    SymbolTypePtr_t     m_symbol_type;
    SymbolTypePtr_t     m_target_type;
    SymbolTypePtr_t     m_held_type;
    RC<AstExpression>   m_proxy_expr;
    RC<AstExpression>   m_override_expr;
    UInt                m_found_index;
    bool                m_enable_generic_member_substitution;

    RC<AstMember> CloneImpl() const
    {
        return RC<AstMember>(new AstMember(
            m_field_name,
            CloneAstNode(m_target),
            m_location
        ));
    }
};

} // namespace hyperion::compiler

#endif
