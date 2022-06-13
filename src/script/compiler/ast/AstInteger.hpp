#ifndef AST_INTEGER_HPP
#define AST_INTEGER_HPP

#include <script/compiler/ast/AstConstant.hpp>

#include <cstdint>

namespace hyperion::compiler {

class AstInteger : public AstConstant {
public:
    AstInteger(hyperion::aint32 value, const SourceLocation &location);

    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    
    virtual Pointer<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool IsNumber() const override;
    virtual hyperion::aint32 IntValue() const override;
    virtual hyperion::auint32 UnsignedValue() const override;
    virtual hyperion::afloat32 FloatValue() const override;
    virtual SymbolTypePtr_t GetExprType() const override;

    virtual std::shared_ptr<AstConstant> HandleOperator(Operators op_type, const AstConstant *right) const override;

private:
    hyperion::aint32 m_value;

    inline Pointer<AstInteger> CloneImpl() const
    {
        return Pointer<AstInteger>(new AstInteger(
            m_value,
            m_location
        ));
    }
};

} // namespace hyperion::compiler

#endif
