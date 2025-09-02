#pragma once

#include <script/compiler/ast/AstStatement.hpp>
#include <script/compiler/Scope.hpp>

#include <vector>
#include <memory>

namespace hyperion::compiler {

class Scope;

class AstBlock : public AstStatement
{
public:
    AstBlock(
        const Array<RC<AstStatement>>& children,
        const SourceLocation& location);

    AstBlock(const SourceLocation& location);

    virtual ~AstBlock() = default;

    void PrependChild(const RC<AstStatement>& stmt)
    {
        m_children.PushFront(stmt);
    }

    void AddChild(const RC<AstStatement>& stmt)
    {
        m_children.PushBack(stmt);
    }

    Array<RC<AstStatement>>& GetChildren()
    {
        return m_children;
    }

    const Array<RC<AstStatement>>& GetChildren() const
    {
        return m_children;
    }

    int NumLocals() const
    {
        return m_numLocals;
    }

    bool IsLastStatementReturn() const
    {
        return m_lastIsReturn;
    }

    Scope* GetScope() const
    {
        return m_scope;
    }

    ScopeType GetScopeType() const
    {
        return m_scopeType;
    }

    void SetScopeType(ScopeType scopeType)
    {
        m_scopeType = scopeType;
    }

    int ScopeFlags() const
    {
        return m_scopeFlags;
    }

    void SetScopeFlags(int scopeFlags)
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
    bool m_lastIsReturn;
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

} // namespace hyperion::compiler
