#ifndef AST_MEMBER_HPP
#define AST_MEMBER_HPP

#include <script/compiler/ast/AstIdentifier.hpp>

namespace hyperion {
namespace compiler {

class AstMember : public AstExpression {
public:
    AstMember(
      const std::string &field_name,
      const std::shared_ptr<AstExpression> &target,
      const SourceLocation &location
    );
    virtual ~AstMember() = default;

    inline const std::shared_ptr<AstExpression> &GetTarget() const
      { return m_target; }
    
    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override;
    
    virtual Pointer<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool MayHaveSideEffects() const override;
    virtual SymbolTypePtr_t GetSymbolType() const override;

protected:
    std::string m_field_name;
    std::shared_ptr<AstExpression> m_target;

    // set while analyzing
    SymbolTypePtr_t m_symbol_type;
    SymbolTypePtr_t m_target_type;

    inline Pointer<AstMember> CloneImpl() const
    {
        return Pointer<AstMember>(new AstMember(
            m_field_name,
            CloneAstNode(m_target),
            m_location
        ));
    }
};

} // namespace compiler
} // namespace hyperion

#endif
