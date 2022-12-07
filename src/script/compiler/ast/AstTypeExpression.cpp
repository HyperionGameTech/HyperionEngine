#include <script/compiler/ast/AstTypeExpression.hpp>
#include <script/compiler/ast/AstNil.hpp>
#include <script/compiler/ast/AstArrayExpression.hpp>
#include <script/compiler/ast/AstTypeName.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Keywords.hpp>
#include <script/compiler/Module.hpp>
#include <script/compiler/Configuration.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>

#include <script/Hasher.hpp>
#include <system/Debug.hpp>

namespace hyperion::compiler {

AstTypeExpression::AstTypeExpression(
    const std::string &name,
    const std::shared_ptr<AstPrototypeSpecification> &base_specification,
    const std::vector<std::shared_ptr<AstVariableDeclaration>> &members,
    const std::vector<std::shared_ptr<AstVariableDeclaration>> &static_members,
    const SymbolTypePtr_t &enum_underlying_type,
    bool is_proxy_class,
    const SourceLocation &location
) : AstExpression(location, ACCESS_MODE_LOAD),
    m_name(name),
    m_base_specification(base_specification),
    m_members(members),
    m_static_members(static_members),
    m_enum_underlying_type(enum_underlying_type),
    m_is_proxy_class(is_proxy_class)
{
}

AstTypeExpression::AstTypeExpression(
    const std::string &name,
    const std::shared_ptr<AstPrototypeSpecification> &base_specification,
    const std::vector<std::shared_ptr<AstVariableDeclaration>> &members,
    const std::vector<std::shared_ptr<AstVariableDeclaration>> &static_members,
    bool is_proxy_class,
    const SourceLocation &location
) : AstTypeExpression(
        name,
        base_specification,
        members,
        static_members,
        nullptr,
        is_proxy_class,
        location
    )
{
}

void AstTypeExpression::Visit(AstVisitor *visitor, Module *mod)
{   
    AssertThrow(visitor != nullptr && mod != nullptr);

    auto prototype_type = SymbolType::Object(
        m_name + "Instance", // Prototype type
        {},
        BuiltinTypes::OBJECT
    );

    // TODO: allow custom bases (which would have to extend Type somewhere)
    SymbolTypePtr_t base_type = BuiltinTypes::CLASS_TYPE;

    if (m_base_specification != nullptr) {
        m_base_specification->Visit(visitor, mod);

        if (auto base_type_inner = m_base_specification->GetHeldType()) {
            base_type = base_type_inner;
        }
    }

    m_symbol_type = SymbolType::Extend(
        m_name,
        base_type,
        {}
    );

    if (m_is_proxy_class) {
        m_symbol_type->GetFlags() |= SymbolTypeFlags::SYMBOL_TYPE_FLAGS_PROXY;
    }

    m_expr.reset(new AstTypeObject(
        m_symbol_type,
        nullptr, // prototype - TODO
        m_enum_underlying_type,
        m_is_proxy_class,
        m_location
    ));

    ScopeGuard scope(mod, SCOPE_TYPE_NORMAL, IsEnum() ? ScopeFunctionFlags::ENUM_MEMBERS_FLAG : 0);

    // add symbol type to be usable within members
    scope->GetIdentifierTable().AddSymbolType(m_symbol_type);

    {
        // type alias
        SymbolTypePtr_t internal_type = SymbolType::Alias(
            "Self",
            { m_symbol_type }
        );

        scope->GetIdentifierTable().AddSymbolType(internal_type);
    }

    // check if one with the name $proto already exists.
    bool proto_found = false;
    bool base_found = false;

    for (const auto &mem : m_static_members) {
        AssertThrow(mem != nullptr);

        if (mem->GetName() == "$proto") {
            proto_found = true;
        } else if (mem->GetName() == "base") {
            base_found = true;
        }
    }

    if (!proto_found) { // no custom '$proto' member, add default.
        m_symbol_type->AddMember(SymbolMember_t {
            "$proto",
            prototype_type,
            std::shared_ptr<AstTypeObject>(new AstTypeObject(
                prototype_type,
                nullptr,
                m_location
            ))
        });
    }

    if (!base_found) { // no custom 'base' member, add default
        m_symbol_type->AddMember(SymbolMember_t {
            "base",
            base_type,
            std::shared_ptr<AstTypeObject>(new AstTypeObject(
                base_type,
                nullptr,
                m_location
            ))
        });
    }

    m_symbol_type->SetTypeObject(m_expr);

    // ===== STATIC DATA MEMBERS ======
    {
        ScopeGuard static_data_members(mod, SCOPE_TYPE_TYPE_DEFINITION, 0);

        for (const auto &mem : m_static_members) {
            AssertThrow(mem != nullptr);
            mem->Visit(visitor, mod);

            std::string mem_name = mem->GetName();

            AssertThrow(mem->GetIdentifier() != nullptr);
            SymbolTypePtr_t mem_type = mem->GetIdentifier()->GetSymbolType();
            
            m_symbol_type->AddMember(SymbolMember_t {
                mem_name,
                mem_type,
                mem->GetRealAssignment()
            });
        }
    }

    // ===== INSTANCE DATA MEMBERS =====

    // open the scope for data members
    {
        ScopeGuard instance_data_members(mod, SCOPE_TYPE_TYPE_DEFINITION, 0);

        for (const auto &mem : m_members) {
            if (mem != nullptr) {
                // if name of the method matches that of the class, it is the constructor.
                const bool is_constructor_definition = mem->GetName() == m_name;

                if (is_constructor_definition) {
                    ScopeGuard constructor_definition_scope(mod, SCOPE_TYPE_FUNCTION, CONSTRUCTOR_DEFINITION_FLAG);

                    mem->SetName("$construct");
                    mem->Visit(visitor, mod);
                } else {
                    mem->Visit(visitor, mod);
                }

                AssertThrow(mem->GetIdentifier() != nullptr);
                
                prototype_type->AddMember(SymbolMember_t(
                    mem->GetName(),
                    mem->GetIdentifier()->GetSymbolType(),
                    mem->GetRealAssignment()
                ));
            }
        }
    }

    m_expr->Visit(visitor, mod);
}

std::unique_ptr<Buildable> AstTypeExpression::Build(AstVisitor *visitor, Module *mod)
{
    std::unique_ptr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    AssertThrow(m_expr != nullptr);
    chunk->Append(m_expr->Build(visitor, mod));

    return chunk;
}

void AstTypeExpression::Optimize(AstVisitor *visitor, Module *mod)
{
    AssertThrow(m_expr != nullptr);
    m_expr->Optimize(visitor, mod);
}

Pointer<AstStatement> AstTypeExpression::Clone() const
{
    return CloneImpl();
}

bool AstTypeExpression::IsLiteral() const
{
    return false;
}

Tribool AstTypeExpression::IsTrue() const
{
    return Tribool::True();
}

bool AstTypeExpression::MayHaveSideEffects() const
{
    return true;
}

SymbolTypePtr_t AstTypeExpression::GetExprType() const
{
    AssertThrow(m_expr != nullptr);
    return m_expr->GetExprType();
}

const AstExpression *AstTypeExpression::GetValueOf() const
{
    AssertThrow(m_expr != nullptr);
    return m_expr->GetValueOf();
}

const AstExpression *AstTypeExpression::GetDeepValueOf() const
{
    AssertThrow(m_expr != nullptr);
    return m_expr->GetDeepValueOf();
}

const std::string &AstTypeExpression::GetName() const
{
    return m_name;
}

} // namespace hyperion::compiler
