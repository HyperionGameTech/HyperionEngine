#pragma once

#include <Lang/compiler/Identifier.hpp>
#include <Lang/compiler/type-system/SymbolType.hpp>
#include <Lang/compiler/type-system/BuiltinTypes.hpp>

#include <Core/containers/String.hpp>

namespace Hyperion {

class Scope;

class IdentifierTable
{
public:
    explicit IdentifierTable(Scope* scope)
        : identifierIndex(0),
          scope(scope)
    {
    }

    IdentifierTable(const IdentifierTable& other) = delete;
    IdentifierTable& operator=(const IdentifierTable& other) = delete;

    int CountUsedVariables() const;

    /** Constructs an identifier with the given name, as an alias to the given identifier. */
    const RC<Identifier>& AddAlias(const String& name, Identifier* aliasee);

    /** Constructs an identifier with the given name, and assigns an index to it. */
    const RC<Identifier>& AddIdentifier(
        const String& name,
        int flags = 0,
        const RC<AstExpression>& currentValue = nullptr,
        const SymbolType* symbolType = nullptr);

    bool AddIdentifier(const RC<Identifier>& identifier);

    /** Look up an identifier by name. Returns nullptr if not found */
    RC<Identifier> LookUpIdentifier(const String& name);

    /** Look up symbol type by name */
    const SymbolType* LookupSymbolType(const String& name, bool includePlaceholderTypes = true) const;

    void AddSymbolType(SymbolType* symbolType);

    Scope* scope;
    /** To be incremented every time a new identifier is added */
    int identifierIndex;
    /** List of all identifiers in the table */
    Array<RC<Identifier>> identifiers;

    /** All types that are defined in this identifier table */
    Array<const SymbolType*> symbolTypes;
};

} // namespace Hyperion
