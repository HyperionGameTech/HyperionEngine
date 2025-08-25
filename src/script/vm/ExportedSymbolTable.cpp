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
    return Find(hashFnv1(name), out);
}

bool ExportedSymbolTable::Find(HashFNV1 hash, Value*& out)
{
    auto it = m_symbols.Find(hash);

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
    return Store(hashFnv1(name), std::move(value));
}

auto ExportedSymbolTable::Store(HashFNV1 hash, Value&& value) -> typename SymbolMap::InsertResult
{
    return m_symbols.Insert(hash, new Value(std::move(value)));
}

} // namespace vm
} // namespace hyperion
