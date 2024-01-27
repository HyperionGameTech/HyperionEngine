#ifndef AST_EXPRESSION_HPP
#define AST_EXPRESSION_HPP

#include <script/compiler/ast/AstStatement.hpp>
#include <script/compiler/type-system/SymbolType.hpp>
#include <script/compiler/Enums.hpp>
#include <script/Tribool.hpp>

#include <HashCode.hpp>

namespace hyperion::compiler {

class AstTypeObject;

using ExprAccess = UInt32;

enum ExprAccessBits : ExprAccess
{
    EXPR_ACCESS_NONE      = 0x0,
    EXPR_ACCESS_PUBLIC    = 0x1,
    EXPR_ACCESS_PRIVATE   = 0x2,
    EXPR_ACCESS_PROTECTED = 0x4
};

using ExpressionFlags = UInt32;

enum ExpressionFlagBits : ExpressionFlags
{
    EXPR_FLAGS_NONE                   = 0x0,
    EXPR_FLAGS_CONSTRUCTOR_DEFINITION = 0x1
};

class AstExpression : public AstStatement
{
public:
    AstExpression(
        const SourceLocation &location,
        int access_options
    );
    virtual ~AstExpression() = default;

    Int GetAccessOptions() const
        { return m_access_options; }

    AccessMode GetAccessMode() const
        { return m_access_mode; }

    void SetAccessMode(AccessMode access_mode)
        { m_access_mode = access_mode; }

    virtual void Visit(AstVisitor *visitor, Module *mod) override = 0;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override = 0;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override = 0;
    
    virtual RC<AstStatement> Clone() const override = 0;

    /**
     * Overridden by derived classes to allow "constexpr"-type functionality.
     */
    virtual bool IsLiteral() const { return false; }
    virtual const AstExpression *GetValueOf() const { return this; }
    virtual const AstExpression *GetDeepValueOf() const { return GetValueOf(); }
    virtual AstExpression *GetTarget() const { return nullptr; }
    virtual const AstExpression *GetHeldGenericExpr() const { return nullptr; }

    /** Determine whether the expression would evaluate to true.
        Returns -1 if it cannot be evaluated at compile time.
    */
    virtual Tribool IsTrue() const = 0;

    /** Determine whether or not there is a possibility of side effects. */
    virtual bool MayHaveSideEffects() const = 0;
    virtual SymbolTypePtr_t GetExprType() const = 0;
    virtual SymbolTypePtr_t GetHeldType() const { return nullptr; }

    virtual HashCode GetHashCode() const override
    {
        return HashCode().Add(TypeName<AstExpression>());
    }

    virtual bool IsMutable() const
        { return false; }

    ExpressionFlags GetExpressionFlags() const
        { return m_expression_flags; }

    void SetExpressionFlags(ExpressionFlags expression_flags)
        { m_expression_flags = expression_flags; }

    void ApplyExpressionFlags(ExpressionFlags expression_flags, bool set = true)
    {
        if (set) {
            m_expression_flags |= expression_flags;
        } else {
            m_expression_flags &= ~expression_flags;
        }
    }
    
protected:
    AccessMode      m_access_mode;
    Int             m_access_options;
    ExpressionFlags m_expression_flags = EXPR_FLAGS_NONE;
};

} // namespace hyperion::compiler

#endif
