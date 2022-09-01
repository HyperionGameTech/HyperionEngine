#ifndef AST_TYPE_NAME_HPP
#define AST_TYPE_NAME_HPP

#include <script/compiler/ast/AstIdentifier.hpp>
#include <script/compiler/ast/AstMember.hpp>

namespace hyperion::compiler {

class AstTypeName : public AstIdentifier {
public:
    AstTypeName(const std::string &name, const SourceLocation &location);
    virtual ~AstTypeName() = default;

    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override;
    
    virtual bool IsLiteral() const override;
    virtual Pointer<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool MayHaveSideEffects() const override;
    virtual SymbolTypePtr_t GetExprType() const override;

private:
    SymbolTypePtr_t m_symbol_type;

    Pointer<AstTypeName> CloneImpl() const
    {
        return Pointer<AstTypeName>(new AstTypeName(
            m_name,
            m_location
        ));
    }
};

} // namespace hyperion::compiler

#endif
