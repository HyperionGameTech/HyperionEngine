#include <Lang/Compiler/Ast/AstParameter.hpp>
#include <Lang/Compiler/Ast/AstTypeSpecifier.hpp>
#include <Lang/Compiler/Ast/AstTemplateInstantiation.hpp>
#include <Lang/Compiler/Ast/AstArgument.hpp>
#include <Lang/Compiler/Ast/AstVariable.hpp>
#include <Lang/Compiler/Ast/AstTypeRef.hpp>
#include <Lang/Compiler/AstVisitor.hpp>

#include <Lang/Compiler/TypeSystem/BuiltinTypes.hpp>

#include <Lang/Compiler/Emit/BytecodeChunk.hpp>
#include <Lang/Compiler/Emit/BytecodeUtil.hpp>

#include <Core/Debug/Debug.hpp>

#include <AstParameter.generated.inl>

namespace Hyperion {

AstParameter::AstParameter(
    const String& name,
    const Handle<AstTypeSpecifier>& typeSpec,
    const Handle<AstExpression>& defaultParam,
    bool isVariadic,
    EnumFlags<IdentifierFlags> flags,
    const SourceLocation& location)
    : AstDeclaration(name, flags | IdentifierFlags::ARGUMENT, location),
      m_typeSpec(typeSpec),
      m_defaultParam(defaultParam),
      m_isVariadic(isVariadic),
      m_symbolType(nullptr)
{
}

void AstParameter::Visit(AstVisitor* visitor, Module* mod)
{
    AstDeclaration::Visit(visitor, mod);

    // params are `Any` by default
    m_symbolType = BuiltinTypes::s_anyType;

    const SymbolType* specifiedSymbolType = nullptr;

    if (m_typeSpec != nullptr)
    {
        m_typeSpec->Visit(visitor, mod);

        if ((specifiedSymbolType = m_typeSpec->GetHeldType()))
        {
            m_symbolType = specifiedSymbolType;
        }
    }
    else
    {
        m_symbolType = BuiltinTypes::s_anyType;
    }

    if (m_defaultParam != nullptr)
    {
        if (IsRef())
        {
            // error; cannot create reference parameter with default argument
            visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_cannot_create_reference,
                m_location));
        }

        m_defaultParam->Visit(visitor, mod);

        const SymbolType* defaultParamType = m_defaultParam->GetExprType();
        Assert(defaultParamType != nullptr);

        if (specifiedSymbolType == nullptr)
        { // no symbol type specified; just set to the default arg type
            m_symbolType = defaultParamType;
        }
        else
        {                                                // have to check compatibility
            Assert(m_symbolType == specifiedSymbolType); // just sanity check, assigned above

            // verify types compatible
            if (!specifiedSymbolType->TypeCompatible(
                    *defaultParamType,
                    /* strictNumbers */ true,
                    /* strictAny */ true,
                    /* strictEnum */ true,
                    /* strictNull */ true,
                    /* strictDownCasting */ true))
            {
                visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                    LEVEL_ERROR,
                    Msg_arg_type_incompatible,
                    m_defaultParam->GetLocation(),
                    m_symbolType->ToString(),
                    defaultParamType->ToString()));
            }
        }
    }

    // if variadic, then change symbol type to `varargs<T>`
    if (m_isVariadic)
    {
        m_varargsTypeSpec = MakeHandle<AstTemplateInstantiation>(
            MakeHandle<AstTypeRef>(BuiltinTypes::s_varArgsType, m_location),
            Array<Handle<AstTypeSpecifier>> {
                MakeHandle<AstTypeSpecifier>(
                    MakeHandle<AstTypeRef>(m_symbolType, m_location),
                    m_location)
            },
            nullptr, // no function return type
            m_location);

        m_varargsTypeSpec->Visit(visitor, mod);

        const SymbolType* heldType = m_varargsTypeSpec->GetHeldType();
        Assert(heldType != nullptr);
        heldType = heldType->GetUnaliased();

        m_symbolType = heldType;
        Assert(m_symbolType->IsVarArgsType());
    }

    if (m_identifier != nullptr)
    {
        m_identifier->SetSymbolType(m_symbolType);

        if (m_defaultParam != nullptr)
        {
            m_identifier->SetCurrentValue(m_defaultParam);
        }
    }
}

UniquePtr<Buildable> AstParameter::Build(AstVisitor* visitor, Module* mod)
{
    UniquePtr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    Assert(m_identifier != nullptr);

    if (m_varargsTypeSpec != nullptr)
    {
        chunk->Append(m_varargsTypeSpec->Build(visitor, mod));
    }

    // get current stack size
    const int stackLocation = visitor->GetCompilationUnit()->GetInstructionStream().GetStackSize();
    // set identifier stack location
    m_identifier->SetStackLocation(stackLocation);

    visitor->GetCompilationUnit()->GetInstructionStream().IncStackSize();

    return chunk;
}

void AstParameter::Optimize(AstVisitor* visitor, Module* mod)
{
    if (m_varargsTypeSpec != nullptr)
    {
        m_varargsTypeSpec->Optimize(visitor, mod);
    }
}

Handle<AstStatement> AstParameter::Clone() const
{
    return CloneImpl();
}

} // namespace Hyperion
