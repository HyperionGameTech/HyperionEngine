#pragma once

#include <script/vm/Value.hpp>

#include <Core/HashCode.hpp>

#include <Core/containers/HashMap.hpp>
#include <Core/debug/Debug.hpp>

namespace Hyperion {

class Script_SymbolTable
{
    using SymbolMap = HashMap<HashCode::ValueType, BoxedValue*>;

public:
    Script_SymbolTable();
    Script_SymbolTable(const Script_SymbolTable& other) = delete;
    Script_SymbolTable& operator=(const Script_SymbolTable& other) = delete;
    Script_SymbolTable(Script_SymbolTable&& other) noexcept = delete;
    Script_SymbolTable& operator=(Script_SymbolTable&& other) noexcept = delete;
    ~Script_SymbolTable();

    void MarkAll();

    bool Find(const char* name, BoxedValue*& out);
    bool Find(HashCode::ValueType hash, BoxedValue*& out);
    typename SymbolMap::InsertResult Store(const char* name, BoxedValue&& value);
    typename SymbolMap::InsertResult Store(HashCode::ValueType hash, BoxedValue&& value);

private:
    SymbolMap m_symbols;
};

} // namespace Hyperion
