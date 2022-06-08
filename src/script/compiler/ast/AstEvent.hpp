#ifndef AST_EVENT_HPP
#define AST_EVENT_HPP

#include <script/compiler/ast/AstExpression.hpp>
#include <script/compiler/ast/AstConstant.hpp>
#include <script/compiler/ast/AstFunctionExpression.hpp>

namespace hyperion {
namespace compiler {

class AstEvent : public AstExpression {
public:
    AstEvent(const std::shared_ptr<AstFunctionExpression> &trigger,
      const SourceLocation &location);
    virtual ~AstEvent() = default;

    inline const std::shared_ptr<AstFunctionExpression> &GetTrigger() const
      { return m_trigger; }

    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override;
    

    virtual Tribool IsTrue() const override;
    virtual bool MayHaveSideEffects() const override;
    virtual SymbolTypePtr_t GetSymbolType() const override;
    virtual std::shared_ptr<AstExpression> GetKey() const = 0;
    virtual std::string GetKeyName() const = 0;

protected:
    std::shared_ptr<AstFunctionExpression> m_trigger;
};

class AstConstantEvent : public AstEvent {
public:
    AstConstantEvent(const std::shared_ptr<AstConstant> &key,
      const std::shared_ptr<AstFunctionExpression> &trigger,
      const SourceLocation &location);
    virtual ~AstConstantEvent() = default;

    virtual std::shared_ptr<AstExpression> GetKey() const override;
    virtual std::string GetKeyName() const override;

    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override;
    
    virtual Pointer<AstStatement> Clone() const override;

private:
    std::shared_ptr<AstConstant> m_key;

    inline Pointer<AstConstantEvent> CloneImpl() const
    {
        return Pointer<AstConstantEvent>(new AstConstantEvent(
            CloneAstNode(m_key),
            CloneAstNode(m_trigger),
            m_location
        ));
    }
};

} // namespace compiler
} // namespace hyperion

#endif
