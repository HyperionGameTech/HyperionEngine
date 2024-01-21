#ifndef AST_PROTOTYPE_SPECIFICATION_HPP
#define AST_PROTOTYPE_SPECIFICATION_HPP

#include <script/compiler/ast/AstExpression.hpp>
#include <script/compiler/type-system/SymbolType.hpp>

#include <string>
#include <memory>

namespace hyperion::compiler {

class AstPrototypeSpecification : public AstExpression
{
public:
    AstPrototypeSpecification(
        const RC<AstExpression> &expr,
        const SourceLocation &location
    );
    virtual ~AstPrototypeSpecification() = default;

    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override;
    
    const SymbolTypePtr_t &GetPrototypeType() const { return m_prototype_type; }
    const RC<AstExpression> &GetDefaultValue() const { return m_default_value; }
    virtual const RC<AstExpression> &GetExpr() const { return m_expr; }

    virtual RC<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool MayHaveSideEffects() const override;
    virtual SymbolTypePtr_t GetExprType() const override;
  
    virtual const AstExpression *GetValueOf() const override;
    virtual const AstExpression *GetDeepValueOf() const override;
    
    virtual SymbolTypePtr_t GetHeldType() const override;

    virtual HashCode GetHashCode() const override
    {
        HashCode hc = AstExpression::GetHashCode().Add(TypeName<AstPrototypeSpecification>());
        hc.Add(m_expr ? m_expr->GetHashCode() : HashCode());

        return hc;
    }

protected:
    bool FindPrototypeType(const SymbolTypePtr_t &symbol_type);

    RC<AstExpression>   m_expr;

    /** Set while analyzing */
    SymbolTypePtr_t     m_symbol_type;
    SymbolTypePtr_t     m_prototype_type;
    RC<AstExpression>   m_default_value;

private:
    RC<AstPrototypeSpecification> CloneImpl() const
    {
        return RC<AstPrototypeSpecification>(new AstPrototypeSpecification(
            CloneAstNode(m_expr),
            m_location
        ));
    }
};

} // namespace hyperion::compiler

#endif
