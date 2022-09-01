#ifndef IDENTIFIER_TABLE_HPP
#define IDENTIFIER_TABLE_HPP

#include <script/compiler/Identifier.hpp>
#include <script/compiler/type-system/SymbolType.hpp>
#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <string>
#include <memory>
#include <vector>

namespace hyperion::compiler {

class IdentifierTable {
public:
    IdentifierTable();
    IdentifierTable(const IdentifierTable &other);

    int CountUsedVariables() const;
    
    void PopIdentifier() { m_identifiers.pop_back(); m_identifier_index--; }
    int GetIdentifierIndex() const { return m_identifier_index; }
    std::vector<std::shared_ptr<Identifier>> &GetIdentifiers()
        { return m_identifiers; }
    const std::vector<std::shared_ptr<Identifier>> &GetIdentifiers() const
        { return m_identifiers; }

    /** Constructs an identifier with the given name, as an alias to the given identifier. */
    Identifier *AddAlias(const std::string &name, Identifier *aliasee);

    /** Constructs an identifier with the given name, and assigns an index to it. */
    Identifier *AddIdentifier(const std::string &name,
        int flags = 0,
        std::shared_ptr<AstExpression> current_value = nullptr,
        SymbolTypePtr_t symbol_type = nullptr);

    /** Look up an identifier by name. Returns nullptr if not found */
    Identifier *LookUpIdentifier(const std::string &name);

    void BindTypeToIdentifier(const std::string &name, SymbolTypePtr_t symbol_type);

    /** Look up symbol type by name */
    SymbolTypePtr_t LookupSymbolType(const std::string &name) const;
    /** Look up an instance of a generic type, with the given parameters*/
    SymbolTypePtr_t LookupGenericInstance(const SymbolTypePtr_t &base, 
        const std::vector<GenericInstanceTypeInfo::Arg> &params) const;
        
    void AddSymbolType(const SymbolTypePtr_t &type);

private:
    /** To be incremented every time a new identifier is added */
    int m_identifier_index;
    /** List of all identifiers in the table */
    std::vector<std::shared_ptr<Identifier>> m_identifiers;

    /** All types that are defined in this identifier table */
    std::vector<std::shared_ptr<SymbolType>> m_symbol_types;
};

} // namespace hyperion::compiler

#endif
