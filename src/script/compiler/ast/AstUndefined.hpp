#ifndef AST_UNDEFINED_HPP
#define AST_UNDEFINED_HPP

#include <script/compiler/ast/AstConstant.hpp>

namespace hyperion {
namespace compiler {

class AstUndefined : public AstConstant {
public:
    AstUndefined(const SourceLocation &location);

    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    
    virtual Pointer<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool IsNumber() const override;
    virtual hyperion::aint32 IntValue() const override;
    virtual hyperion::afloat32 FloatValue() const override;
    virtual SymbolTypePtr_t GetSymbolType() const override;
    
    virtual std::shared_ptr<AstConstant> HandleOperator(Operators op_type, AstConstant *right) const override;

private:
    inline Pointer<AstUndefined> CloneImpl() const
    {
        return Pointer<AstUndefined>(new AstUndefined(
            m_location
        ));
    }
};

} // namespace compiler
} // namespace hyperion

#endif
