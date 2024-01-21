#include <script/compiler/ast/AstTypeExpression.hpp>
#include <script/compiler/ast/AstNil.hpp>
#include <script/compiler/ast/AstArrayExpression.hpp>
#include <script/compiler/ast/AstNewExpression.hpp>
#include <script/compiler/ast/AstTypeName.hpp>
#include <script/compiler/ast/AstReturnStatement.hpp>
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
    const String &name,
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
    const String &name,
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

    if (IsEnum()) {
        // Create a generic instance of the enum type
        m_symbol_type = SymbolType::GenericInstance(
            BuiltinTypes::ENUM_TYPE,
            GenericInstanceTypeInfo {
                {
                    { "of", m_enum_underlying_type }
                }
            }
        );

        m_expr.Reset(new AstTypeObject(
            m_symbol_type,
            nullptr,
            m_enum_underlying_type,
            m_is_proxy_class,
            m_location
        ));
    } else {
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

        // special names
        bool proto_found = false;
        bool base_found = false;
        bool name_found = false;
        bool invoke_found = false;

        for (const auto &mem : m_static_members) {
            AssertThrow(mem != nullptr);

            if (mem->GetName() == "$proto") {
                proto_found = true;
            } else if (mem->GetName() == "base") {
                base_found = true;
            } else if (mem->GetName() == "name") {
                name_found = true;
            } else if (mem->GetName() == "$invoke") {
                invoke_found = true;
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
                BuiltinTypes::CLASS_TYPE,
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
    }

    m_symbol_type->SetTypeObject(m_expr);

    // Create scope

    ScopeGuard scope(mod, SCOPE_TYPE_NORMAL, IsEnum() ? ScopeFunctionFlags::ENUM_MEMBERS_FLAG : 0);

    // if (mod->IsInScopeOfType(SCOPE_TYPE_NORMAL, UNINSTANTIATED_GENERIC_FLAG)) { // add symbol type to be usable within members
    //     SymbolTypePtr_t placeholder_type = m_symbol_type;

    //     // If the type is generic, we need to use a placeholder type
    //     // so that we can use the type within the type definition, without having to
    //     // instantiate it first.
    //     // if (m_symbol_type->IsGeneric()) {
    //         placeholder_type = SymbolType::Alias(
    //             m_symbol_type->GetName(),
    //             { BuiltinTypes::PLACEHOLDER }
    //         );
    //     // }

    //     scope->GetIdentifierTable().AddSymbolType(placeholder_type);
    // }

    // {
    //     // type alias
    //     SymbolTypePtr_t internal_type = SymbolType::Alias(
    //         "Self",
    //         { m_symbol_type }
    //     );

    //     scope->GetIdentifierTable().AddSymbolType(internal_type);
    // }

    // ===== STATIC DATA MEMBERS ======
    {
        ScopeGuard static_data_members(mod, SCOPE_TYPE_TYPE_DEFINITION, 0);

        // Add a static $invoke method which will be used to invoke the constructor.
        

        for (const auto &mem : m_static_members) {
            AssertThrow(mem != nullptr);
            mem->Visit(visitor, mod);

            String mem_name = mem->GetName();

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

    SymbolMember_t constructor_member;

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

                SymbolMember_t member(
                    mem->GetName(),
                    mem->GetIdentifier()->GetSymbolType(),
                    mem->GetRealAssignment()
                );

                if (is_constructor_definition) {
                    constructor_member = member;
                }
                
                prototype_type->AddMember(std::move(member));

                m_combined_members.PushBack(mem);
            }
        }
    }

#if HYP_SCRIPT_CALLABLE_CLASS_CONSTRUCTORS
    if (!invoke_found && !IsProxyClass() && !IsEnum()) { // Add an '$invoke' static member, if not already defined.
        ScopeGuard invoke_scope(mod, SCOPE_TYPE_FUNCTION, 0);

        Array<RC<AstParameter>> invoke_params;
        invoke_params.Reserve(2); // for 'self', varargs
        invoke_params.PushBack(RC<AstParameter>(new AstParameter(
            "self", // self: Class
            RC<AstPrototypeSpecification>(new AstPrototypeSpecification(
                RC<AstVariable>(new AstVariable(
                    class_type->GetName(),
                    m_location
                )),
                m_location
            )),
            nullptr,
            false,
            false,
            false,
            m_location
        )));

        invoke_params.PushBack(RC<AstParameter>(new AstParameter(
            "args", // args: Any...
            RC<AstPrototypeSpecification>(new AstPrototypeSpecification(
                RC<AstVariable>(new AstVariable(
                    BuiltinTypes::ANY->GetName(),
                    m_location
                )),
                m_location
            )),
            nullptr,
            true,
            false,
            false,
            m_location
        )));

        Array<RC<AstArgument>> invoke_args;
        invoke_args.Reserve(2); // for 'self', varargs
        invoke_args.PushBack(RC<AstArgument>(new AstArgument(
            RC<AstVariable>(new AstVariable(
                "self",
                m_location
            )),
            false,
            false,
            false,
            false,
            "self",
            m_location
        )));

        invoke_args.PushBack(RC<AstArgument>(new AstArgument(
            RC<AstVariable>(new AstVariable(
                "args",
                m_location
            )),
            true,
            false,
            false,
            false,
            "args",
            m_location
        )));

        // if (SymbolTypePtr_t constructor_member_type = std::get<1>(constructor_member)) {
        //     if (constructor_member_type->IsGeneric()) {
        //         invoke_params.Reserve(1 + constructor_member_type->GetGenericInstanceInfo().m_generic_args.Size());
        //         invoke_args.Reserve(1 + constructor_member_type->GetGenericInstanceInfo().m_generic_args.Size());

        //         for (const auto &param : constructor_member_type->GetGenericInstanceInfo().m_generic_args) {
        //             RC<AstPrototypeSpecification> param_type_spec(new AstPrototypeSpecification(
        //                 RC<AstVariable>(new AstVariable(
        //                     param.m_type->GetName(),
        //                     m_location
        //                 )),
        //                 m_location
        //             ));
                    
        //             invoke_params.PushBack(RC<AstParameter>(new AstParameter(
        //                 param.m_name,
        //                 param_type_spec,
        //                 CloneAstNode(param.m_default_value),
        //                 false,
        //                 param.m_is_const,
        //                 param.m_is_ref,
        //                 m_location
        //             )));

        //             invoke_args.PushBack(RC<AstArgument>(new AstArgument(
        //                 RC<AstVariable>(new AstVariable(
        //                     param.m_name,
        //                     m_location
        //                 )),
        //                 false,
        //                 false,
        //                 param.m_is_ref,
        //                 param.m_is_const,
        //                 param.m_name,
        //                 m_location
        //             )));
        //         }
        //     }
        // }

        RC<AstPrototypeSpecification> self_type_spec(new AstPrototypeSpecification(
            RC<AstVariable>(new AstVariable(
                m_symbol_type->GetName(),
                m_location
            )),
            m_location
        ));

        RC<AstBlock> invoke_block(new AstBlock(m_location));

        // Add AstNewExpression to the block
        invoke_block->AddChild(RC<AstReturnStatement>(new AstReturnStatement(
            RC<AstNewExpression>(new AstNewExpression(
                RC<AstPrototypeSpecification>(new AstPrototypeSpecification(
                    RC<AstVariable>(new AstVariable(
                        m_symbol_type->GetName(),
                        m_location
                    )),
                    m_location
                )),
                RC<AstArgumentList>(new AstArgumentList(
                    invoke_args,
                    m_location
                )),
                true,
                m_location
            )),
            m_location
        )));

        RC<AstFunctionExpression> invoke_expr(new AstFunctionExpression(
            invoke_params,
            nullptr,
            invoke_block,
            m_location
        ));

        // Add $invoke member to the prototype type

        invoke_expr->Visit(visitor, mod);

        // add it to the list of static members
        m_static_members.PushBack(RC<AstVariableDeclaration>(new AstVariableDeclaration(
            "$invoke",
            nullptr,
            invoke_expr,
            IdentifierFlags::FLAG_CONST,
            m_location
        )));

        // Add $invoke member to the symbol type
        m_symbol_type->AddMember(SymbolMember_t {
            "$invoke",
            invoke_expr->GetExprType(),
            invoke_expr
        });
    }
#endif

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

SymbolTypePtr_t AstTypeExpression::GetHeldType() const
{
    AssertThrow(m_symbol_type != nullptr);

    return m_symbol_type;
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

const String &AstTypeExpression::GetName() const
{
    return m_name;
}

} // namespace hyperion::compiler
