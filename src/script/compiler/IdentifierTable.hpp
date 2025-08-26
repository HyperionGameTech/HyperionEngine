#pragma once

#include <script/compiler/Identifier.hpp>
#include <script/compiler/type-system/SymbolType.hpp>
#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <core/containers/String.hpp>

#include <string>
#include <memory>
#include <vector>

namespace hyperion::compiler {

class IdentifierTable
{
public:
    IdentifierTable();
    IdentifierTable(const IdentifierTable& other);

    int CountUsedVariables() const;

    const Array<RC<Identifier>>& GetIdentifiers() const
    {
        return m_identifiers;
    }

    /** Constructs an identifier with the given name, as an alias to the given identifier. */
    RC<Identifier> AddAlias(const String& name, Identifier* aliasee);

    /** Constructs an identifier with the given name, and assigns an index to it. */
    RC<Identifier> AddIdentifier(
        const String& name,
        int flags = 0,
        RC<AstExpression> currentValue = nullptr,
        SymbolTypeRef symbolType = nullptr);

    bool AddIdentifier(const RC<Identifier>& identifier);

    /** Look up an identifier by name. Returns nullptr if not found */
    RC<Identifier> LookUpIdentifier(const String& name);

    /** Look up symbol type by name */
    SymbolTypeRef LookupSymbolType(const String& name) const;

    void AddSymbolType(const SymbolTypeRef& type);

private:
    /** To be incremented every time a new identifier is added */
    int m_identifierIndex;
    /** List of all identifiers in the table */
    Array<RC<Identifier>> m_identifiers;

    /** All types that are defined in this identifier table */
    Array<SymbolTypeRef> m_symbolTypes;
};

} // namespace hyperion::compiler
