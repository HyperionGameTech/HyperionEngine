#ifndef AST_DIRECTIVE_HPP
#define AST_DIRECTIVE_HPP

#include <script/compiler/ast/AstStatement.hpp>
#include <script/compiler/ast/AstArrayExpression.hpp>

#include <memory>
#include <string>

namespace hyperion::compiler {

class AstDirective : public AstStatement {
public:
    AstDirective(const std::string &key,
        const std::vector<std::string> &args,
        const SourceLocation &location);
    virtual ~AstDirective() = default;

    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override;
    
    virtual Pointer<AstStatement> Clone() const override;

private:
    std::string m_key;
    std::vector<std::string> m_args;

    inline Pointer<AstDirective> CloneImpl() const
    {
        return Pointer<AstDirective>(new AstDirective(
            m_key,
            m_args,
            m_location
        ));
    }
};

} // namespace hyperion::compiler

#endif
