#include <script/compiler/IdentifierTable.hpp>
#include <script/compiler/Configuration.hpp>
#include <script/compiler/ast/AstTypeObject.hpp>

#include <system/debug.h>

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
        if (!hyperion::compiler::Config::cull_unused_objects || ident->GetUseCount() > 0) {
            if (used_variables.find(ident->GetIndex()) == used_variables.end()) {
                used_variables.insert(ident->GetIndex());
            }
        }
    }
    
    return used_variables.size();
}

Identifier *IdentifierTable::AddAlias(const std::string &name, Identifier *aliasee)
{
    AssertThrow(aliasee != nullptr);

    m_identifiers.push_back(std::shared_ptr<Identifier>(new Identifier(
        name,
        aliasee->GetIndex(),
        aliasee->GetFlags() | FLAG_ALIAS,
        aliasee
    )));
    
    return m_identifiers.back().get();
}

Identifier *IdentifierTable::AddIdentifier(const std::string &name,
    int flags,
    std::shared_ptr<AstExpression> current_value,
    SymbolTypePtr_t symbol_type)
{
    std::shared_ptr<Identifier> ident(new Identifier(
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

    m_identifiers.push_back(ident);
    return m_identifiers.back().get();
}

Identifier *IdentifierTable::LookUpIdentifier(const std::string &name)
{
    for (auto &ident : m_identifiers) {
        if (ident != nullptr) {
            if (ident->GetName() == name) {
                return ident.get()->Unalias();
            }
        }
    }

    return nullptr;
}

void IdentifierTable::BindTypeToIdentifier(const std::string &name, SymbolTypePtr_t symbol_type)
{
    AddIdentifier(name, 0, std::shared_ptr<AstTypeObject>(new AstTypeObject(
        symbol_type, nullptr, SourceLocation::eof
    )), symbol_type->GetBaseType());
}

SymbolTypePtr_t IdentifierTable::LookupSymbolType(const std::string &name) const
{
    for (auto &type : m_symbol_types) {
        if (type != nullptr && type->GetName() == name) {
            return type;
        }
    }
    
    return nullptr;
}

SymbolTypePtr_t IdentifierTable::LookupGenericInstance(
    const SymbolTypePtr_t &base,
    const std::vector<GenericInstanceTypeInfo::Arg> &params) const
{
    AssertThrow(base != nullptr);
    AssertThrow(base->GetTypeClass() == TYPE_GENERIC);

    for (auto &type : m_symbol_types) {
        if (type != nullptr) {
            if (type->GetTypeClass() == TYPE_GENERIC_INSTANCE && type->GetBaseType() == base) {

                // check params
                const auto &other_params = type->GetGenericInstanceInfo().m_generic_args;

                if (other_params.size() != params.size()) {
                    continue;
                }

                bool found = true;

                for (size_t i = 0; i < params.size(); i++) {
                    const SymbolTypePtr_t &param_type = params[i].m_type;
                    const SymbolTypePtr_t &arg_type = type->GetGenericInstanceInfo().m_generic_args[i].m_type;

                    AssertThrow(param_type != nullptr);
                    AssertThrow(arg_type != nullptr);

                    if (!param_type->TypeEqual(*arg_type)) {
                        found = false;
                        break;
                    }
                }

                if (found) {
                    return type;
                }
            }
        }
    }

    return nullptr;
}

void IdentifierTable::AddSymbolType(const SymbolTypePtr_t &type)
{
    m_symbol_types.push_back(type);
}

} // namespace hyperion::compiler
