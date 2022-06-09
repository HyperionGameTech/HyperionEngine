#ifndef AST_META_BLOCK_HPP
#define AST_META_BLOCK_HPP

#include <script/compiler/ast/AstStatement.hpp>
#include <script/compiler/ast/AstBlock.hpp>
#include <script/compiler/ast/AstFunctionExpression.hpp>

#include <memory>
#include <vector>

namespace hyperion::compiler {

class AstMetaBlock : public AstStatement {
public:
    AstMetaBlock(const std::vector<std::shared_ptr<AstStatement>> &children,
        const SourceLocation &location);
    virtual ~AstMetaBlock() = default;

    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override;
    
    virtual Pointer<AstStatement> Clone() const override;

protected:
    std::vector<std::shared_ptr<AstStatement>> m_children;

    // set while analyzing
    std::shared_ptr<AstFunctionExpression> m_result_closure;

    inline Pointer<AstMetaBlock> CloneImpl() const
    {
        return Pointer<AstMetaBlock>(new AstMetaBlock(
            CloneAllAstNodes(m_children),
            m_location
        ));
    }
};

} // namespace hyperion::compiler

#endif
