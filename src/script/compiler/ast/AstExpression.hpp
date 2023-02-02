#ifndef AST_EXPRESSION_HPP
#define AST_EXPRESSION_HPP

#include <script/compiler/ast/AstStatement.hpp>
#include <script/compiler/type-system/SymbolType.hpp>
#include <script/compiler/Enums.hpp>

#include <script/Tribool.hpp>

namespace hyperion::compiler {

class AstTypeObject;

using ExprAccessBits = UInt32;

enum ExprAccess : ExprAccessBits
{
    EXPR_ACCESS_PUBLIC    = 0x1,
    EXPR_ACCESS_PRIVATE   = 0x2,
    EXPR_ACCESS_PROTECTED = 0x4
};

class AstExpression : public AstStatement
{
public:
    AstExpression(
        const SourceLocation &location,
        int access_options
    );
    virtual ~AstExpression() = default;

    int GetAccessOptions() const
        { return m_access_options; }

    AccessMode GetAccessMode() const
      { return m_access_mode; }

    void SetAccessMode(AccessMode access_mode)
      { m_access_mode = access_mode; }

    virtual void Visit(AstVisitor *visitor, Module *mod) override = 0;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override = 0;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override = 0;
    
    virtual Pointer<AstStatement> Clone() const override = 0;

    /**
     * Overridden by derived classes to allow "constexpr"-type functionality.
     */
    virtual bool IsLiteral() const { return false; }
    virtual const AstExpression *GetValueOf() const { return this; }
    virtual const AstExpression *GetDeepValueOf() const { return GetValueOf(); }
    virtual AstExpression *GetTarget() const { return nullptr; }
    virtual AstExpression *GetHeldGenericExpr() const { return nullptr; }

    /** Determine whether the expression would evaluate to true.
        Returns -1 if it cannot be evaluated at compile time.
    */
    virtual Tribool IsTrue() const = 0;

    /** Determine whether or not there is a possibility of side effects. */
    virtual bool MayHaveSideEffects() const = 0;
    virtual SymbolTypePtr_t GetExprType() const = 0;
    virtual ExprAccessBits GetExprAccess() const { return ExprAccess::EXPR_ACCESS_PUBLIC; }
    virtual bool IsMutable() const { return false; }
    
protected:
    AccessMode m_access_mode;
    int m_access_options;
};

} // namespace hyperion::compiler

#endif
