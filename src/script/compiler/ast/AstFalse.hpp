#ifndef AST_FALSE_HPP
#define AST_FALSE_HPP

#include <script/compiler/ast/AstConstant.hpp>

namespace hyperion::compiler {

class AstFalse : public AstConstant {
public:
    AstFalse(const SourceLocation &location);

    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;

    virtual Pointer<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool IsNumber() const override;
    virtual hyperion::aint32 IntValue() const override;
    virtual hyperion::afloat32 FloatValue() const override;
    virtual SymbolTypePtr_t GetExprType() const override;

    virtual std::shared_ptr<AstConstant> HandleOperator(Operators op_type, const AstConstant *right) const override;

private:
    inline Pointer<AstFalse> CloneImpl() const
    {
        return Pointer<AstFalse>(new AstFalse(
            m_location
        ));
    }
};

} // namespace hyperion::compiler

#endif
