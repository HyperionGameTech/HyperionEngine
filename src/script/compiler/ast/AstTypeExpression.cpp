#include <script/compiler/ast/AstTypeExpression.hpp>
#include <script/compiler/ast/AstNil.hpp>
#include <script/compiler/ast/AstArrayExpression.hpp>
#include <script/compiler/ast/AstNewExpression.hpp>
#include <script/compiler/ast/AstReturnStatement.hpp>
#include <script/compiler/ast/AstTypeRef.hpp>
#include <script/compiler/ast/AstString.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Keywords.hpp>
#include <script/compiler/Module.hpp>
#include <script/compiler/Configuration.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>

#include <core/lib/Optional.hpp>

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
    m_is_proxy_class(is_proxy_class),
    m_is_uninstantiated_generic(false),
    m_is_visited(false)
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
    AssertThrow(!m_is_visited);

    m_is_uninstantiated_generic = mod->IsInScopeOfType(SCOPE_TYPE_NORMAL, UNINSTANTIATED_GENERIC_FLAG);
    bool has_custom_proto = false;

    // Create scope
    ScopeGuard scope(mod, SCOPE_TYPE_NORMAL, IsEnum() ? ScopeFunctionFlags::ENUM_MEMBERS_FLAG : 0);

    SymbolTypePtr_t prototype_type = SymbolType::Object(
        "$$" + m_name + "Prototype",
        {},
        BuiltinTypes::OBJECT
    );

    SymbolTypePtr_t base_type = BuiltinTypes::OBJECT;

    if (m_base_specification != nullptr) {
        m_base_specification->Visit(visitor, mod);

        AssertThrow(m_base_specification->GetExprType() != nullptr);

        if (auto base_type_inner = m_base_specification->GetHeldType()) {
            base_type = base_type_inner;
        } else {
            visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_not_a_type,
                m_location,
                m_base_specification->GetExprType()->ToString()
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

        m_type_object.Reset(new AstTypeObject(
            m_symbol_type,
            BuiltinTypes::CLASS_TYPE,
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

        if (m_is_uninstantiated_generic) {
            m_symbol_type->GetFlags() |= SYMBOL_TYPE_FLAGS_UNINSTANTIATED_GENERIC;
        }

        m_type_object.Reset(new AstTypeObject(
            m_symbol_type,
            BuiltinTypes::CLASS_TYPE,
            m_enum_underlying_type,
            m_is_proxy_class,
            m_location
        ));

        // special names
        bool proto_found = false;
        bool base_found = false;
        bool name_found = false;

        for (const auto &mem : m_static_members) {
            AssertThrow(mem != nullptr);

            if (mem->GetName() == "$proto") {
                proto_found = true;
                has_custom_proto = true;
            } else if (mem->GetName() == "base") {
                base_found = true;
            } else if (mem->GetName() == "name") {
                name_found = true;
            }
        }

        if (!proto_found) { // no custom '$proto' member, add default.
            m_symbol_type->AddMember(SymbolTypeMember {
                "$proto",
                prototype_type,
                RC<AstTypeRef>(new AstTypeRef(
                    prototype_type,
                    m_location
                ))
            });
        }

        if (!base_found) { // no custom 'base' member, add default
            m_symbol_type->AddMember(SymbolTypeMember {
                "base",
                BuiltinTypes::CLASS_TYPE,
                RC<AstTypeRef>(new AstTypeRef(
                    base_type,
                    m_location
                ))
            });
        }

        if (!name_found) { // no custom 'name' member, add default
            m_symbol_type->AddMember(SymbolTypeMember {
                "name",
                BuiltinTypes::STRING,
                RC<AstString>(new AstString(
                    m_name,
                    m_location
                ))
            });
        }

        if (has_custom_proto) {
            m_symbol_type->GetFlags() |= SYMBOL_TYPE_FLAGS_PROXY;
        }
    }

    m_symbol_type->SetTypeObject(m_type_object);

    // Add the symbol type to the identifier table so that it can be used within the type definition.
    // scope->GetIdentifierTable().AddSymbolType(m_symbol_type);

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

    { // add type aliases to be usable within members
        scope->GetIdentifierTable().AddSymbolType(SymbolType::Alias(
            "SelfType",
            { m_symbol_type }
        ));

        scope->GetIdentifierTable().AddSymbolType(SymbolType::Alias(
            m_symbol_type->GetName(),
            { m_symbol_type }
        ));
    }

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
            
            m_symbol_type->AddMember(SymbolTypeMember {
                mem_name,
                mem_type,
                mem->GetRealAssignment()
            });
        }
    }

    // ===== INSTANCE DATA MEMBERS =====

    Optional<SymbolTypeMember> constructor_member;

    // open the scope for data members
    {
        ScopeGuard instance_data_members(mod, SCOPE_TYPE_TYPE_DEFINITION, 0);

        // Do data members first so we can use them all in functions.

        for (const RC<AstVariableDeclaration> &mem : m_data_members) {
            if (mem != nullptr) {
                mem->Visit(visitor, mod);

                AssertThrow(mem->GetIdentifier() != nullptr);
                
                prototype_type->AddMember(SymbolTypeMember {
                    mem->GetName(),
                    mem->GetIdentifier()->GetSymbolType(),
                    mem->GetRealAssignment()
                });
            }
        }

        for (const RC<AstVariableDeclaration> &mem : m_function_members) {
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

                SymbolTypeMember member {
                    mem->GetName(),
                    mem->GetIdentifier()->GetSymbolType(),
                    mem->GetRealAssignment()
                };

                if (is_constructor_definition) {
                    constructor_member = member;
                }
                
                prototype_type->AddMember(std::move(member));
            }
        }
    }

