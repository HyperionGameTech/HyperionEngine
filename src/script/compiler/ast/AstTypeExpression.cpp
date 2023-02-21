#include <script/compiler/ast/AstTypeExpression.hpp>
#include <script/compiler/ast/AstNil.hpp>
#include <script/compiler/ast/AstArrayExpression.hpp>
#include <script/compiler/ast/AstTypeName.hpp>
#include <script/compiler/ast/AstString.hpp>
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
    const RC<AstPrototypeSpecification> &base_specification,
    const Array<RC<AstVariableDeclaration>> &data_members,
    const Array<RC<AstVariableDeclaration>> &function_members,
    const Array<RC<AstVariableDeclaration>> &static_members,
    const SymbolTypePtr_t &enum_underlying_type,
    bool is_proxy_class,
    const SourceLocation &location
) : AstExpression(location, ACCESS_MODE_LOAD),
    m_name(name),
    m_base_specification(base_specification),
    m_data_members(data_members),
    m_function_members(function_members),
    m_static_members(static_members),
    m_enum_underlying_type(enum_underlying_type),
    m_is_proxy_class(is_proxy_class)
{
}

AstTypeExpression::AstTypeExpression(
    const std::string &name,
    const RC<AstPrototypeSpecification> &base_specification,
    const Array<RC<AstVariableDeclaration>> &data_members,
    const Array<RC<AstVariableDeclaration>> &function_members,
    const Array<RC<AstVariableDeclaration>> &static_members,
    bool is_proxy_class,
    const SourceLocation &location
) : AstTypeExpression(
        name,
        base_specification,
        data_members,
        function_members,
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
        "$$" + m_name + "Prototype",
        {},
        BuiltinTypes::OBJECT
    );

    SymbolTypePtr_t base_type = BuiltinTypes::OBJECT;

    if (m_base_specification != nullptr) {
        m_base_specification->Visit(visitor, mod);

        if (auto base_type_inner = m_base_specification->GetHeldType()) {
            base_type = base_type_inner;
        } else {
            visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_internal_error,
                m_location
            ));
        }
    }

    m_symbol_type = SymbolType::Extend(
        m_name,
        base_type,
        {}
    );

    if (m_is_proxy_class) {
        m_symbol_type->GetFlags() |= SYMBOL_TYPE_FLAGS_PROXY;
    }

    m_expr.Reset(new AstTypeObject(
        m_symbol_type,
        RC<AstVariable>(new AstVariable(BuiltinTypes::CLASS_TYPE->GetName(), m_location)),
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
    bool name_found = false;

    for (const auto &mem : m_static_members) {
        AssertThrow(mem != nullptr);

        if (mem->GetName() == "$proto") {
            proto_found = true;
        } else if (mem->GetName() == "base") {
            base_found = true;
        } else if (mem->GetName() == "name") {
            name_found = true;
        }
    }

    if (!proto_found) { // no custom '$proto' member, add default.
        m_symbol_type->AddMember(SymbolMember_t {
            "$proto",
            prototype_type,
            RC<AstTypeObject>(new AstTypeObject(
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
            m_base_specification != nullptr
                ? CloneAstNode(m_base_specification->GetExpr())
                : RC<AstExpression>(new AstVariable(BuiltinTypes::CLASS_TYPE->GetName(), m_location))
        });
    }

    if (!name_found) { // no custom 'name' member, add default
        m_symbol_type->AddMember(SymbolMember_t {
            "name",
            BuiltinTypes::STRING,
            RC<AstString>(new AstString(
                m_name,
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

    m_combined_members.Reserve(m_data_members.Size() + m_function_members.Size());

    // open the scope for data members
    {
        ScopeGuard instance_data_members(mod, SCOPE_TYPE_TYPE_DEFINITION, 0);

        // Do data members first so we can use them all in functions.

        for (const auto &mem : m_data_members) {
            if (mem != nullptr) {
                mem->Visit(visitor, mod);

                AssertThrow(mem->GetIdentifier() != nullptr);
                
                prototype_type->AddMember(SymbolMember_t(
                    mem->GetName(),
                    mem->GetIdentifier()->GetSymbolType(),
                    mem->GetRealAssignment()
                ));

                m_combined_members.PushBack(mem);
            }
        }

        for (const auto &mem : m_function_members) {
            if (mem != nullptr) {
                // if name of the method matches that of the class, it is the constructor.
                const bool is_constructor_definition = mem->GetName() == m_name;

                if (is_constructor_definition) {
                    // ScopeGuard constructor_definition_scope(mod, SCOPE_TYPE_FUNCTION, CONSTRUCTOR_DEFINITION_FLAG);

                    mem->ApplyIdentifierFlags(FLAG_CONSTRUCTOR);
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

                m_combined_members.PushBack(mem);
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

RC<AstStatement> AstTypeExpression::Clone() const
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
