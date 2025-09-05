#pragma once

#include <core/memory/RefCountedPtr.hpp>
#include <core/memory/UniquePtr.hpp>

#include <core/containers/String.hpp>

#include <script/SourceLocation.hpp>
#include <script/compiler/emit/Buildable.hpp>

#include <vector>
#include <sstream>

namespace hyperion {

// Forward declarations
class AstVisitor;
class Module;
class SymbolType;

class AstStatement
{
    friend class AstIterator;

protected:
    static const String unnamed;

public:
    AstStatement(const SourceLocation& location);
    virtual ~AstStatement() = default;

    SourceLocation& GetLocation()
    {
        return m_location;
    }
    const SourceLocation& GetLocation() const
    {
        return m_location;
    }

    uint32 GetScopeDepth() const
    {
        return m_scopeDepth;
    }
    void SetScopeDepth(uint32 depth)
    {
        m_scopeDepth = depth;
    }

    virtual void Visit(AstVisitor* visitor, Module* mod) = 0;
    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) = 0;
    virtual void Optimize(AstVisitor* visitor, Module* mod) = 0;

    virtual HashCode GetHashCode() const = 0;

    virtual const String& GetName() const
    {
        return unnamed;
    }

    // For bytecode debugging - converts the AST node to a readable string representation
    virtual String ToString() const
    {
        return GetName();
    }

    virtual RC<AstStatement> Clone() const = 0;

protected:
    SourceLocation m_location;
    uint32 m_scopeDepth;
};

template <typename T>
typename std::enable_if<std::is_base_of_v<AstStatement, T>, RC<T>>::type
CloneAstNode(const RC<T>& stmt)
{
    return (stmt != nullptr)
        ? stmt->Clone().template CastUnsafe<T>()
        : nullptr;
}

template <typename T>
typename std::enable_if<std::is_base_of_v<AstStatement, T>, RC<T>>::type
CloneAstNode(const T* stmt)
{
    return (stmt != nullptr)
        ? stmt->Clone().template CastUnsafe<T>()
        : nullptr;
}

template <typename T>
typename std::enable_if<std::is_base_of_v<AstStatement, T>, Array<RC<T>>>::type
CloneAllAstNodes(const Array<RC<T>>& stmts)
{
    Array<RC<T>> res;
    res.Reserve(stmts.Size());
    for (auto& stmt : stmts)
    {
        res.PushBack(CloneAstNode(stmt));
    }
    return res;
}

template <typename T>
typename std::enable_if<std::is_base_of_v<AstStatement, T>, Array<RC<T>>>::type
CloneAllAstNodes(const Array<T*>& stmts)
{
    Array<RC<T>> res;
    res.Reserve(stmts.Size());
    for (auto& stmt : stmts)
    {
        res.PushBack(CloneAstNode(stmt));
    }
    return res;
}

} // namespace hyperion
