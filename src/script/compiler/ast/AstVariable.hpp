#ifndef AST_VARIABLE_HPP
#define AST_VARIABLE_HPP

#include <script/compiler/ast/AstIdentifier.hpp>
#include <script/compiler/ast/AstMember.hpp>

namespace hyperion::compiler {

class AstVariable : public AstIdentifier
{
public:
    AstVariable(const std::string &name, const SourceLocation &location);
    virtual ~AstVariable() = default;

    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override;
    
    virtual bool IsLiteral() const override;
    virtual RC<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool MayHaveSideEffects() const override;
    virtual SymbolTypePtr_t GetExprType() const override;
    virtual bool IsMutable() const override;

private:
    // set while analyzing
    // used to get locals from outer function in a closure
    RC<AstMember> m_closure_member_access;
    RC<AstMember> m_self_member_access;
    RC<AstExpression> m_inline_value;
    bool m_should_inline;
    bool m_is_in_ref_assignment;
    bool m_is_in_const_assignment;

    RC<AstVariable> CloneImpl() const
    {
        return RC<AstVariable>(new AstVariable(
            m_name,
            m_location
        ));
    }
};

} // namespace hyperion::compiler

#endif
