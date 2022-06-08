#ifndef AST_MIXIN_HPP
#define AST_MIXIN_HPP

#include <script/compiler/ast/AstExpression.hpp>

#include <string>
#include <vector>
#include <memory>

namespace hyperion {
namespace compiler {

class AstMixinDeclaration;

class AstMixin : public AstExpression {
public:
    AstMixin(const std::string &name,
        const std::string &mixin_expr,
        const SourceLocation &location);

    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override;
    
    virtual Pointer<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool MayHaveSideEffects() const override;
    virtual SymbolTypePtr_t GetSymbolType() const override;

private:
    std::string m_name;
    std::string m_mixin_expr;

    // set while analyzing
    std::vector<std::shared_ptr<AstStatement>> m_statements;

    inline Pointer<AstMixin> CloneImpl() const
    {
        return Pointer<AstMixin>(new AstMixin(
            m_name,
            m_mixin_expr,
            m_location
        ));
    }
};

} // namespace compiler
} // namespace hyperion

#endif
