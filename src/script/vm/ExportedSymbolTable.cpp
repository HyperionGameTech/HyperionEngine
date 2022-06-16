#include <script/vm/ExportedSymbolTable.hpp>

namespace hyperion {
namespace vm {

ExportedSymbolTable::ExportedSymbolTable() = default;
ExportedSymbolTable::~ExportedSymbolTable() = default;

void ExportedSymbolTable::MarkAll()
{
    for (auto &it : m_symbols) {
        it.second.Mark();
    }
}

bool ExportedSymbolTable::Find(const char *name, Value *out)
{
    return Find(hash_fnv_1(name), out);
}

bool ExportedSymbolTable::Find(HashFnv1 hash, Value *out)
{
    const auto it = m_symbols.Find(hash);

    if (it == m_symbols.End()) {
        return false;
    }

    *out = it->second;

    return true;
}

auto ExportedSymbolTable::Store(const char *name, const Value &value) -> typename SymbolMap::InsertResult
{
    return Store(hash_fnv_1(name), value);
}

auto ExportedSymbolTable::Store(HashFnv1 hash, const Value &value) -> typename SymbolMap::InsertResult
{
    return m_symbols.Insert(hash, value);
}


} // namespace vm
} // namespace hyperion
