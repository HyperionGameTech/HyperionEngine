#pragma once

#include <script/compiler/ast/AstStatement.hpp>
#include <script/compiler/Scope.hpp>

#include <vector>
#include <memory>

namespace Hyperion {

class Scope;

class AstBlock : public AstStatement
{
public:
    AstBlock(
        const Array<RC<AstStatement>>& children,
        const SourceLocation& location);

    AstBlock(const SourceLocation& location);

    virtual ~AstBlock() = default;

    HYP_FORCE_INLINE void PrependChild(const RC<AstStatement>& stmt)
    {
        m_children.PushFront(stmt);
    }

    HYP_FORCE_INLINE void AddChild(const RC<AstStatement>& stmt)
    {
        m_children.PushBack(stmt);
    }

    HYP_FORCE_INLINE Array<RC<AstStatement>>& GetChildren()
    {
        return m_children;
    }

    HYP_FORCE_INLINE const Array<RC<AstStatement>>& GetChildren() const
    {
        return m_children;
    }

    HYP_FORCE_INLINE int NumLocals() const
    {
        return m_numLocals;
    }

    HYP_FORCE_INLINE bool IsLastStatementReturn() const
    {
        return m_lastIsReturn;
    }

    HYP_FORCE_INLINE bool IsLastStatementExpr() const
    {
        return m_lastIsExpr;
    }

    HYP_FORCE_INLINE const SymbolType* GetLastExprType() const
    {
        return m_lastExprType;
    }

    HYP_FORCE_INLINE Scope* GetScope() const
    {
        return m_scope;
    }

    HYP_FORCE_INLINE ScopeType GetScopeType() const
    {
        return m_scopeType;
    }

    HYP_FORCE_INLINE void SetScopeType(ScopeType scopeType)
    {
        m_scopeType = scopeType;
    }

    HYP_FORCE_INLINE int ScopeFlags() const
    {
        return m_scopeFlags;
    }

    HYP_FORCE_INLINE void SetScopeFlags(int scopeFlags)
    {
        m_scopeFlags = scopeFlags;
    }

    virtual void Visit(AstVisitor* visitor, Module* mod) override;
    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override;
    virtual void Optimize(AstVisitor* visitor, Module* mod) override;

    virtual RC<AstStatement> Clone() const override;

    virtual HashCode GetHashCode() const override
    {
        HashCode hc;
        hc.Add(TypeName<AstBlock>());

        for (auto& child : m_children)
        {
            hc.Add(child ? child->GetHashCode() : HashCode());
        }

        return hc;
    }

protected:
    Array<RC<AstStatement>> m_children;

    // set while analyzing
    int m_numLocals;
    bool m_lastIsReturn : 1;
    bool m_lastIsExpr : 1;
    const SymbolType* m_lastExprType;
    Scope* m_scope = nullptr;
    ScopeType m_scopeType = ScopeType::SCOPE_TYPE_NORMAL;
    int m_scopeFlags = 0;

    RC<AstBlock> CloneImpl() const
    {
        return RC<AstBlock>(new AstBlock(
            CloneAllAstNodes(m_children),
            m_location));
    }
};

} // namespace Hyperion
