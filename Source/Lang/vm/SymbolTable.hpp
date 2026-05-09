#pragma once

#include <Lang/vm/Value.hpp>

#include <Core/HashCode.hpp>

#include <Core/containers/Map.hpp>
#include <Core/debug/Debug.hpp>

namespace Hyperion {

class SymbolTable
{
    using SymbolMap = TMap<HashCode::ValueType, BoxedValue*>;

public:
    SymbolTable();
    SymbolTable(const SymbolTable& other) = delete;
    SymbolTable& operator=(const SymbolTable& other) = delete;
    SymbolTable(SymbolTable&& other) noexcept = delete;
    SymbolTable& operator=(SymbolTable&& other) noexcept = delete;
    ~SymbolTable();

    void MarkAll();

    bool Find(const char* name, BoxedValue*& out);
    bool Find(HashCode::ValueType hash, BoxedValue*& out);
    typename SymbolMap::InsertResult Store(const char* name, BoxedValue&& value);
    typename SymbolMap::InsertResult Store(HashCode::ValueType hash, BoxedValue&& value);

private:
    SymbolMap m_symbols;
};

} // namespace Hyperion
