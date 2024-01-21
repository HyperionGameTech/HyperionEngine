#ifndef COMPILATION_UNIT_HPP
#define COMPILATION_UNIT_HPP

#include <script/compiler/Module.hpp>
#include <script/compiler/ErrorList.hpp>
#include <script/compiler/emit/InstructionStream.hpp>
#include <script/compiler/ast/AstNodeBuilder.hpp>
#include <script/compiler/type-system/SymbolType.hpp>
#include <script/compiler/Tree.hpp>

#include <core/lib/String.hpp>
#include <core/lib/Optional.hpp>
#include <core/lib/HashMap.hpp>
#include <core/lib/FlatSet.hpp>

#include <memory>

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

    GenericInstanceCache(Proc<UInt> &&next_id_fn)
        : m_next_id_fn(std::move(next_id_fn))
    {
    }

    GenericInstanceCache(const GenericInstanceCache &other)                 = delete;
    GenericInstanceCache &operator=(const GenericInstanceCache &other)      = delete;
    GenericInstanceCache(GenericInstanceCache &&other) noexcept             = delete;
    GenericInstanceCache &operator=(GenericInstanceCache &&other) noexcept  = delete;
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

        const UInt id = m_next_id_fn();

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
    Proc<UInt>                  m_next_id_fn;
    HashMap<Key, CachedObject>  m_cache;
};

class CompilationUnit
{
public:
    CompilationUnit();
    CompilationUnit(const CompilationUnit &other) = delete;
    ~CompilationUnit();

    Module *GetGlobalModule() { return m_global_module.Get(); }
    const Module *GetGlobalModule() const { return m_global_module.Get(); }

    Module *GetCurrentModule() { return m_module_tree.Top(); }
    const Module *GetCurrentModule() const { return m_module_tree.Top(); }

    ErrorList &GetErrorList() { return m_error_list; }
    const ErrorList &GetErrorList() const { return m_error_list; }

    InstructionStream &GetInstructionStream() { return m_instruction_stream; }
    const InstructionStream &GetInstructionStream() const { return m_instruction_stream; }

    AstNodeBuilder &GetAstNodeBuilder() { return m_ast_node_builder; }
    const AstNodeBuilder &GetAstNodeBuilder() const { return m_ast_node_builder; }

    GenericInstanceCache &GetGenericInstanceCache() { return m_generic_instance_cache; }
    const GenericInstanceCache &GetGenericInstanceCache() const { return m_generic_instance_cache; }

    /**
        Allows a non-builtin type to be used
    */
    void RegisterType(SymbolTypePtr_t &type_ptr);

    /** Looks up the module with the name, taking scope into account.
        Modules with the name that are in the current module or any module
        above the current one will be considered.
    */
    Module *LookupModule(const String &name);

    /** Maps filepath to a vector of modules, so that no module has to be parsed
        and analyze more than once.
    */
    HashMap<String, Array<RC<Module>>>  m_imported_modules;
    Tree<Module*>                       m_module_tree;

private:
    String                  m_exec_path;

    ErrorList               m_error_list;
    InstructionStream       m_instruction_stream;
    AstNodeBuilder          m_ast_node_builder;

    GenericInstanceCache    m_generic_instance_cache;

    // the global module
    RC<Module>              m_global_module;
};

} // namespace hyperion::compiler

#endif
