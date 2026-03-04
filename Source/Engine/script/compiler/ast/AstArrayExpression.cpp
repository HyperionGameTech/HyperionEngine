#include <script/compiler/ast/AstArrayExpression.hpp>
#include <script/compiler/ast/AstVariable.hpp>
#include <script/compiler/ast/AstArgument.hpp>
#include <script/compiler/ast/AstMember.hpp>
#include <script/compiler/ast/AstCallExpression.hpp>
#include <script/compiler/ast/AstAsExpression.hpp>
#include <script/compiler/ast/AstTypeSpecifier.hpp>
#include <script/compiler/ast/AstTemplateInstantiation.hpp>
#include <script/compiler/ast/AstTypeRef.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Module.hpp>
#include <script/compiler/Compiler.hpp>
#include <script/compiler/Configuration.hpp>
#include <script/compiler/SemanticAnalyzer.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>
#include <script/compiler/emit/StorageOperation.hpp>

#include <script/Instructions.hpp>

#include <Core/reflection/BoxedValue.hpp>

#include <Core/debug/Debug.hpp>

#include <Core/containers/FlatSet.hpp>

#include <Core/Types.hpp>

namespace Hyperion {

AstArrayExpression::AstArrayExpression(
    const Array<RC<AstExpression>>& members,
    const SourceLocation& location)
    : AstExpression(location, ACCESS_MODE_LOAD),
      m_members(members),
      m_heldType(BuiltinTypes::s_anyType),
      m_exprType(nullptr)
{
}

void AstArrayExpression::Visit(AstVisitor* visitor, Module* mod)
{
    m_exprType = BuiltinTypes::s_errorType;

    m_replacedMembers.Reserve(m_members.Size());

    FlatSet<const SymbolType*> heldTypes;

    for (auto& member : m_members)
    {
        Assert(member != nullptr);
        member->Visit(visitor, mod);

        if (member->GetExprType() != nullptr)
        {
            heldTypes.Insert(member->GetExprType());
        }
        else
        {
            heldTypes.Insert(BuiltinTypes::s_anyType);
        }

        m_replacedMembers.PushBack(CloneAstNode(member));
    }

    for (const auto& it : heldTypes)
    {
        Assert(it != nullptr);

        if (m_heldType->IsOrHasBase(*BuiltinTypes::s_errorType))
        {
            // `Undefined` invalidates the array type
            break;
        }

        if (m_heldType->IsAnyType())
        {
            // take first item found that is not `Any`
            m_heldType = it;
        }
        else if (m_heldType->TypeCompatible(
                     *it,
                     /* strictNumbers */ false,
                     /* strictAny */ false,
                     /* strictEnum */ false,
                     /* strictNull */ true,
                     /* strictDownCasting */ true))
        { // allow non-strict numbers because we can do a cast
            m_heldType = SymbolType::TypePromotion(m_heldType, it);
        }
        else
        {
            // more than one differing type, use Any.
            m_heldType = BuiltinTypes::s_anyType;
            break;
        }
    }

    for (size_t index = 0; index < m_replacedMembers.Size(); index++)
    {
        auto& replacedMember = m_replacedMembers[index];
        Assert(replacedMember != nullptr);

        auto& member = m_members[index];
        Assert(member != nullptr);

        if (const SymbolType* exprType = member->GetExprType())
        {
            if (!exprType->TypeEqual(*m_heldType))
            {
                // replace with a cast to the held type
                replacedMember.Reset(new AstAsExpression(
                    replacedMember,
                    RC<AstTypeSpecifier>(new AstTypeSpecifier(
                        RC<AstTypeRef>(new AstTypeRef(m_heldType, member->GetLocation())),
                        member->GetLocation())),
                    member->GetLocation()));
            }
        }

        replacedMember->Visit(visitor, mod);
    }

    AstTemplateInstantiation genericInst(
        RC<AstTypeRef>(new AstTypeRef(BuiltinTypes::s_arrayType, m_location)),
        { RC<AstTypeSpecifier>(new AstTypeSpecifier(RC<AstTypeRef>(new AstTypeRef(m_heldType, m_location)), m_location)) },
        nullptr, // no function return type
        m_location);

    genericInst.Visit(visitor, mod);

    const SymbolType* arrayType = genericInst.GetHeldType();

    if (!arrayType)
    {
        // error already reported
        return;
    }

    m_exprType = arrayType;
}

UniquePtr<Buildable> AstArrayExpression::Build(AstVisitor* visitor, Module* mod)
{
    UniquePtr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    // get active register
    uint8 rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

    const bool hasSideEffects = MayHaveSideEffects();
    const uint32 arraySize = uint32(m_members.Size());

    const uint8 arrayReg = rp;
    int arrayStackLocation = -1;

    { // add NEW_ARRAY instruction
        auto instrNewArray = BytecodeUtil::Make<RawOperation<>>();
        instrNewArray->opcode = NEW_ARRAY;
        instrNewArray->Accept<uint8>(arrayReg);
        instrNewArray->Accept<uint32>(arraySize);
        chunk->Append(std::move(instrNewArray));
    }

    if (hasSideEffects)
    {
        // if the elements may have side effects, we need to keep the array on the stack instead of just in a register
        arrayStackLocation = visitor->GetCompilationUnit()->GetInstructionStream().GetStackSize();
        visitor->GetCompilationUnit()->GetInstructionStream().IncStackSize();

        auto instrPush = BytecodeUtil::Make<RawOperation<>>();
        instrPush->opcode = PUSH;
        instrPush->Accept<uint8>(rp);
        chunk->Append(std::move(instrPush));
    }
    else
    {
        // claim register for array
        visitor->GetCompilationUnit()->GetInstructionStream().IncRegisterUsage();

        // get active register
        rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();
    }

    // assign all array items
    for (size_t index = 0; index < m_replacedMembers.Size(); index++)
    {
        auto& member = m_replacedMembers[index];

        chunk->Append(member->Build(visitor, mod));

        rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

        if (hasSideEffects)
        {
            // claim register for member
            visitor->GetCompilationUnit()->GetInstructionStream().IncRegisterUsage();
            // get active register
            rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

            const int stackSizeAfter = visitor->GetCompilationUnit()->GetInstructionStream().GetStackSize();
            const int diff = stackSizeAfter - arrayStackLocation;
            Assert(diff == 1);

            { // load array from stack into a register
                auto instrLoadOffset = BytecodeUtil::Make<StorageOperation>();
                instrLoadOffset->GetBuilder().Load(rp).Local().ByOffset(diff);
                chunk->Append(std::move(instrLoadOffset));
            }

            { // send to the array
                auto instrMovArrayIdx = BytecodeUtil::Make<StorageOperation>();
                instrMovArrayIdx->GetBuilder().Store(rp - 1).Array(rp).ByIndex(uint32(index));
                chunk->Append(std::move(instrMovArrayIdx));
            }

            // unclaim register for member
            visitor->GetCompilationUnit()->GetInstructionStream().DecRegisterUsage();
            // get active register
            rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();
        }
        else
        {
            // send to the array
            auto instrMovArrayIdx = BytecodeUtil::Make<StorageOperation>();
            instrMovArrayIdx->GetBuilder().Store(rp).Array(rp - 1).ByIndex(uint32(index));
            chunk->Append(std::move(instrMovArrayIdx));
        }
    }

    if (hasSideEffects)
    {
        // move the array back into the first register we started with
        auto instrLoadOffset = BytecodeUtil::Make<StorageOperation>();
        instrLoadOffset->GetBuilder().Load(arrayReg).Local().ByOffset(visitor->GetCompilationUnit()->GetInstructionStream().GetStackSize() - arrayStackLocation);
        chunk->Append(std::move(instrLoadOffset));

        // pop array from stack
        chunk->Append(BytecodeUtil::Make<PopLocal>(1));
        visitor->GetCompilationUnit()->GetInstructionStream().DecStackSize();
    }
    else
    {
        // unclaim register for array
        visitor->GetCompilationUnit()->GetInstructionStream().DecRegisterUsage();
        // get active register
        rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();
    }

    return chunk;
}

void AstArrayExpression::Optimize(AstVisitor* visitor, Module* mod)
{
    for (auto& member : m_replacedMembers)
    {
        if (member != nullptr)
        {
            member->Optimize(visitor, mod);
        }
    }
}

RC<AstStatement> AstArrayExpression::Clone() const
{
    return CloneImpl();
}

Tribool AstArrayExpression::IsTrue() const
{
    return Tribool::True();
}

bool AstArrayExpression::MayHaveSideEffects() const
{
    bool sideEffects = false;

    for (const auto& member : m_replacedMembers)
    {
        Assert(member != nullptr);

        if (member->MayHaveSideEffects())
        {
            sideEffects = true;
            break;
        }
    }

    return sideEffects;
}

const SymbolType* AstArrayExpression::GetExprType() const
{
    if (m_exprType == nullptr)
    {
        return BuiltinTypes::s_errorType;
    }

    return m_exprType;
}

bool AstArrayExpression::IsMutable() const
{
    return true;
}

} // namespace Hyperion
