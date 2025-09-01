#pragma once

#include <script/compiler/Identifier.hpp>
#include <script/compiler/type-system/SymbolType.hpp>
#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <core/containers/String.hpp>

namespace hyperion::compiler {

class Scope;

class IdentifierTable
{
public:
    explicit IdentifierTable(Scope* scope);
    IdentifierTable(const IdentifierTable& other) = delete;
    IdentifierTable& operator=(const IdentifierTable& other) = delete;

    int CountUsedVariables() const;

    /** Constructs an identifier with the given name, as an alias to the given identifier. */
    const RC<Identifier>& AddAlias(const String& name, Identifier* aliasee);

    /** Constructs an identifier with the given name, and assigns an index to it. */
    const RC<Identifier>& AddIdentifier(
        const String& name,
        int flags = 0,
        RC<AstExpression> currentValue = nullptr,
        SymbolTypeRef symbolType = nullptr);

    bool AddIdentifier(const RC<Identifier>& identifier);

    /** Look up an identifier by name. Returns nullptr if not found */
    RC<Identifier> LookUpIdentifier(const String& name);

    /** Look up symbol type by name */
    SymbolTypeRef LookupSymbolType(const String& name, bool includePlaceholderTypes = true) const;

    void AddSymbolType(const SymbolTypeRef& type);

    Scope* scope;
    /** To be incremented every time a new identifier is added */
    int identifierIndex;
    /** List of all identifiers in the table */
    Array<RC<Identifier>> identifiers;

    /** All types that are defined in this identifier table */
    Array<SymbolTypeRef> symbolTypes;
};

} // namespace hyperion::compiler
