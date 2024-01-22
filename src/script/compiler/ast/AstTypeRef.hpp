#ifndef AST_TYPE_REF_HPP
#define AST_TYPE_REF_HPP

#include <script/compiler/ast/AstExpression.hpp>

namespace hyperion::compiler {

class AstTypeRef : public AstExpression
{
public:
    AstTypeRef(
        const SymbolTypePtr_t &symbol_type,
        const SourceLocation &location
    );

    virtual ~AstTypeRef() = default;

    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override;
    
    virtual RC<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool MayHaveSideEffects() const override;

    virtual SymbolTypePtr_t GetExprType() const override;
    virtual SymbolTypePtr_t GetHeldType() const override;

    virtual HashCode GetHashCode() const override
    {
        HashCode hc = AstExpression::GetHashCode().Add(TypeName<AstTypeRef>());
        hc.Add(m_symbol_type ? m_symbol_type->GetHashCode() : HashCode());

        return hc;
    }

private:
    SymbolTypePtr_t             m_symbol_type;
    
    // set while analyzing
    bool                        m_is_visited;

    RC<AstTypeRef> CloneImpl() const
    {
        return RC<AstTypeRef>(new AstTypeRef(
            m_symbol_type,
            m_location
        ));
    }
};

} // namespace hyperion::compiler

#endif
