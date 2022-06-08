#include <script/compiler/ast/AstUndefined.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

namespace hyperion {
namespace compiler {

AstUndefined::AstUndefined(const SourceLocation &location)
    : AstConstant(location)
{
}

std::unique_ptr<Buildable> AstUndefined::Build(AstVisitor *visitor, Module *mod)
{
    return nullptr;
}

Pointer<AstStatement> AstUndefined::Clone() const
{
    return CloneImpl();
}

Tribool AstUndefined::IsTrue() const
{
    return Tribool::False();
}

bool AstUndefined::IsNumber() const
{
    return false;
}

hyperion::aint32 AstUndefined::IntValue() const
{
    return 0;
}

hyperion::afloat32 AstUndefined::FloatValue() const
{
    return 0.0f;
}

SymbolTypePtr_t AstUndefined::GetSymbolType() const
{
    return BuiltinTypes::UNDEFINED;
}

std::shared_ptr<AstConstant> AstUndefined::HandleOperator(Operators op_type, AstConstant *right) const
{
    return nullptr;
}

} // namespace compiler
} // namespace hyperion
