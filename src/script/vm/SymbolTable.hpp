#pragma once

#include <script/vm/Value.hpp>

#include <core/HashCode.hpp>

#include <core/containers/HashMap.hpp>
#include <core/debug/Debug.hpp>

namespace hyperion {

class Script_SymbolTable
{
    using SymbolMap = HashMap<HashCode::ValueType, HypData*>;

public:
    Script_SymbolTable();
    Script_SymbolTable(const Script_SymbolTable& other) = delete;
    Script_SymbolTable& operator=(const Script_SymbolTable& other) = delete;
    Script_SymbolTable(Script_SymbolTable&& other) noexcept = delete;
    Script_SymbolTable& operator=(Script_SymbolTable&& other) noexcept = delete;
    ~Script_SymbolTable();

    void MarkAll();

    bool Find(const char* name, HypData*& out);
    bool Find(HashCode::ValueType hash, HypData*& out);
    typename SymbolMap::InsertResult Store(const char* name, HypData&& value);
    typename SymbolMap::InsertResult Store(HashCode::ValueType hash, HypData&& value);

private:
    SymbolMap m_symbols;
};

} // namespace hyperion
