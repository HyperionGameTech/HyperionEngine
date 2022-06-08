#ifndef AST_MODULE_ACCESS_HPP
#define AST_MODULE_ACCESS_HPP

#include <script/compiler/ast/AstExpression.hpp>

#include <string>
#include <vector>
#include <memory>

namespace hyperion {
namespace compiler {

class AstModuleAccess : public AstExpression {
public:
    AstModuleAccess(const std::string &target,
        const std::shared_ptr<AstExpression> &expr,
        const SourceLocation &location);
    virtual ~AstModuleAccess() = default;

    inline Module *GetModule() { return m_mod_access; }
    inline const Module *GetModule() const { return m_mod_access; }
    inline const std::string &GetTarget() const { return m_target; }
    inline const std::shared_ptr<AstExpression> &GetExpression() const { return m_expr; }
    inline void SetExpression(const std::shared_ptr<AstExpression> &expr) { m_expr = expr; }
    inline void SetChained(bool is_chained) { m_is_chained = is_chained; }

    void PerformLookup(AstVisitor *visitor, Module *mod);

    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override;
    
    virtual Pointer<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool MayHaveSideEffects() const override;
    virtual SymbolTypePtr_t GetSymbolType() const override;

private:
    std::string m_target;
    std::shared_ptr<AstExpression> m_expr;
    Module *m_mod_access;
    // is this module access chained to another before it?
    bool m_is_chained;
    bool m_looked_up;

    inline Pointer<AstModuleAccess> CloneImpl() const
    {
        return Pointer<AstModuleAccess>(new AstModuleAccess(
            m_target,
            CloneAstNode(m_expr),
            m_location
        ));
    }
};

} // namespace compiler
} // namespace hyperion

#endif
