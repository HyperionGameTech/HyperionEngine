#ifndef SCOPE_HPP
#define SCOPE_HPP

#include <script/compiler/IdentifierTable.hpp>
#include <script/compiler/type-system/SymbolType.hpp>
#include <script/SourceLocation.hpp>

#include <vector>
#include <utility>
#include <map>

namespace hyperion::compiler {

typedef std::pair<SymbolTypePtr_t, SourceLocation> ReturnType_t;

enum ScopeType
{
    SCOPE_TYPE_NORMAL,
    SCOPE_TYPE_FUNCTION,
    SCOPE_TYPE_TYPE_DEFINITION,
    SCOPE_TYPE_LOOP,
    SCOPE_TYPE_GENERIC_INSTANTIATION,
    SCOPE_TYPE_ALIAS_DECLARATION
};

enum ScopeFunctionFlags : int
{
    PURE_FUNCTION_FLAG          = 0b00000001,
    CLOSURE_FUNCTION_FLAG       = 0b00000010,
    GENERATOR_FUNCTION_FLAG     = 0b00000100,
    UNINSTANTIATED_GENERIC_FLAG = 0b00001000,
    CONSTRUCTOR_DEFINITION_FLAG = 0b00010000
};

class Scope
{
    friend class Module;
public:
    Scope();
    Scope(ScopeType scope_type, int scope_flags);
    Scope(const Scope &other);

    IdentifierTable &GetIdentifierTable()
        { return m_identifier_table; }

    const IdentifierTable &GetIdentifierTable() const
        { return m_identifier_table; }

    ScopeType GetScopeType() const
        { return m_scope_type; }

    int GetScopeFlags() const
        { return m_scope_flags; }

    void AddReturnType(const SymbolTypePtr_t &type, const SourceLocation &location) 
        { m_return_types.push_back({type, location}); }

    const std::vector<ReturnType_t> &GetReturnTypes() const
        { return m_return_types; }

    std::shared_ptr<Identifier> FindClosureCapture(const std::string &name) 
    {
        auto it = m_closure_captures.find(name);

        return it != m_closure_captures.end() ? it->second : nullptr;
    }
    void AddClosureCapture(const std::string &name, const std::shared_ptr<Identifier> &ident) 
        { m_closure_captures.insert({ name, ident }); }

    const std::map<std::string, std::shared_ptr<Identifier>> &GetClosureCaptures() const
        { return m_closure_captures; }

private:
    IdentifierTable m_identifier_table;
    ScopeType m_scope_type;
    int m_scope_flags;
    std::vector<ReturnType_t> m_return_types;
    std::map<std::string, std::shared_ptr<Identifier>> m_closure_captures;
};

} // namespace hyperion::compiler

#endif
