#include <script/vm/ExportedSymbolTable.hpp>

namespace hyperion {
namespace vm {

ExportedSymbolTable::ExportedSymbolTable() = default;
ExportedSymbolTable::~ExportedSymbolTable()
{
    // delete all stored values
    for (auto& pair : m_symbols)
    {
        delete pair.second;
    }
}

void ExportedSymbolTable::MarkAll()
{
    // not needed anymore
}

bool ExportedSymbolTable::Find(const char* name, Value*& out)
{
    return Find(HashCode::GetHashCode(name).Value(), out);
}

bool ExportedSymbolTable::Find(HashCode::ValueType hash, Value*& out)
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

auto ExportedSymbolTable::Store(const char* name, Value&& value) -> typename SymbolMap::InsertResult
{
    return Store(HashCode::GetHashCode(name).Value(), std::move(value));
}

auto ExportedSymbolTable::Store(HashCode::ValueType hash, Value&& value) -> typename SymbolMap::InsertResult
{
    return m_symbols.Insert(hash, new Value(std::move(value)));
}

} // namespace vm
} // namespace hyperion