#if HYP_SCRIPT_CALLABLE_CLASS_CONSTRUCTORS
    bool invoke_found = false;

    // Find the $invoke member on the class object (if it exists)
    for (const SymbolTypeMember &it : m_symbol_type->GetMembers()) {
        if (it.name == "$invoke") {
            invoke_found = true;
            break;
        }
    }

    if (!invoke_found && !IsProxyClass() && !IsEnum()) { // Add an '$invoke' static member, if not already defined.
        Array<RC<AstParameter>> invoke_params;
        invoke_params.Reserve(1);

        // Add `self: typeof SelfType`
        invoke_params.PushBack(RC<AstParameter>(new AstParameter(
            "self", // self: typeof SelfType
            RC<AstPrototypeSpecification>(new AstPrototypeSpecification(
                RC<AstTypeRef>(new AstTypeRef(
                    BuiltinTypes::CLASS_TYPE,
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

        if (constructor_member.HasValue()) {
            // We need to get the arguments for the constructor member, if possible

            const SymbolTypeMember &constructor_member_ref = constructor_member.Get();

            SymbolTypePtr_t constructor_member_type = constructor_member_ref.type;
            AssertThrow(constructor_member_type != nullptr);
            constructor_member_type = constructor_member_type->GetUnaliased();

            // Rely on the fact that the constructor member type is a function type
            if (constructor_member_type->IsGenericInstanceType()) {
                // Get params from generic expression type
                const Array<GenericInstanceTypeInfo::Arg> &params = constructor_member_type->GetGenericInstanceInfo().m_generic_args;
                AssertThrowMsg(params.Size() >= 1, "Generic param list must have at least one parameter (return type should be first).");

                // `self` not guaranteed to be first parameter, so reserve with what we know we have
                invoke_params.Reserve(params.Size() - 1);

                // Start at 2 to skip the return type and `self` parameter - it will be supplied by `new SelfType()`
                for (SizeType i = 2; i < params.Size(); i++) {
                    const GenericInstanceTypeInfo::Arg &param = params[i];

                    SymbolTypePtr_t param_type = param.m_type;
                    AssertThrow(param_type != nullptr);
                    param_type = param_type->GetUnaliased();

                    const bool is_variadic = param_type->IsVarArgsType() && i == params.Size() - 1;

                    RC<AstPrototypeSpecification> param_type_spec(new AstPrototypeSpecification(
                        RC<AstTypeRef>(new AstTypeRef(
                            param_type,
                            m_location
                        )),
                        m_location
                    ));
                    
                    invoke_params.PushBack(RC<AstParameter>(new AstParameter(
                        param.m_name,
                        param_type_spec,
                        CloneAstNode(param.m_default_value),
                        is_variadic,
                        param.m_is_const,
                        param.m_is_ref,
                        m_location
                    )));
                }
            }
        }

        // we don't provide `self` (the class) to the new expression
        
        Array<RC<AstArgument>> invoke_args;
        invoke_args.Reserve(invoke_params.Size() - 1);
        
        // Pass each parameter as an argument to the constructor
        for (SizeType index = 1; index < invoke_params.Size(); index++) {
            const RC<AstParameter> &param = invoke_params[index];
            AssertThrow(param != nullptr);

            invoke_args.PushBack(RC<AstArgument>(new AstArgument(
                RC<AstVariable>(new AstVariable(
                    param->GetName(),
                    m_location
                )),
                false,
                false,
                param->IsRef(),
                param->IsConst(),
                param->GetName(),
                m_location
            )));
        }

        RC<AstBlock> invoke_block(new AstBlock(m_location));

        // Add AstNewExpression to the block
        // `new Self($invoke_args...)`
        invoke_block->AddChild(RC<AstReturnStatement>(new AstReturnStatement(
            RC<AstNewExpression>(new AstNewExpression(
                RC<AstPrototypeSpecification>(new AstPrototypeSpecification(
                    RC<AstTypeRef>(new AstTypeRef(
                        m_symbol_type,
                        m_location
                    )),
                    m_location
                )),
                RC<AstArgumentList>(new AstArgumentList(
                    invoke_args,
                    m_location
                )),
                true, // enable constructor call
                m_location
            )),
            m_location
        )));

        RC<AstFunctionExpression> invoke_expr(new AstFunctionExpression(
            invoke_params,
            RC<AstPrototypeSpecification>(new AstPrototypeSpecification(
                RC<AstTypeRef>(new AstTypeRef(
                    m_symbol_type,
                    m_location
                )),
                m_location
            )),
            invoke_block,
            m_location
        ));

        invoke_expr->Visit(visitor, mod);

        // // add it to the list of static members
        // m_static_members.PushBack(RC<AstVariableDeclaration>(new AstVariableDeclaration(
        //     "$invoke",
        //     nullptr,
        //     invoke_expr,
        //     IdentifierFlags::FLAG_CONST,
        //     m_location
        // )));

        // Add $invoke member to the symbol type
        m_symbol_type->AddMember(SymbolTypeMember {
            "$invoke",
            invoke_expr->GetExprType(),
            CloneAstNode(invoke_expr)
        });
    }
#endif

    { // create a type object for the prototype type
        m_prototype_expr.Reset(new AstTypeObject(
            prototype_type,
            BuiltinTypes::CLASS_TYPE,
            m_location
        ));

        prototype_type->SetTypeObject(m_prototype_expr);
        m_prototype_expr->Visit(visitor, mod); // will register the type. it will be built later.
    }

    { // Finally we visit the newly created AstTypeObject, this will Register our SymbolType
        m_type_object->Visit(visitor, mod);
    }

    { // create a type ref for the symbol type
        m_type_ref.Reset(new AstTypeRef(
            m_symbol_type,
            m_location
        ));

        m_type_ref->Visit(visitor, mod);
    }

    m_is_visited = true;
}

std::unique_ptr<Buildable> AstTypeExpression::Build(AstVisitor *visitor, Module *mod)
{
    AssertThrow(m_symbol_type != nullptr);
    AssertThrow(m_symbol_type->GetId() != -1);

    AssertThrow(m_is_visited);

    // AssertThrowMsg(!m_is_uninstantiated_generic, "Cannot build an uninstantiated generic type.");

    std::unique_ptr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    if (m_prototype_expr != nullptr) {
        chunk->Append(m_prototype_expr->Build(visitor, mod));
    }

    AssertThrow(m_type_object != nullptr);
    chunk->Append(m_type_object->Build(visitor, mod));

    AssertThrow(m_type_ref != nullptr);
    chunk->Append(m_type_ref->Build(visitor, mod));

    return chunk;
}

void AstTypeExpression::Optimize(AstVisitor *visitor, Module *mod)
{
    AssertThrow(m_is_visited);

    AssertThrow(m_type_object != nullptr);
    m_type_object->Optimize(visitor, mod);

    AssertThrow(m_prototype_expr != nullptr);
    m_prototype_expr->Optimize(visitor, mod);

    AssertThrow(m_type_ref != nullptr);
    m_type_ref->Optimize(visitor, mod);
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
    return BuiltinTypes::CLASS_TYPE;
}

SymbolTypePtr_t AstTypeExpression::GetHeldType() const
{
    AssertThrow(m_symbol_type != nullptr);

    return m_symbol_type;
}

const AstExpression *AstTypeExpression::GetValueOf() const
{
    AssertThrow(m_is_visited);
    AssertThrow(m_type_ref != nullptr);

    return m_type_ref->GetValueOf();
}

const AstExpression *AstTypeExpression::GetDeepValueOf() const
{
    AssertThrow(m_is_visited);
    AssertThrow(m_type_ref != nullptr);

    return m_type_ref->GetDeepValueOf();
}

const String &AstTypeExpression::GetName() const
{
    return m_name;
}

} // namespace hyperion::compiler
