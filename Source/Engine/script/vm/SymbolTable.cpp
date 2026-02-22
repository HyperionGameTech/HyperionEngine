#include <script/vm/SymbolTable.hpp>

#include <Core/reflection/BoxedValue.hpp>

namespace Hyperion {

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

bool Script_SymbolTable::Find(const char* name, BoxedValue*& out)
{
    return Find(HashCode::GetHashCode(name).Value(), out);
}

bool Script_SymbolTable::Find(HashCode::ValueType hash, BoxedValue*& out)
{
    auto it = m_symbols.FindByHashCode(HashCode(hash));

    if (it == m_symbols.End())
    {
        return false;
    }

    Assert(it->second != nullptr);

    out = Deref(*it->second);

    return true;
}

auto Script_SymbolTable::Store(const char* name, BoxedValue&& value) -> typename SymbolMap::InsertResult
{
    return Store(HashCode::GetHashCode(name).Value(), std::move(value));
}

auto Script_SymbolTable::Store(HashCode::ValueType hash, BoxedValue&& value) -> typename SymbolMap::InsertResult
{
    return m_symbols.Insert(hash, new BoxedValue(std::move(value)));
}

} // namespace Hyperion
