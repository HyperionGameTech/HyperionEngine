#include <script/vm/SymbolTable.hpp>

namespace hyperion {

Script_SymbolTable::Script_SymbolTable() = default;
Script_SymbolTable::~Script_SymbolTable()
{
    // delete all stored values
    for (auto& pair : m_symbols)
    {
        delete pair.second;
    }
}

void Script_SymbolTable::MarkAll()
{
    // not needed anymore
}

bool Script_SymbolTable::Find(const char* name, Script_Value*& out)
{
    return Find(HashCode::GetHashCode(name).Value(), out);
}

bool Script_SymbolTable::Find(HashCode::ValueType hash, Script_Value*& out)
{
    auto it = m_symbols.FindByHashCode(HashCode(hash));

    if (it == m_symbols.End())
    {
        return false;
    }

    Assert(it->second != nullptr);

    out = it->second->Deref();

    return true;
}

auto Script_SymbolTable::Store(const char* name, Script_Value&& value) -> typename SymbolMap::InsertResult
{
    return Store(HashCode::GetHashCode(name).Value(), std::move(value));
}

auto Script_SymbolTable::Store(HashCode::ValueType hash, Script_Value&& value) -> typename SymbolMap::InsertResult
{
    return m_symbols.Insert(hash, new Script_Value(std::move(value)));
}

} // namespace hyperion
