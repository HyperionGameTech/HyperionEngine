#ifndef SCOPE_HPP
#define SCOPE_HPP

#include <script/compiler/IdentifierTable.hpp>
#include <script/compiler/type-system/SymbolType.hpp>
#include <script/SourceLocation.hpp>
#include <core/lib/HashMap.hpp>

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
    PURE_FUNCTION_FLAG          = 0x1,
    CLOSURE_FUNCTION_FLAG       = 0x2,
    GENERATOR_FUNCTION_FLAG     = 0x4,
    UNINSTANTIATED_GENERIC_FLAG = 0x8,
    CONSTRUCTOR_DEFINITION_FLAG = 0x10,
    REF_VARIABLE_FLAG           = 0x20,
    CONST_VARIABLE_FLAG         = 0x40,
    ENUM_MEMBERS_FLAG           = 0x80
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

    void SetScopeType(ScopeType scope_type)
        { m_scope_type = scope_type; }

    int GetScopeFlags() const
        { return m_scope_flags; }

    void SetScopeFlags(int flags)
        { m_scope_flags = flags; }

    void AddReturnType(const SymbolTypePtr_t &type, const SourceLocation &location) 
        { m_return_types.PushBack({type, location}); }

    const Array<ReturnType_t> &GetReturnTypes() const
        { return m_return_types; }

    RC<Identifier> FindClosureCapture(const String &name) 
    {
        auto it = m_closure_captures.Find(name);

        return it != m_closure_captures.End() ? it->second : nullptr;
    }

    void AddClosureCapture(const String &name, const RC<Identifier> &ident) 
        { m_closure_captures.Insert(name, ident); }

    const HashMap<String, RC<Identifier>> &GetClosureCaptures() const
        { return m_closure_captures; }

private:
    IdentifierTable                 m_identifier_table;
    ScopeType                       m_scope_type;
    Int                             m_scope_flags;
    Array<ReturnType_t>             m_return_types;
    HashMap<String, RC<Identifier>> m_closure_captures;
};

} // namespace hyperion::compiler

#endif
