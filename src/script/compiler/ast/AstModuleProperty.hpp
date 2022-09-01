#ifndef AST_MODULE_PROPERTY_HPP
#define AST_MODULE_PROPERTY_HPP

#include <script/compiler/ast/AstIdentifier.hpp>

namespace hyperion::compiler {

class AstModuleProperty : public AstExpression {
public:
    AstModuleProperty(
      const std::string &field_name,
      const SourceLocation &location
    );
    virtual ~AstModuleProperty() = default;
    
    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override;
    
    virtual Pointer<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool MayHaveSideEffects() const override;
    virtual SymbolTypePtr_t GetExprType() const override;

protected:
    std::string m_field_name;

    // set while analyzing
    SymbolTypePtr_t m_expr_type;
    std::shared_ptr<AstExpression> m_expr_value;

    Pointer<AstModuleProperty> CloneImpl() const
    {
        return Pointer<AstModuleProperty>(new AstModuleProperty(
            m_field_name,
            m_location
        ));
    }
};

} // namespace hyperion::compiler

#endif
