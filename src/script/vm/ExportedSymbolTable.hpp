#pragma once

#include <script/vm/Value.hpp>

#include <core/HashCode.hpp>

#include <core/containers/HashMap.hpp>
#include <core/debug/Debug.hpp>

#include <utility>

namespace hyperion {

class ExportedSymbolTable
{
    using SymbolMap = HashMap<HashCode::ValueType, Value*>;

public:
    ExportedSymbolTable();
    ExportedSymbolTable(const ExportedSymbolTable& other) = delete;
    ExportedSymbolTable& operator=(const ExportedSymbolTable& other) = delete;
    ExportedSymbolTable(ExportedSymbolTable&& other) noexcept = delete;
    ExportedSymbolTable& operator=(ExportedSymbolTable&& other) noexcept = delete;
    ~ExportedSymbolTable();

    void MarkAll();

    bool Find(const char* name, Value*& out);
    bool Find(HashCode::ValueType hash, Value*& out);
    typename SymbolMap::InsertResult Store(const char* name, Value&& value);
    typename SymbolMap::InsertResult Store(HashCode::ValueType hash, Value&& value);

private:
    SymbolMap m_symbols;
};

} // namespace hyperion
