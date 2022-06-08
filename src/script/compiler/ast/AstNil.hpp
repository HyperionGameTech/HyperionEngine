#ifndef AST_NIL_HPP
#define AST_NIL_HPP

#include <script/compiler/ast/AstConstant.hpp>

namespace hyperion {
namespace compiler {

class AstNil : public AstConstant {
public:
    AstNil(const SourceLocation &location);

    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    
    virtual Pointer<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool IsNumber() const override;
    virtual hyperion::aint32 IntValue() const override;
    virtual hyperion::afloat32 FloatValue() const override;
    virtual SymbolTypePtr_t GetSymbolType() const override;

    virtual std::shared_ptr<AstConstant> HandleOperator(Operators op_type, AstConstant *right) const override;

private:
    inline Pointer<AstNil> CloneImpl() const
    {
        return Pointer<AstNil>(new AstNil(
            m_location
        ));
    }
};

} // namespace compiler
} // namespace hyperion

#endif
