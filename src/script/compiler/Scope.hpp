#pragma once

#include <script/compiler/IdentifierTable.hpp>
#include <script/compiler/type-system/SymbolType.hpp>
#include <script/SourceLocation.hpp>

#include <core/containers/HashMap.hpp>
#include <core/memory/RefCountedPtr.hpp>
#include <core/utilities/Optional.hpp>

#include <core/HashCode.hpp>

#include <utility>

namespace hyperion {

/*! \brief A cache for generic instances.
    \details
    Generic instances are cached so that they can be reused when the same
    generic type is used with the same type arguments.
*/
class GenericInstanceCache
{

public:
    class Key
    {
        friend class GenericInstanceCache;

        Key() = default;

    public:
        Key(const Key& other) = default;
        Key& operator=(const Key& other) = default;
        Key(Key&& other) noexcept = default;
        Key& operator=(Key&& other) noexcept = default;
        ~Key() = default;

        bool operator==(const Key& other) const
        {
            return GetHashCode() == other.GetHashCode();
        }

        bool operator!=(const Key& other) const
        {
            return !(*this == other);
        }

        bool operator<(const Key& other) const
        {
            return GetHashCode() < other.GetHashCode();
        }

        HashCode GetHashCode() const
        {
            HashCode hash;
            hash.Add(uintptr_t(m_originalType));
            hash.Add(m_genericArgsHashCode);

            return hash;
        }

    private:
        SymbolType* m_originalType;
        HashCode m_genericArgsHashCode;
    };

    static Key MakeKey(const SymbolTypeRef& originalType, const GenericInstanceTypeInfo& genericInstanceTypeInfo)
    {
        Key key;
        key.m_originalType = originalType.Get();
        key.m_genericArgsHashCode = genericInstanceTypeInfo.GetHashCode();

        return key;
    }

    SymbolTypeRef Lookup(const Key& key) const
    {
        Mutex::Guard guard(m_mutex);

        auto it = m_cache.Find(key);

        if (it != m_cache.End())
        {
            SymbolTypeRef type = it->second.Lock();

            if (type.IsValid())
            {
                return type;
            }
        }

        return SymbolTypeRef();
    }

    void Put(const Key& key, const SymbolTypeRef& type)
    {
        Mutex::Guard guard(m_mutex);

        m_cache.Set(key, type.ToWeak());
    }

private:
    using CacheMap = HashMap<Key, SymbolTypeWeakRef, HashTable_DynamicNodeAllocator<KeyValuePair<Key, SymbolTypeWeakRef>>>;

    mutable Mutex m_mutex;
    CacheMap m_cache;
};

enum ScopeType
{
    SCOPE_TYPE_NORMAL,
    SCOPE_TYPE_FUNCTION,
    SCOPE_TYPE_TYPE_DEFINITION,
    SCOPE_TYPE_LOOP,
    SCOPE_TYPE_ALIAS_DECLARATION
};

enum ScopeFlags : int
{
    CLOSURE_FUNCTION_FLAG = 0x2,
    GENERATOR_FUNCTION_FLAG = 0x4,
    CONSTRUCTOR_DEFINITION_FLAG = 0x10,
    REF_VARIABLE_FLAG = 0x20,
    CONST_VARIABLE_FLAG = 0x40,
    ENUM_MEMBERS_FLAG = 0x80
};

class Scope
{
    friend class Module;

public:
    Scope();
    Scope(ScopeType scopeType, int scopeFlags = 0);
    Scope(const Scope& other) = delete;
    Scope& operator=(const Scope& other) = delete;

    Identifier* FindClosureCapture(const String& name)
    {
        auto it = closureCaptures.Find(name);

        return it != closureCaptures.End() ? it->second : nullptr;
    }

    void AddClosureCapture(const String& name, const RC<Identifier>& ident)
    {
        closureCaptures.Insert(name, ident);
    }

    IdentifierTable identifierTable;
    ScopeType scopeType;
    int scopeFlags;
    Array<SymbolTypeRef> returnTypes;
    HashMap<String, RC<Identifier>> closureCaptures;
    GenericInstanceCache genericInstanceCache;
};

} // namespace hyperion
