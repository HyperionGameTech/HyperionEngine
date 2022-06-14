#ifndef AST_STATEMENT_HPP
#define AST_STATEMENT_HPP

#include <script/SourceLocation.hpp>
#include <script/compiler/emit/Buildable.hpp>

#include <memory>
#include <vector>
#include <sstream>

namespace hyperion::compiler {

template <typename T> using Pointer = std::shared_ptr<T>;

// Forward declarations
class AstVisitor;
class Module;
class SymbolType;

typedef std::shared_ptr<SymbolType> SymbolTypePtr_t;

class AstStatement {
    friend class AstIterator;
public:
    AstStatement(const SourceLocation &location);
    virtual ~AstStatement() = default;

    inline SourceLocation &GetLocation() { return m_location; }
    inline const SourceLocation &GetLocation() const { return m_location; }

    virtual void Visit(AstVisitor *visitor, Module *mod) = 0;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) = 0;
    virtual void Optimize(AstVisitor *visitor, Module *mod) = 0;
    
    virtual Pointer<AstStatement> Clone() const = 0;

protected:
    SourceLocation m_location;
};

template <typename T>
typename std::enable_if<std::is_base_of<AstStatement, T>::value, std::shared_ptr<T>>::type
CloneAstNode(const std::shared_ptr<T> &stmt) 
{ 
    return (stmt != nullptr) 
        ? std::static_pointer_cast<T>(stmt->Clone()) 
        : nullptr; 
}

template <typename T>
typename std::enable_if<std::is_base_of<AstStatement, T>::value, std::shared_ptr<T>>::type
CloneAstNode(const T *stmt) 
{ 
    return (stmt != nullptr) 
        ? std::static_pointer_cast<T>(stmt->Clone()) 
        : nullptr; 
}

template <typename T>
typename std::enable_if<std::is_base_of<AstStatement, T>::value, std::vector<std::shared_ptr<T>>>::type
CloneAllAstNodes(const std::vector<std::shared_ptr<T>> &stmts) 
{
    std::vector<std::shared_ptr<T>> res;
    res.reserve(stmts.size());
    for (auto &stmt : stmts) {
        res.push_back(CloneAstNode(stmt));
    }
    return res;
}

template <typename T>
typename std::enable_if<std::is_base_of<AstStatement, T>::value, std::vector<std::shared_ptr<T>>>::type
CloneAllAstNodes(const std::vector<T *> &stmts) 
{
    std::vector<std::shared_ptr<T>> res;
    res.reserve(stmts.size());
    for (auto &stmt : stmts) {
        res.push_back(CloneAstNode(stmt));
    }
    return res;
}

} // namespace hyperion::compiler

#endif
