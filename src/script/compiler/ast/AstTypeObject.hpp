#ifndef AST_TYPE_OBJECT_HPP
#define AST_TYPE_OBJECT_HPP

#include <script/compiler/ast/AstExpression.hpp>
#include <script/compiler/ast/AstVariable.hpp>

namespace hyperion::compiler {

class AstTypeObject : public AstExpression {
public:
    AstTypeObject(const SymbolTypePtr_t &symbol_type,
        const std::shared_ptr<AstVariable> &proto,
        const SourceLocation &location);
    virtual ~AstTypeObject() = default;

    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override;
    
    virtual Pointer<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool MayHaveSideEffects() const override;
    virtual SymbolTypePtr_t GetExprType() const override;

    const SymbolTypePtr_t &GetHeldType() const { return m_symbol_type; }

private:
    SymbolTypePtr_t m_symbol_type;
    std::shared_ptr<AstVariable> m_proto;

    Pointer<AstTypeObject> CloneImpl() const
    {
        return Pointer<AstTypeObject>(new AstTypeObject(
            m_symbol_type,
            CloneAstNode(m_proto),
            m_location
        ));
    }
};

} // namespace hyperion::compiler

#endif
