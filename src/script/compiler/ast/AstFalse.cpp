#include <script/compiler/ast/AstFalse.hpp>
#include <script/compiler/ast/AstTrue.hpp>
#include <script/compiler/ast/AstInteger.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Keywords.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <script/compiler/emit/BytecodeUtil.hpp>

#include <core/Types.hpp>

namespace hyperion {

AstFalse::AstFalse(const SourceLocation& location)
    : AstConstant(ConstantValue(false, CBS_8), location)
{
}

UniquePtr<Buildable> AstFalse::Build(AstVisitor* visitor, Module* mod)
{
    // get active register
    const uint8 rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

    return BytecodeUtil::Make<ConstBool>(rp, false);
}

RC<AstStatement> AstFalse::Clone() const
{
    return CloneImpl();
}

Tribool AstFalse::IsTrue() const
{
    return Tribool::False();
}

bool AstFalse::IsNumber() const
{
    return false;
}

SymbolTypeRef AstFalse::GetExprType() const
{
    return BuiltinTypes::s_boolType;
}

} // namespace hyperion
