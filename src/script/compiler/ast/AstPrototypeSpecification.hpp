#ifndef AST_PROTOTYPE_SPECIFICATION_HPP
#define AST_PROTOTYPE_SPECIFICATION_HPP

#include <script/compiler/ast/AstStatement.hpp>
#include <script/compiler/ast/AstTypeHolder.hpp>
#include <script/compiler/type-system/SymbolType.hpp>

#include <string>
#include <memory>

namespace hyperion::compiler {

class AstPrototypeSpecification: public AstStatement {
public:
    AstPrototypeSpecification(
        const std::shared_ptr<AstExpression> &proto,
        const SourceLocation &location);
    virtual ~AstPrototypeSpecification() = default;

    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override;
    
    virtual Pointer<AstStatement> Clone() const override;

    inline SymbolTypePtr_t GetExprType() const { return m_proto != nullptr ? m_proto->GetExprType() : nullptr; }

    inline const SymbolTypePtr_t &GetHeldType() const { return m_symbol_type; }
    inline const SymbolTypePtr_t &GetPrototypeType() const { return m_prototype_type; }
    inline const std::shared_ptr<AstExpression> &GetDefaultValue() const { return m_symbol_type->GetDefaultValue(); }

private:
    std::shared_ptr<AstExpression> m_proto;

    /** Set while analyzing */
    SymbolTypePtr_t m_symbol_type;
    SymbolTypePtr_t m_prototype_type;
    std::shared_ptr<AstExpression> m_default_value;

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
