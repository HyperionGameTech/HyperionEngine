#ifndef AST_VARIABLE_HPP
#define AST_VARIABLE_HPP

#include <script/compiler/ast/AstIdentifier.hpp>
#include <script/compiler/ast/AstMember.hpp>

namespace hyperion {
namespace compiler {

class AstVariable : public AstIdentifier {
public:
    AstVariable(const std::string &name, const SourceLocation &location);
    virtual ~AstVariable() = default;

    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override;
    
    virtual Pointer<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool MayHaveSideEffects() const override;

private:
    // set while analyzing
    // used to get locals from outer function in a closure
    std::shared_ptr<AstMember> m_closure_member_access;
    // inline value
    std::shared_ptr<AstExpression> m_inline_value;
    bool m_should_inline;

    inline Pointer<AstVariable> CloneImpl() const
    {
        return Pointer<AstVariable>(new AstVariable(
            m_name,
            m_location
        ));
    }
};

} // namespace compiler
} // namespace hyperion

#endif
