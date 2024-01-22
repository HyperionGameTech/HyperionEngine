#include <script/compiler/IdentifierTable.hpp>
#include <script/compiler/Configuration.hpp>
#include <script/compiler/ast/AstTypeObject.hpp>
#include <script/compiler/ast/AstTypeRef.hpp>

#include <system/Debug.hpp>

#include <unordered_set>
#include <algorithm>

namespace hyperion::compiler {

IdentifierTable::IdentifierTable()
    : m_identifier_index(0)
{
}

IdentifierTable::IdentifierTable(const IdentifierTable &other)
    : m_identifier_index(other.m_identifier_index),
      m_identifiers(other.m_identifiers)
{
}

int IdentifierTable::CountUsedVariables() const
{
    std::unordered_set<int> used_variables;
    
    for (auto &ident : m_identifiers) {
        if (!Config::cull_unused_objects || ident->GetUseCount() > 0) {
            if (used_variables.find(ident->GetIndex()) == used_variables.end()) {
                used_variables.insert(ident->GetIndex());
            }
        }
    }
    
    return used_variables.size();
}

RC<Identifier> IdentifierTable::AddAlias(const String &name, Identifier *aliasee)
{
    AssertThrow(aliasee != nullptr);

    m_identifiers.PushBack(RC<Identifier>(new Identifier(
        name,
        aliasee->GetIndex(),
        aliasee->GetFlags() | FLAG_ALIAS,
        aliasee
    )));
    
    return m_identifiers.Back();
}

RC<Identifier> IdentifierTable::AddIdentifier(
    const String &name,
    int flags,
    RC<AstExpression> current_value,
    SymbolTypePtr_t symbol_type
)
{
    RC<Identifier> ident(new Identifier(
        name,
        m_identifier_index++,
        flags
    ));

    if (current_value != nullptr) {
        ident->SetCurrentValue(current_value);

        if (symbol_type == nullptr) {
            ident->SetSymbolType(symbol_type);
        }
    }

    if (symbol_type != nullptr) {
        ident->SetSymbolType(symbol_type);
    }

    m_identifiers.PushBack(ident);

    return m_identifiers.Back();
}

bool IdentifierTable::AddIdentifier(const RC<Identifier> &identifier)
{
    if (identifier == nullptr) {
        return false;
    }

    if (auto already_existing_identifier = LookUpIdentifier(identifier->GetName())) {
        return false;
    }

    m_identifiers.PushBack(identifier);

    return true;
}

RC<Identifier> IdentifierTable::LookUpIdentifier(const String &name)
{
    for (auto &ident : m_identifiers) {
        if (ident != nullptr) {
            if (ident->GetName() == name) {
                return ident;
            }
        }
    }

    return nullptr;
}

void IdentifierTable::BindTypeToIdentifier(const String &name, SymbolTypePtr_t symbol_type)
{
    AddIdentifier(
        name,
        0,
        RC<AstTypeRef>(new AstTypeRef(symbol_type, SourceLocation::eof)),
        symbol_type->GetBaseType()
    );
}

SymbolTypePtr_t IdentifierTable::LookupSymbolType(const String &name) const
{
    for (auto &type : m_symbol_types) {
        if (type != nullptr && type->GetName() == name) {
            return type;
        }
    }
    
    return nullptr;
}

void IdentifierTable::AddSymbolType(const SymbolTypePtr_t &type)
{
    m_symbol_types.PushBack(type);
}

} // namespace hyperion::compiler
