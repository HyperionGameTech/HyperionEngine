#ifndef SCOPE_HPP
#define SCOPE_HPP

#include <script/compiler/IdentifierTable.hpp>
#include <script/compiler/type-system/SymbolType.hpp>
#include <script/compiler/SourceLocation.hpp>

#include <vector>
#include <utility>
#include <map>

namespace hyperion {
namespace compiler {

typedef std::pair<SymbolTypePtr_t, SourceLocation> ReturnType_t;

enum ScopeType {
    SCOPE_TYPE_NORMAL,
    SCOPE_TYPE_FUNCTION,
    SCOPE_TYPE_TYPE_DEFINITION,
    SCOPE_TYPE_LOOP
};

enum ScopeFunctionFlags : int {
    PURE_FUNCTION_FLAG      = 0x01,
    CLOSURE_FUNCTION_FLAG   = 0x02,
    GENERATOR_FUNCTION_FLAG = 0x04,

    GENERIC_FLAG            = 0x08
};

class Scope {
    friend class Module;
public:
    Scope();
    Scope(ScopeType scope_type, int scope_flags);
    Scope(const Scope &other);

    inline IdentifierTable &GetIdentifierTable()
        { return m_identifier_table; }
    inline const IdentifierTable &GetIdentifierTable() const
        { return m_identifier_table; }
    inline ScopeType GetScopeType() const
        { return m_scope_type; }
    inline int GetScopeFlags() const
        { return m_scope_flags; }
    inline void AddReturnType(const SymbolTypePtr_t &type, const SourceLocation &location) 
        { m_return_types.push_back({type, location}); }
    inline const std::vector<ReturnType_t> &GetReturnTypes() const
        { return m_return_types; }
    inline Identifier *FindClosureCapture(const std::string &name) 
    {
        auto it = m_closure_captures.find(name);
        return it != m_closure_captures.end() ? it->second : nullptr;
    }
    inline void AddClosureCapture(const std::string &name, Identifier *ident) 
        { m_closure_captures.insert({ name, ident }); }
    inline const std::map<std::string, Identifier*> &GetClosureCaptures() const
        { return m_closure_captures; }

private:
    IdentifierTable m_identifier_table;
    ScopeType m_scope_type;
    int m_scope_flags;
    std::vector<ReturnType_t> m_return_types;
    std::map<std::string, Identifier*> m_closure_captures;
};

} // namespace compiler
} // namespace hyperion

#endif
