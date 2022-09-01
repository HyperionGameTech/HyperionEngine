#ifndef AST_BLOCK_HPP
#define AST_BLOCK_HPP

#include <script/compiler/ast/AstStatement.hpp>

#include <vector>
#include <memory>

namespace hyperion::compiler {

class AstBlock : public AstStatement {
public:
    AstBlock(const std::vector<std::shared_ptr<AstStatement>> &children,
        const SourceLocation &location);
    AstBlock(const SourceLocation &location);

    void AddChild(const std::shared_ptr<AstStatement> &stmt)
        { m_children.push_back(stmt); }
    std::vector<std::shared_ptr<AstStatement>> &GetChildren()
        { return m_children; }
    const std::vector<std::shared_ptr<AstStatement>> &GetChildren() const
        { return m_children; }
    int NumLocals() const
        { return m_num_locals; }
    bool IsLastStatementReturn() const
        { return m_last_is_return; }

    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override;
    
    virtual Pointer<AstStatement> Clone() const override;

protected:
    std::vector<std::shared_ptr<AstStatement>> m_children;
    int m_num_locals;
    bool m_last_is_return;

    Pointer<AstBlock> CloneImpl() const
    {
        return std::shared_ptr<AstBlock>(new AstBlock(
            CloneAllAstNodes(m_children),
            m_location
        ));
    }
};

} // namespace hyperion::compiler

#endif
