#ifndef AST_STATEMENT_HPP
#define AST_STATEMENT_HPP

#include <core/lib/RefCountedPtr.hpp>
#include <core/lib/String.hpp>

#include <script/SourceLocation.hpp>
#include <script/compiler/emit/Buildable.hpp>

#include <memory>
#include <vector>
#include <sstream>

namespace hyperion::compiler {

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
    AstStatement(const SourceLocation &location);
    virtual ~AstStatement() = default;

    SourceLocation &GetLocation() { return m_location; }
    const SourceLocation &GetLocation() const { return m_location; }

    UInt GetScopeDepth() const { return m_scope_depth; }
    void SetScopeDepth(UInt depth) { m_scope_depth = depth; }

    virtual void Visit(AstVisitor *visitor, Module *mod) = 0;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) = 0;
    virtual void Optimize(AstVisitor *visitor, Module *mod) = 0;

    virtual HashCode GetHashCode() const = 0;

    virtual const String &GetName() const { return unnamed; }
    
    virtual RC<AstStatement> Clone() const = 0;

protected:
    SourceLocation  m_location;
    UInt            m_scope_depth;
};

template <typename T>
typename std::enable_if<std::is_base_of<AstStatement, T>::value, RC<T>>::type
CloneAstNode(const RC<T> &stmt) 
{ 
    return (stmt != nullptr) 
        ? stmt->Clone().template CastUnsafe<T>()
        : nullptr; 
}

template <typename T>
typename std::enable_if<std::is_base_of<AstStatement, T>::value, RC<T>>::type
CloneAstNode(const T *stmt) 
{ 
    return (stmt != nullptr) 
        ? stmt->Clone().template CastUnsafe<T>()
        : nullptr; 
}

template <typename T>
typename std::enable_if<std::is_base_of<AstStatement, T>::value, Array<RC<T>>>::type
CloneAllAstNodes(const Array<RC<T>> &stmts) 
{
    Array<RC<T>> res;
    res.Reserve(stmts.Size());
    for (auto &stmt : stmts) {
        res.PushBack(CloneAstNode(stmt));
    }
    return res;
}

template <typename T>
typename std::enable_if<std::is_base_of<AstStatement, T>::value, Array<RC<T>>>::type
CloneAllAstNodes(const Array<T *> &stmts) 
{
    Array<RC<T>> res;
    res.Reserve(stmts.Size());
    for (auto &stmt : stmts) {
        res.PushBack(CloneAstNode(stmt));
    }
    return res;
}

} // namespace hyperion::compiler

#endif
