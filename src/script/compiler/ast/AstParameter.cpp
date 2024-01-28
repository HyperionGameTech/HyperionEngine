#include <script/compiler/ast/AstParameter.hpp>
#include <script/compiler/ast/AstPrototypeSpecification.hpp>
#include <script/compiler/ast/AstTemplateInstantiation.hpp>
#include <script/compiler/ast/AstArgument.hpp>
#include <script/compiler/ast/AstVariable.hpp>
#include <script/compiler/ast/AstTypeRef.hpp>
#include <script/compiler/AstVisitor.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>

#include <system/Debug.hpp>

namespace hyperion::compiler {

AstParameter::AstParameter(
    const String &name, 
    const RC<AstPrototypeSpecification> &type_spec, 
    const RC<AstExpression> &default_param, 
    Bool is_variadic,
    Bool is_const,
    Bool is_ref,
    const SourceLocation &location
) : AstDeclaration(name, location),
    m_type_spec(type_spec),
    m_default_param(default_param),
    m_is_variadic(is_variadic),
    m_is_const(is_const),
    m_is_ref(is_ref),
    m_is_generic_param(false)
{
}

void AstParameter::Visit(AstVisitor *visitor, Module *mod)
{
    AstDeclaration::Visit(visitor, mod);

    // params are `Any` by default
    m_symbol_type = BuiltinTypes::ANY;

    SymbolTypePtr_t specified_symbol_type;

    if (m_type_spec != nullptr) {
        m_type_spec->Visit(visitor, mod);

        if ((specified_symbol_type = m_type_spec->GetHeldType())) {
            m_symbol_type = specified_symbol_type;
        }
    } else {
        m_symbol_type = BuiltinTypes::ANY;
    }

    if (m_default_param != nullptr) {
        m_default_param->Visit(visitor, mod);

        const SymbolTypePtr_t default_param_type = m_default_param->GetExprType();
        AssertThrow(default_param_type != nullptr);

        if (specified_symbol_type == nullptr) { // no symbol type specified; just set to the default arg type
            m_symbol_type = default_param_type;
        } else { // have to check compatibility
            AssertThrow(m_symbol_type == specified_symbol_type); // just sanity check, assigned above
           
            // verify types compatible
            if (!specified_symbol_type->TypeCompatible(*default_param_type, true)) {
                visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                    LEVEL_ERROR,
                    Msg_arg_type_incompatible,
                    m_default_param->GetLocation(),
                    m_symbol_type->ToString(),
                    default_param_type->ToString()
                ));
            }
        }
    }

    // if variadic, then change symbol type to `varargs<T>`
    if (m_is_variadic) {
        m_varargs_type_spec.Reset(new AstPrototypeSpecification(
            RC<AstTemplateInstantiation>(new AstTemplateInstantiation(
                RC<AstVariable>(new AstVariable(
                    "varargs",
                    m_location
                )),
                {
                    RC<AstArgument>(new AstArgument(
                        RC<AstTypeRef>(new AstTypeRef(
                            m_symbol_type,
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
            )),
            m_location
        ));

        m_varargs_type_spec->Visit(visitor, mod);

        auto *varargs_value_of = m_varargs_type_spec->GetDeepValueOf();
        AssertThrow(varargs_value_of != nullptr);

        SymbolTypePtr_t held_type = varargs_value_of->GetHeldType();
        AssertThrow(held_type != nullptr);
        held_type = held_type->GetUnaliased();

        m_symbol_type = held_type;
        AssertThrow(m_symbol_type->IsVarArgsType());
    }

    if (m_identifier != nullptr) {
        m_identifier->SetSymbolType(m_symbol_type);
        m_identifier->SetFlags(m_identifier->GetFlags() | IdentifierFlags::FLAG_ARGUMENT);

        if (m_is_const) {
            m_identifier->SetFlags(m_identifier->GetFlags() | IdentifierFlags::FLAG_CONST);
        }

        if (m_is_ref) {
            m_identifier->SetFlags(m_identifier->GetFlags() | IdentifierFlags::FLAG_REF);
        }

        if (m_default_param != nullptr) {
            m_identifier->SetCurrentValue(m_default_param);
        }
    }
}

std::unique_ptr<Buildable> AstParameter::Build(AstVisitor *visitor, Module *mod)
{
    std::unique_ptr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    AssertThrow(m_identifier != nullptr);

    if (m_varargs_type_spec != nullptr) {
        chunk->Append(m_varargs_type_spec->Build(visitor, mod));
    }

    // get current stack size
    const Int stack_location = visitor->GetCompilationUnit()->GetInstructionStream().GetStackSize();
    // set identifier stack location
    m_identifier->SetStackLocation(stack_location);

    if (IsGenericParam()) {
        AssertThrowMsg(m_default_param != nullptr, "Generic params must be set to a default value");
    
        chunk->Append(m_default_param->Build(visitor, mod));

        UInt8 rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();
        chunk->Append(BytecodeUtil::Make<StoreLocal>(rp));
    }

    visitor->GetCompilationUnit()->GetInstructionStream().IncStackSize();

    return chunk;
}

void AstParameter::Optimize(AstVisitor *visitor, Module *mod)
{
    if (m_varargs_type_spec != nullptr) {
        m_varargs_type_spec->Optimize(visitor, mod);
    }
}

RC<AstStatement> AstParameter::Clone() const
{
    return CloneImpl();
}

SymbolTypePtr_t AstParameter::GetExprType() const
{
    return m_symbol_type;
}

} // namespace hyperion::compiler
