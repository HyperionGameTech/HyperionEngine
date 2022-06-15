#include <script/compiler/ast/AstParameter.hpp>
#include <script/compiler/AstVisitor.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>

#include <system/debug.h>

namespace hyperion::compiler {

AstParameter::AstParameter(const std::string &name, 
    const std::shared_ptr<AstPrototypeSpecification> &type_spec, 
    const std::shared_ptr<AstExpression> &default_param, 
    bool is_variadic,
    bool is_const,
    const SourceLocation &location
) : AstDeclaration(name, location),
    m_type_spec(type_spec),
    m_default_param(default_param),
    m_is_variadic(is_variadic),
    m_is_const(is_const),
    m_is_generic_param(false)
{
}

void AstParameter::Visit(AstVisitor *visitor, Module *mod)
{
    AstDeclaration::Visit(visitor, mod);

    // params are `Any` by default
    SymbolTypePtr_t symbol_type = BuiltinTypes::UNDEFINED,
                    specified_symbol_type;

    if (m_type_spec != nullptr) {
        m_type_spec->Visit(visitor, mod);

        if ((specified_symbol_type = m_type_spec->GetHeldType())) {
            symbol_type = specified_symbol_type;
        }
    } else {
        symbol_type = BuiltinTypes::ANY;
    }

    if (m_default_param != nullptr) {
        m_default_param->Visit(visitor, mod);

        const SymbolTypePtr_t default_param_type = m_default_param->GetExprType();
        AssertThrow(default_param_type != nullptr);

        if (specified_symbol_type == nullptr) { // no symbol type specified; just set to the default arg type
            symbol_type = default_param_type;
        } else { // have to check compatibility
            AssertThrow(symbol_type == specified_symbol_type); // just sanity check, assigned above
           
            // verify types compatible
            if (!specified_symbol_type->TypeCompatible(*default_param_type, true)) {
                visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                    LEVEL_ERROR,
                    Msg_arg_type_incompatible,
                    m_default_param->GetLocation(),
                    symbol_type->GetName(),
                    default_param_type->GetName()
                ));
            }
        }
    }

    // if variadic, then make it array of whatever type it is
    if (m_is_variadic) {
        symbol_type = SymbolType::GenericInstance(
            BuiltinTypes::VAR_ARGS,
            GenericInstanceTypeInfo {
                {
                    { m_name, symbol_type }
                }
            }
        );
    }

    if (m_identifier != nullptr) {
        m_identifier->SetSymbolType(symbol_type);

        if (m_is_const) {
            m_identifier->SetFlags(m_identifier->GetFlags() | IdentifierFlags::FLAG_CONST);
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

    // get current stack size
    const int stack_location = visitor->GetCompilationUnit()->GetInstructionStream().GetStackSize();
    // set identifier stack location
    m_identifier->SetStackLocation(stack_location);

    if (m_default_param != nullptr) {
        chunk->Append(m_default_param->Build(visitor, mod));

        uint8_t rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();
        chunk->Append(BytecodeUtil::Make<StoreLocal>(rp));
    }

    // increment stack size
    visitor->GetCompilationUnit()->GetInstructionStream().IncStackSize();

    return std::move(chunk);
}

void AstParameter::Optimize(AstVisitor *visitor, Module *mod)
{
}

Pointer<AstStatement> AstParameter::Clone() const
{
    return CloneImpl();
}

} // namespace hyperion::compiler
