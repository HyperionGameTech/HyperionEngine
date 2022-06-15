#ifndef AST_PROTOTYPE_SPECIFICATION_HPP
#define AST_PROTOTYPE_SPECIFICATION_HPP

#include <script/compiler/ast/AstExpression.hpp>
#include <script/compiler/ast/AstTypeHolder.hpp>
#include <script/compiler/type-system/SymbolType.hpp>

#include <string>
#include <memory>

namespace hyperion::compiler {

class AstPrototypeSpecification: public AstExpression {
public:
    AstPrototypeSpecification(
        const std::shared_ptr<AstExpression> &proto,
        const SourceLocation &location
    );
    virtual ~AstPrototypeSpecification() = default;

    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override;
    
    inline const SymbolTypePtr_t &GetHeldType() const { return m_symbol_type; }
    inline const SymbolTypePtr_t &GetPrototypeType() const { return m_prototype_type; }
    inline const std::shared_ptr<AstExpression> &GetDefaultValue() const { return m_default_value; }//m_symbol_type->GetDefaultValue(); }
    inline const std::shared_ptr<AstExpression> &GetExpr() const { return m_proto; }

    virtual Pointer<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool MayHaveSideEffects() const override;
    virtual SymbolTypePtr_t GetExprType() const override;
  
    virtual const AstExpression *GetValueOf() const override;
    virtual const AstExpression *GetDeepValueOf() const override;

protected:
    bool FindPrototypeType(const SymbolTypePtr_t &symbol_type);

    std::shared_ptr<AstExpression> m_proto;

    /** Set while analyzing */
    SymbolTypePtr_t m_symbol_type;
    SymbolTypePtr_t m_prototype_type;
    std::shared_ptr<AstExpression> m_default_value;

private:
    inline Pointer<AstPrototypeSpecification> CloneImpl() const
    {
        return Pointer<AstPrototypeSpecification>(new AstPrototypeSpecification(
            CloneAstNode(m_proto),
            m_location
        ));
    }
};

} // namespace hyperion::compiler

#endif
