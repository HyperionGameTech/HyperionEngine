#ifndef SCOPE_HPP
#define SCOPE_HPP

#include <script/compiler/IdentifierTable.hpp>
#include <script/compiler/type-system/SymbolType.hpp>
#include <script/SourceLocation.hpp>

#include <core/lib/HashMap.hpp>
#include <core/lib/HashMap.hpp>
#include <core/lib/RefCountedPtr.hpp>
#include <core/lib/Optional.hpp>

#include <HashCode.hpp>

#include <utility>

namespace hyperion::compiler {

typedef std::pair<SymbolTypePtr_t, SourceLocation> ReturnType_t;

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
        UInt                id = 0;
        RC<AstExpression>   instantiated_expr;
    };

    struct Key
    {
        RC<AstExpression>   generic_expr; // the original generic expression
        Array<HashCode>     arg_hash_codes; // hashcodes of AstArgument nodes

        Key() = default;

        Key(
            RC<AstExpression> generic_expr,
            Array<HashCode> arg_hash_codes
        ) : generic_expr(std::move(generic_expr)),
            arg_hash_codes(std::move(arg_hash_codes))
        {
            AssertThrow(this->generic_expr != nullptr);
        }

        Key(const Key &other)                   = default;
        Key &operator=(const Key &other)        = default;
        Key(Key &&other) noexcept               = default;
        Key &operator=(Key &&other) noexcept    = default;
        ~Key()                                  = default;

        bool IsValid() const
            { return generic_expr != nullptr; }

        HashCode GetHashCode() const
        {
            HashCode hc;

            if (!IsValid()) {
                return hc;
            }

            hc.Add(generic_expr->GetHashCode());

            for (auto &arg_hash_code : arg_hash_codes) {
                hc.Add(arg_hash_code);
            }

            return hc;
        }
    };

    GenericInstanceCache()                                                  = default;
    GenericInstanceCache(const GenericInstanceCache &other)                 = default;
    GenericInstanceCache &operator=(const GenericInstanceCache &other)      = default;
    GenericInstanceCache(GenericInstanceCache &&other) noexcept             = default;
    GenericInstanceCache &operator=(GenericInstanceCache &&other) noexcept  = default;
    ~GenericInstanceCache()                                                 = default;

    /*! \brief Looks up a generic instance in the cache.
        \details
        If the generic instance is not found, an empty optional is returned.

        \return The cached object, or an empty optional if the object is not
    */
    Optional<CachedObject> Lookup(const Key &key) const
    {
        auto it = m_cache.Find(key);

        if (it == m_cache.End()) {
            return { };
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
    UInt Add(Key key, RC<AstExpression> instantiated_expr)
    {
        AssertThrow(key.generic_expr != nullptr);
        AssertThrow(instantiated_expr != nullptr);

        const UInt id = m_next_id++;

        m_cache.Insert(
            std::move(key),
            CachedObject {
                id,
                std::move(instantiated_expr)
            }
        );

        return id;
    }

private:
    UInt                        m_next_id = 0;
    HashMap<Key, CachedObject>  m_cache;
};

enum ScopeType
{
    SCOPE_TYPE_NORMAL,
    SCOPE_TYPE_FUNCTION,
    SCOPE_TYPE_TYPE_DEFINITION,
    SCOPE_TYPE_LOOP,
    SCOPE_TYPE_GENERIC_INSTANTIATION,
    SCOPE_TYPE_ALIAS_DECLARATION
};

enum ScopeFunctionFlags : int
{
    PURE_FUNCTION_FLAG          = 0x1,
    CLOSURE_FUNCTION_FLAG       = 0x2,
    GENERATOR_FUNCTION_FLAG     = 0x4,
    UNINSTANTIATED_GENERIC_FLAG = 0x8,
    CONSTRUCTOR_DEFINITION_FLAG = 0x10,
    REF_VARIABLE_FLAG           = 0x20,
    CONST_VARIABLE_FLAG         = 0x40,
    ENUM_MEMBERS_FLAG           = 0x80
};

class Scope
{
    friend class Module;
public:
    Scope();
    Scope(ScopeType scope_type, int scope_flags);
    Scope(const Scope &other);

    IdentifierTable &GetIdentifierTable()
        { return m_identifier_table; }

    const IdentifierTable &GetIdentifierTable() const
        { return m_identifier_table; }

    ScopeType GetScopeType() const
        { return m_scope_type; }

    void SetScopeType(ScopeType scope_type)
        { m_scope_type = scope_type; }

    int GetScopeFlags() const
        { return m_scope_flags; }

    void SetScopeFlags(int flags)
        { m_scope_flags = flags; }

    void AddReturnType(const SymbolTypePtr_t &type, const SourceLocation &location) 
        { m_return_types.PushBack({type, location}); }

    const Array<ReturnType_t> &GetReturnTypes() const
        { return m_return_types; }

    RC<Identifier> FindClosureCapture(const String &name) 
    {
        auto it = m_closure_captures.Find(name);

        return it != m_closure_captures.End() ? it->second : nullptr;
    }

    void AddClosureCapture(const String &name, const RC<Identifier> &ident) 
        { m_closure_captures.Insert(name, ident); }

    const HashMap<String, RC<Identifier>> &GetClosureCaptures() const
        { return m_closure_captures; }

    GenericInstanceCache &GetGenericInstanceCache()
        { return m_generic_instance_cache; }

    const GenericInstanceCache &GetGenericInstanceCache() const
        { return m_generic_instance_cache; }


private:
    IdentifierTable                 m_identifier_table;
    ScopeType                       m_scope_type;
    Int                             m_scope_flags;
    Array<ReturnType_t>             m_return_types;
    HashMap<String, RC<Identifier>> m_closure_captures;
    GenericInstanceCache            m_generic_instance_cache;
};

} // namespace hyperion::compiler

#endif
