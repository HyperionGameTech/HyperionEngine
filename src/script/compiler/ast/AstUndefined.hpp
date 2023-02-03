#ifndef AST_UNDEFINED_HPP
#define AST_UNDEFINED_HPP

#include <script/compiler/ast/AstConstant.hpp>

namespace hyperion::compiler {

class AstUndefined : public AstConstant
{
public:
    AstUndefined(const SourceLocation &location);

    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    
    virtual RC<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool IsNumber() const override;
    virtual hyperion::Int32 IntValue() const override;
    virtual hyperion::Float32 FloatValue() const override;
    virtual SymbolTypePtr_t GetExprType() const override;
    
    virtual RC<AstConstant> HandleOperator(Operators op_type, const AstConstant *right) const override;

private:
    RC<AstUndefined> CloneImpl() const
    {
        return RC<AstUndefined>(new AstUndefined(
            m_location
        ));
    }
};

} // namespace hyperion::compiler

#endif
