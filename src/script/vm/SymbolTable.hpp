#pragma once

#include <script/vm/Value.hpp>

#include <core/HashCode.hpp>

#include <core/containers/HashMap.hpp>
#include <core/debug/Debug.hpp>

namespace hyperion {

class Script_SymbolTable
{
    using SymbolMap = HashMap<HashCode::ValueType, Script_Value*>;

public:
    Script_SymbolTable();
    Script_SymbolTable(const Script_SymbolTable& other) = delete;
    Script_SymbolTable& operator=(const Script_SymbolTable& other) = delete;
    Script_SymbolTable(Script_SymbolTable&& other) noexcept = delete;
    Script_SymbolTable& operator=(Script_SymbolTable&& other) noexcept = delete;
    ~Script_SymbolTable();

    void MarkAll();

    bool Find(const char* name, Script_Value*& out);
    bool Find(HashCode::ValueType hash, Script_Value*& out);
    typename SymbolMap::InsertResult Store(const char* name, Script_Value&& value);
    typename SymbolMap::InsertResult Store(HashCode::ValueType hash, Script_Value&& value);

private:
    SymbolMap m_symbols;
};

} // namespace hyperion
