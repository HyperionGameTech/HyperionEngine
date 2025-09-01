#pragma once

#include <script/compiler/IdentifierTable.hpp>
#include <script/compiler/type-system/SymbolType.hpp>
#include <script/SourceLocation.hpp>

#include <core/containers/HashMap.hpp>
#include <core/memory/RefCountedPtr.hpp>
#include <core/utilities/Optional.hpp>

#include <core/HashCode.hpp>

#include <utility>

namespace hyperion::compiler {

/*! \brief A cache for generic instances.
    \details
    Generic instances are cached so that they can be reused when the same
    generic type is used with the same type arguments.
*/
class GenericInstanceCache
{
public:
    struct CachedObject
    {
        uint32 id = 0;
        RC<AstExpression> instantiatedExpr;

        HYP_FORCE_INLINE bool operator==(const CachedObject& other) const
        {
            return id == other.id;
        }

        HYP_FORCE_INLINE bool operator!=(const CachedObject& other) const
        {
            return !(*this == other);
        }
    };

    struct Key
    {
        RC<AstExpression> genericExpr; // the original generic expression
        Array<HashCode> argHashCodes;  // hashcodes of AstArgument nodes

        Key() = default;

        Key(
            RC<AstExpression> genericExpr,
            Array<HashCode> argHashCodes)
            : genericExpr(std::move(genericExpr)),
              argHashCodes(std::move(argHashCodes))
        {
            Assert(this->genericExpr != nullptr);
        }

        Key(const Key& other) = default;
        Key& operator=(const Key& other) = default;
        Key(Key&& other) noexcept = default;
        Key& operator=(Key&& other) noexcept = default;
        ~Key() = default;

        bool IsValid() const
        {
            return genericExpr != nullptr;
        }

        bool operator==(const Key& other) const
        {
            if (!IsValid() || !other.IsValid())
            {
                return false;
            }

            if (genericExpr != other.genericExpr)
            {
                return false;
            }

            if (argHashCodes.Size() != other.argHashCodes.Size())
            {
                return false;
            }

            for (SizeType i = 0; i < argHashCodes.Size(); i++)
            {
                if (argHashCodes[i] != other.argHashCodes[i])
                {
                    return false;
                }
            }

            return true;
        }

        HashCode GetHashCode() const
        {
            HashCode hc;

            if (!IsValid())
            {
                return hc;
            }

            hc.Add(genericExpr->GetHashCode());

            for (auto& argHashCode : argHashCodes)
            {
                hc.Add(argHashCode);
            }

            return hc;
        }
    };

    GenericInstanceCache() = default;
    GenericInstanceCache(const GenericInstanceCache& other) = default;
    GenericInstanceCache& operator=(const GenericInstanceCache& other) = default;
    GenericInstanceCache(GenericInstanceCache&& other) noexcept = default;
    GenericInstanceCache& operator=(GenericInstanceCache&& other) noexcept = default;
    ~GenericInstanceCache() = default;

    /*! \brief Looks up a generic instance in the cache.
        \details
        If the generic instance is not found, an empty optional is returned.

        \return The cached object, or an empty optional if the object is not
    */
    Optional<CachedObject> Lookup(const Key& key) const
    {
        auto it = m_cache.Find(key);

        if (it == m_cache.End())
        {
            return {};
        }

        return it->second;
    }

    /*! \brief Adds a generic instance to the cache.
        \details
        The generic expression must be a generic expression, and the
        instantiated expression must be the result of instantiating the
        generic expression with the given type arguments.

        \return The ID of the cached object.
    */
    uint32 Add(Key key, RC<AstExpression> instantiatedExpr)
    {
        Assert(key.genericExpr != nullptr);
        Assert(instantiatedExpr != nullptr);

        const uint32 id = m_nextId++;

        m_cache.Insert(std::move(key), CachedObject { id, std::move(instantiatedExpr) });

        return id;
    }

private:
    uint32 m_nextId = 0;
    HashMap<Key, CachedObject> m_cache;
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
    UNINSTANTIATED_GENERIC_FLAG = 0x8,
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

} // namespace hyperion::compiler
