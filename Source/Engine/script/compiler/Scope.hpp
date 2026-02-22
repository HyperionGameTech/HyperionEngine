#pragma once

#include <script/compiler/IdentifierTable.hpp>
#include <script/compiler/type-system/SymbolType.hpp>
#include <script/SourceLocation.hpp>

#include <Core/containers/HashMap.hpp>
#include <Core/memory/RefCountedPtr.hpp>
#include <Core/utilities/Optional.hpp>

#include <Core/HashCode.hpp>

#include <utility>

namespace Hyperion {

/*! \brief A cache for generic instances.
    \details
    Generic instances are cached so that they can be reused when the same
    generic type is used with the same type arguments.
*/
class TypeInstanceCache
{
    static constexpr SizeType s_maxCacheSize = 65536;

public:
    class Key
    {
        friend class TypeInstanceCache;

        Key()
            : m_originalType(nullptr)
        {
        }

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
            hash.Add(UIntPtr(m_originalType));
            hash.Add(m_genericArgsHashCode);

            return hash;
        }

    private:
        const SymbolType* m_originalType;
        HashCode m_genericArgsHashCode;
    };

    ~TypeInstanceCache()
    {
        for (auto& it : m_cache)
        {
            it.second->SetIsCached(false);
        }
    }

    static Key MakeKey(const SymbolType* originalType, const GenericInstanceTypeInfo& genericInstanceTypeInfo)
    {
        Assert(originalType != nullptr);

        Key key;
        key.m_originalType = originalType;
        key.m_genericArgsHashCode = genericInstanceTypeInfo.GetHashCode();

        return key;
    }

    SymbolType* Lookup(const Key& key) const
    {
        Mutex::Guard guard(m_mutex);

        auto it = m_cache.Find(key);

        if (it != m_cache.End())
        {
            return it->second;
        }

        return nullptr;
    }

    void Put(const Key& key, SymbolType* type)
    {
        Assert(type != nullptr);

        Mutex::Guard guard(m_mutex);

        type->SetIsCached(true);

        m_cache.Set(key, type);

        Assert(m_cache.Size() <= s_maxCacheSize,
            "TypeInstanceCache exceeded max size! May indicate a logic error inserting infinite entries.");
    }

private:
    using CacheMap = HashMap<Key, SymbolType*, DynamicNodeAllocator>;

    mutable Mutex m_mutex;
    CacheMap m_cache;
};

enum ScopeType
{
    SCOPE_TYPE_NORMAL,
    SCOPE_TYPE_MODULE,
    SCOPE_TYPE_FUNCTION,
    SCOPE_TYPE_CLASS_DEFINITION,
    SCOPE_TYPE_LOOP,
    SCOPE_TYPE_ALIAS_DECLARATION,
    SCOPE_TYPE_TYPE_SPECIFICATION
};

enum ScopeFlags : int
{
    CLOSURE_FUNCTION_FLAG = 0x2,
    CONSTRUCTOR_DEFINITION_FLAG = 0x10,
    REF_VARIABLE_FLAG = 0x20,
    CONST_VARIABLE_FLAG = 0x40,
    EXTERN_CLASS_FLAG = 0x80
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
    Array<const SymbolType*> returnTypes;
    HashMap<String, RC<Identifier>> closureCaptures;
    TypeInstanceCache typeInstanceCache;
};

} // namespace Hyperion
