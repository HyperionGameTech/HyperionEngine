#include <script/compiler/ast/AstTypeName.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/type-system/BuiltinTypes.hpp>
#include <script/compiler/ast/AstConstant.hpp>
#include <script/compiler/ast/AstInteger.hpp>
#include <script/compiler/Scope.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>
#include <script/compiler/emit/StorageOperation.hpp>

#include <script/Instructions.hpp>
#include <system/debug.h>

#include <iostream>

namespace hyperion::compiler {

AstTypeName::AstTypeName(
    const std::string &name,
    const SourceLocation &location
) : AstIdentifier(name, location)
{
}

void AstTypeName::Visit(AstVisitor *visitor, Module *mod)
{
    AstIdentifier::Visit(visitor, mod);

    AssertThrow(m_properties.GetIdentifierType() != IDENTIFIER_TYPE_UNKNOWN);

    m_symbol_type = BuiltinTypes::UNDEFINED;

    switch (m_properties.GetIdentifierType()) {
        case IDENTIFIER_TYPE_TYPE:
            m_symbol_type = m_properties.m_found_type;

            break;
        case IDENTIFIER_TYPE_NOT_FOUND:
            visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_undeclared_identifier,
                m_location,
                m_name,
                mod->GenerateFullModuleName()
            ));
            break;
        default:
            visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_not_a_type,
                m_location,
                m_name
            ));
            break;
    }
}

std::unique_ptr<Buildable> AstTypeName::Build(AstVisitor *visitor, Module *mod)
{
    return nullptr; // TODO
}

void AstTypeName::Optimize(AstVisitor *visitor, Module *mod)
{
}

Pointer<AstStatement> AstTypeName::Clone() const
{
    return CloneImpl();
}

Tribool AstTypeName::IsTrue() const
{
    return Tribool::True();
}

bool AstTypeName::MayHaveSideEffects() const
{
    return false;
}

bool AstTypeName::IsLiteral() const
{
    return false;
}

SymbolTypePtr_t AstTypeName::GetExprType() const
{
    AssertThrow(m_symbol_type != nullptr);

    return m_symbol_type;
}

} // namespace hyperion::compiler
