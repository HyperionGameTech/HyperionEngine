#ifndef AST_OBJECT_HPP
#define AST_OBJECT_HPP

#include <script/compiler/ast/AstExpression.hpp>

namespace hyperion::compiler {

class AstObject : public AstExpression {
public:
    AstObject(const SymbolTypeWeakPtr_t &symbol_type, const SourceLocation &location);
    virtual ~AstObject() = default;

    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override;
    
    virtual RC<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool MayHaveSideEffects() const override;
    virtual SymbolTypePtr_t GetExprType() const override;

private:
    SymbolTypeWeakPtr_t m_symbol_type;

    RC<AstObject> CloneImpl() const
    {
        return RC<AstObject>(new AstObject(
            m_symbol_type,
            m_location
        ));
    }
};

} // namespace hyperion::compiler

#endif
