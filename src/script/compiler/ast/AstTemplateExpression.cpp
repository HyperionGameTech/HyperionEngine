#include <script/compiler/ast/AstTemplateExpression.hpp>
#include <script/compiler/ast/AstTemplateInstantiation.hpp>
#include <script/compiler/ast/AstPrototypeSpecification.hpp>
#include <script/compiler/ast/AstTypeObject.hpp>
#include <script/compiler/ast/AstTypeRef.hpp>
#include <script/compiler/ast/AstVariable.hpp>
#include <script/compiler/ast/AstNil.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Module.hpp>
#include <script/compiler/Compiler.hpp>
#include <script/compiler/SemanticAnalyzer.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>
#include <script/compiler/emit/StorageOperation.hpp>

#include <system/Debug.hpp>
#include <util/UTF8.hpp>
#include <script/Hasher.hpp>

namespace hyperion::compiler {

AstTemplateExpression::AstTemplateExpression(
    const RC<AstExpression> &expr,
    const Array<RC<AstParameter>> &generic_params,
    const RC<AstPrototypeSpecification> &return_type_specification,
    AstTemplateExpressionFlags flags,
    const SourceLocation &location
) : AstExpression(location, ACCESS_MODE_LOAD),
    m_expr(expr),
    m_generic_params(generic_params),
    m_return_type_specification(return_type_specification),
    m_flags(flags)
{
}

AstTemplateExpression::AstTemplateExpression(
    const RC<AstExpression> &expr,
    const Array<RC<AstParameter>> &generic_params,
    const RC<AstPrototypeSpecification> &return_type_specification,
    const SourceLocation &location
) : AstTemplateExpression(
        expr,
        generic_params,
        return_type_specification,
        AST_TEMPLATE_EXPRESSION_FLAG_NONE,
        location
    )
{
}

void AstTemplateExpression::Visit(AstVisitor *visitor, Module *mod)
{
    AssertThrow(!m_is_visited);
    m_is_visited = true;

    m_symbol_type = BuiltinTypes::UNDEFINED;

    AssertThrow(visitor != nullptr);
    AssertThrow(mod != nullptr);

    // dirty hacky way to make uninstantiated generic types be treated similarly to c++ templates
    visitor->GetCompilationUnit()->GetErrorList().SuppressErrors(true);

    mod->m_scopes.Open(Scope(SCOPE_TYPE_NORMAL, UNINSTANTIATED_GENERIC_FLAG));

    m_block.Reset(new AstBlock(m_location));

    // visit params before expression to make declarations
    // for things that may be used in the expression

    Array<SymbolTypePtr_t> param_symbol_types;
    param_symbol_types.Reserve(m_generic_params.Size());

    for (RC<AstParameter> &generic_param : m_generic_params) {
        AssertThrow(generic_param != nullptr);

        SymbolTypePtr_t generic_param_type = SymbolType::GenericParameter(
            generic_param->GetName(),
            BuiltinTypes::CLASS_TYPE
        );

        RC<AstTypeObject> generic_param_type_object(new AstTypeObject(
            generic_param_type,
            BuiltinTypes::CLASS_TYPE,
            m_location
        ));

        generic_param_type->SetTypeObject(generic_param_type_object);

        mod->m_scopes.Top().GetIdentifierTable().AddSymbolType(generic_param_type);

        // when visited, will register the SymbolType
        generic_param_type_object->Visit(visitor, mod);

        {
            auto *generic_param_type_object_value_of = generic_param_type_object->GetDeepValueOf();
            AssertThrow(generic_param_type_object_value_of != nullptr);

            SymbolTypePtr_t generic_param_type_object_value_of_type = generic_param_type_object_value_of->GetHeldType();
            AssertThrow(generic_param_type_object_value_of_type != nullptr);
            generic_param_type = generic_param_type_object_value_of_type->GetUnaliased();
        }

        // Keep it around because SymbolType holds a weak reference to it
        m_generic_param_type_objects.PushBack(std::move(generic_param_type_object));

        RC<AstVariableDeclaration> var_decl;

        if (generic_param->IsVariadic()) {
            // if variadic, create a variadic template instantiation
            // so T gets transformed into varargs<T>
            RC<AstTemplateInstantiation> variadic_template_instantiation(new AstTemplateInstantiation(
                RC<AstVariable>(new AstVariable(
                    "varargs",
                    m_location
                )),
                {
                    RC<AstArgument>(new AstArgument(
                        RC<AstTypeRef>(new AstTypeRef(
                            generic_param_type,
                            m_location
                        )),
                        false,
                        false,
                        false,
                        false,
                        "T",
                        m_location
                    ))
                },
                m_location
            ));

            variadic_template_instantiation->Visit(visitor, mod);

            { // set generic_param_type to be the variadic template instantiation type
                auto *variadic_template_instantiation_value_of = variadic_template_instantiation->GetDeepValueOf();
                AssertThrow(variadic_template_instantiation_value_of != nullptr);
                
                SymbolTypePtr_t variadic_template_instantiation_type = variadic_template_instantiation_value_of->GetHeldType();
                AssertThrow(variadic_template_instantiation_type != nullptr);
                variadic_template_instantiation_type = variadic_template_instantiation_type->GetUnaliased();

                // set the variadic template instantiation type as the generic param type
                generic_param_type = std::move(variadic_template_instantiation_type);
            }

            var_decl.Reset(new AstVariableDeclaration(
                generic_param->GetName(),
                nullptr,
                CloneAstNode(variadic_template_instantiation),
                IdentifierFlags::FLAG_CONST,
                m_location
            ));
        } else {
            var_decl.Reset(new AstVariableDeclaration(
                generic_param->GetName(),
                nullptr,
                RC<AstTypeRef>(new AstTypeRef(
                    generic_param_type,
                    SourceLocation::eof
                )),
                IdentifierFlags::FLAG_CONST,
                m_location
            ));
        }

        m_block->AddChild(var_decl);

        param_symbol_types.PushBack(std::move(generic_param_type));
    }
    
    if (m_return_type_specification != nullptr) {
        m_block->AddChild(m_return_type_specification);
    }

    AssertThrow(m_expr != nullptr);

    m_block->AddChild(m_expr);
    m_block->Visit(visitor, mod);

    Array<GenericInstanceTypeInfo::Arg> generic_param_types;
    generic_param_types.Reserve(m_generic_params.Size() + 1); // anotha one for @return
    
    SymbolTypePtr_t expr_return_type;
    SymbolTypePtr_t explicit_return_type = m_return_type_specification != nullptr
        ? m_return_type_specification->GetHeldType()
        : nullptr;

    // if return type has been specified - visit it and check to see
    // if it's compatible with the expression
    if (m_return_type_specification != nullptr) {
        AssertThrow(explicit_return_type != nullptr);
        
        expr_return_type = explicit_return_type;
    } else {
        expr_return_type = BuiltinTypes::PLACEHOLDER;
    }

    generic_param_types.PushBack({
        "@return",
        expr_return_type
    });

    AssertThrow(m_generic_params.Size() == param_symbol_types.Size());

    for (SizeType i = 0; i < m_generic_params.Size(); i++) {
        const RC<AstParameter> &param = m_generic_params[i];
        const SymbolTypePtr_t &param_symbol_type = param_symbol_types[i];

        RC<AstExpression> default_value = CloneAstNode(param->GetDefaultValue());

        if (default_value == nullptr) {
            default_value = RC<AstTypeRef>(new AstTypeRef(
                param_symbol_type,
                SourceLocation::eof
            ));
        }

        generic_param_types.PushBack(GenericInstanceTypeInfo::Arg {
            param->GetName(),
            std::move(param_symbol_type),
            std::move(default_value)
        });
    }

    m_symbol_type = SymbolType::GenericInstance(
        BuiltinTypes::GENERIC_VARIABLE_TYPE,
        GenericInstanceTypeInfo {
            generic_param_types
        }
    );

    AssertThrow(m_symbol_type != nullptr);

    if (m_flags & AST_TEMPLATE_EXPRESSION_FLAG_NATIVE) {
        AssertThrowMsg(m_symbol_type->GetId() == -1, "For native generic expressions, symbol type must not yet be registered");

        m_symbol_type->SetFlags(m_symbol_type->GetFlags() | SYMBOL_TYPE_FLAGS_NATIVE);

        // Create dummy type object
        m_native_dummy_type_object.Reset(new AstTypeObject(
            m_symbol_type,
            BuiltinTypes::CLASS_TYPE,
            m_location
        ));

        m_symbol_type->SetTypeObject(m_native_dummy_type_object);

        // Register our type
        visitor->GetCompilationUnit()->RegisterType(m_symbol_type);
    }

    mod->m_scopes.Close();

    // dirty hacky way to make uninstantiated generic types be treated similarly to c++ templates
    visitor->GetCompilationUnit()->GetErrorList().SuppressErrors(false);
}

std::unique_ptr<Buildable> AstTemplateExpression::Build(AstVisitor *visitor, Module *mod)
{
    AssertThrow(m_is_visited);

    // // build the expression
    // if (m_flags & AST_TEMPLATE_EXPRESSION_FLAG_NATIVE) {
    //     auto chunk = BytecodeUtil::Make<BytecodeChunk>();

    //     AssertThrowMsg(m_symbol_type->GetId() != -1, "For native generic expressions, symbol type must be registered");

    //     // AssertThrow(m_native_dummy_type_object != nullptr);
    //     // chunk->Append(m_native_dummy_type_object->Build(visitor, mod));

    //     chunk->Append(m_block->Build(visitor, mod));

    //     UInt8 rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

    //     // store the result which is in rp into static memory
    //     chunk->Append(BytecodeUtil::Make<Comment>("Store native generic class " + m_symbol_type->GetName() + " in static data at index " + String::ToString(m_symbol_type->GetId())));

    //     auto instr_store_static = BytecodeUtil::Make<StorageOperation>();
    //     instr_store_static->GetBuilder().Store(rp).Static().ByIndex(m_symbol_type->GetId());
    //     chunk->Append(std::move(instr_store_static));

    //     return chunk;
    // }

    return nullptr; // Uninstantiated generic types are not buildable
}

void AstTemplateExpression::Optimize(AstVisitor *visitor, Module *mod)
{
    // do nothing - instantiated generic expressions will be optimized
}

RC<AstStatement> AstTemplateExpression::Clone() const
{
    return CloneImpl();
}

Tribool AstTemplateExpression::IsTrue() const
{
    return Tribool::Indeterminate();
}

bool AstTemplateExpression::MayHaveSideEffects() const
{
    return true;
}

SymbolTypePtr_t AstTemplateExpression::GetExprType() const
{
    AssertThrow(m_is_visited);
    AssertThrow(m_symbol_type != nullptr);

    return m_symbol_type;
}

SymbolTypePtr_t AstTemplateExpression::GetHeldType() const
{
    AssertThrow(m_is_visited);
    AssertThrow(m_expr != nullptr);

    return m_expr->GetHeldType();
}

const AstExpression *AstTemplateExpression::GetValueOf() const
{
    return AstExpression::GetValueOf();
}

const AstExpression *AstTemplateExpression::GetDeepValueOf() const
{
    return AstExpression::GetDeepValueOf();
}

const AstExpression *AstTemplateExpression::GetHeldGenericExpr() const
{
    return m_expr.Get();
}

} // namespace hyperion::compiler
