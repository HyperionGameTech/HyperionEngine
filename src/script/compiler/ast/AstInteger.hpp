#ifndef AST_INTEGER_HPP
#define AST_INTEGER_HPP

#include <script/compiler/ast/AstConstant.hpp>

#include <cstdint>

namespace hyperion::compiler {

class AstInteger : public AstConstant
{
public:
    AstInteger(hyperion::Int32 value, const SourceLocation &location);

    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    
    virtual RC<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool IsNumber() const override;
    virtual hyperion::Int32 IntValue() const override;
    virtual hyperion::UInt32 UnsignedValue() const override;
    virtual hyperion::Float32 FloatValue() const override;
    virtual SymbolTypePtr_t GetExprType() const override;

    virtual RC<AstConstant> HandleOperator(Operators op_type, const AstConstant *right) const override;

    virtual HashCode GetHashCode() const override
    {
        HashCode hc = AstConstant::GetHashCode().Add(TypeName<AstInteger>());
        hc.Add(m_value);

        return hc;
    }

private:
    hyperion::Int32 m_value;

    RC<AstInteger> CloneImpl() const
    {
        return RC<AstInteger>(new AstInteger(
            m_value,
            m_location
        ));
    }
};

} // namespace hyperion::compiler

#endif
