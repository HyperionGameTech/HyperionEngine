#include <Lang/compiler/ast/AstNil.hpp>
#include <Lang/compiler/ast/AstInteger.hpp>
#include <Lang/compiler/ast/AstUnsignedInteger.hpp>
#include <Lang/compiler/ast/AstFloat.hpp>
#include <Lang/compiler/ast/AstFalse.hpp>
#include <Lang/compiler/ast/AstTrue.hpp>
#include <Lang/compiler/AstVisitor.hpp>
#include <Lang/compiler/Keywords.hpp>

#include <Lang/compiler/type-system/BuiltinTypes.hpp>

#include <Lang/compiler/emit/BytecodeChunk.hpp>
#include <Lang/compiler/emit/BytecodeUtil.hpp>

#include <Lang/Instructions.hpp>

#include <Core/Types.hpp>

namespace Hyperion {

AstNil::AstNil(const SourceLocation& location)
    : AstConstant(ConstantValue(int64(0), CBS_32), location)
{
}

UniquePtr<Buildable> AstNil::Build(AstVisitor* visitor, Module* mod)
{
    // get active register
    uint8 rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

    return BytecodeUtil::Make<ConstNull>(rp);
}

RC<AstStatement> AstNil::Clone() const
{
    return CloneImpl();
}

Tribool AstNil::IsTrue() const
{
    return Tribool::False();
}

bool AstNil::IsNumber() const
{
    return false;
}

const SymbolType* AstNil::GetExprType() const
{
    return BuiltinTypes::s_nullType;
}

} // namespace Hyperion
