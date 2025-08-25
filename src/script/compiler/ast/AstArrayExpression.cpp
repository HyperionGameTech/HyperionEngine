#include <script/compiler/ast/AstArrayExpression.hpp>
#include <script/compiler/ast/AstTypeObject.hpp>
#include <script/compiler/ast/AstVariable.hpp>
#include <script/compiler/ast/AstArgument.hpp>
#include <script/compiler/ast/AstMember.hpp>
#include <script/compiler/ast/AstCallExpression.hpp>
#include <script/compiler/ast/AstAsExpression.hpp>
#include <script/compiler/ast/AstPrototypeSpecification.hpp>
#include <script/compiler/ast/AstTemplateInstantiation.hpp>
#include <script/compiler/ast/AstTypeRef.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Module.hpp>
#include <script/compiler/Compiler.hpp>
#include <script/compiler/Configuration.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>
#include <script/compiler/emit/StorageOperation.hpp>

#include <script/Instructions.hpp>
#include <core/debug/Debug.hpp>

#include <core/containers/FlatSet.hpp>

#include <core/Types.hpp>

namespace hyperion::compiler {

AstArrayExpression::AstArrayExpression(
    const Array<RC<AstExpression>>& members,
    const SourceLocation& location)
    : AstExpression(location, ACCESS_MODE_LOAD),
      m_members(members),
      m_heldType(BuiltinTypes::ANY)
{
}

void AstArrayExpression::Visit(AstVisitor* visitor, Module* mod)
{
    m_exprType = BuiltinTypes::UNDEFINED;

    m_replacedMembers.Reserve(m_members.Size());

    FlatSet<SymbolTypeRef> heldTypes;

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
            heldTypes.Insert(BuiltinTypes::ANY);
        }

        m_replacedMembers.PushBack(CloneAstNode(member));
    }

    for (const auto& it : heldTypes)
    {
        Assert(it != nullptr);

        if (m_heldType->IsOrHasBase(*BuiltinTypes::UNDEFINED))
        {
            // `Undefined` invalidates the array type
            break;
        }

        if (m_heldType->IsAnyType() || m_heldType->IsPlaceholderType())
        {
            // take first item found that is not `Any`
            m_heldType = it;
        }
        else if (m_heldType->TypeCompatible(*it, false))
        { // allow non-strict numbers because we can do a cast
            m_heldType = SymbolType::TypePromotion(m_heldType, it);
        }
        else
        {
            // more than one differing type, use Any.
            m_heldType = BuiltinTypes::ANY;
            break;
        }
    }

    for (SizeType index = 0; index < m_replacedMembers.Size(); index++)
    {
        auto& replacedMember = m_replacedMembers[index];
        Assert(replacedMember != nullptr);

        auto& member = m_members[index];
        Assert(member != nullptr);

        if (SymbolTypeRef exprType = member->GetExprType())
        {
            if (!exprType->TypeEqual(*m_heldType))
            {
                // replace with a cast to the held type
                replacedMember.Reset(new AstAsExpression(
                    replacedMember,
                    RC<AstPrototypeSpecification>(new AstPrototypeSpecification(
                        RC<AstTypeRef>(new AstTypeRef(
                            m_heldType,
                            member->GetLocation())),
                        member->GetLocation())),
                    member->GetLocation()));
            }
        }

        replacedMember->Visit(visitor, mod);
    }

    // Call Array<T>.from([ ... ])

    // m_arrayFromCall.Reset(new AstCallExpression(
    //     RC<AstMember>(new AstMember(
    //         "from",
    //         RC<AstTemplateInstantiation>(new AstTemplateInstantiation(
    //             RC<AstVariable>(new AstVariable(
    //                 "Array",
    //                 m_location
    //             )),
    //             {
    //                 RC<AstArgument>(new AstArgument(
    //                     RC<AstTypeRef>(new AstTypeRef(
    //                         m_heldType,
    //                         m_location
    //                     )),
    //                     false,
    //                     false,
    //                     false,
    //                     false,
    //                     "T",
    //                     m_location
    //                 ))
    //             },
    //             m_location
    //         )),
    //         m_location
    //     )),
    //     m_location
    // ));

    m_arrayTypeExpr.Reset(new AstPrototypeSpecification(
        RC<AstTemplateInstantiation>(new AstTemplateInstantiation(
            RC<AstVariable>(new AstVariable(
                "Array",
                m_location)),
            { RC<AstArgument>(new AstArgument(
                RC<AstTypeRef>(new AstTypeRef(
                    m_heldType,
                    m_location)),
                false,
                false,
                false,
                false,
                "T",
                m_location)) },
            m_location)),
        m_location));

    m_arrayTypeExpr->Visit(visitor, mod);

    auto* arrayTypeExprValueOf = m_arrayTypeExpr->GetDeepValueOf();
    Assert(arrayTypeExprValueOf != nullptr);

    SymbolTypeRef arrayType = arrayTypeExprValueOf->GetHeldType();

    if (arrayType == nullptr)
    {
        // error already reported
        return;
    }

    arrayType = arrayType->GetUnaliased();

    // @TODO: Cache generic instance types
    m_exprType = arrayType;
}

UniquePtr<Buildable> AstArrayExpression::Build(AstVisitor* visitor, Module* mod)
{
    UniquePtr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    Assert(m_arrayTypeExpr != nullptr);
    chunk->Append(m_arrayTypeExpr->Build(visitor, mod));

    // get active register
    uint8 rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

    { // keep type obj in memory so we can do Array<T>.from(...), so push it to the stack
        auto instrPush = BytecodeUtil::Make<RawOperation<>>();
        instrPush->opcode = PUSH;
        instrPush->Accept<uint8>(rp);
        chunk->Append(std::move(instrPush));
    }

    int classStackLocation = visitor->GetCompilationUnit()->GetInstructionStream().GetStackSize();
    visitor->GetCompilationUnit()->GetInstructionStream().IncStackSize();

    const bool hasSideEffects = true; // MayHaveSideEffects();
    const uint32 arraySize = uint32(m_members.Size());

    { // add NEW_ARRAY instruction
        auto instrNewArray = BytecodeUtil::Make<RawOperation<>>();
        instrNewArray->opcode = NEW_ARRAY;
        instrNewArray->Accept<uint8>(rp);
        instrNewArray->Accept<uint32>(arraySize);
        chunk->Append(std::move(instrNewArray));
    }

    const uint8 arrayReg = rp;

    // move array to stack
    {
        auto instrPush = BytecodeUtil::Make<RawOperation<>>();
        instrPush->opcode = PUSH;
        instrPush->Accept<uint8>(rp);
        chunk->Append(std::move(instrPush));
    }

    int arrayStackLocation = visitor->GetCompilationUnit()->GetInstructionStream().GetStackSize();
    visitor->GetCompilationUnit()->GetInstructionStream().IncStackSize();

    if (!hasSideEffects)
    {
        // claim register for array
        visitor->GetCompilationUnit()->GetInstructionStream().IncRegisterUsage();

        // get active register
        rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();
    }

    // assign all array items
    for (SizeType index = 0; index < m_replacedMembers.Size(); index++)
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

            { // load array from stack back into register
                auto instrLoadOffset = BytecodeUtil::Make<RawOperation<>>();
                instrLoadOffset->opcode = LOAD_OFFSET;
                instrLoadOffset->Accept<uint8>(rp);
                instrLoadOffset->Accept<uint16>(uint16(diff));
                chunk->Append(std::move(instrLoadOffset));
            }

            { // send to the array
                auto instrMovArrayIdx = BytecodeUtil::Make<RawOperation<>>();
                instrMovArrayIdx->opcode = MOV_ARRAYIDX;
                instrMovArrayIdx->Accept<uint8>(rp);
                instrMovArrayIdx->Accept<uint32>(uint32(index));
                instrMovArrayIdx->Accept<uint8>(rp - 1);
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
            auto instrMovArrayIdx = BytecodeUtil::Make<RawOperation<>>();
            instrMovArrayIdx->opcode = MOV_ARRAYIDX;
            instrMovArrayIdx->Accept<uint8>(rp - 1);
            instrMovArrayIdx->Accept<uint32>(uint32(index));
            instrMovArrayIdx->Accept<uint8>(rp);
            chunk->Append(std::move(instrMovArrayIdx));
        }
    }

    if (!hasSideEffects)
    {
        // unclaim register for array
        visitor->GetCompilationUnit()->GetInstructionStream().DecRegisterUsage();
        // get active register
        rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();
    }

    { // load array type expr from stack back into register
        auto instrLoadOffset = BytecodeUtil::Make<StorageOperation>();
        instrLoadOffset->GetBuilder()
            .Load(rp)
            .Local()
            .ByOffset(visitor->GetCompilationUnit()->GetInstructionStream().GetStackSize() - classStackLocation);

        chunk->Append(std::move(instrLoadOffset));
    }

    { // load member `from` from array type expr
        constexpr HashCode::ValueType fromHash = HashCode::GetHashCode("from").Value();

        chunk->Append(Compiler::LoadMemberFromHash(visitor, mod, fromHash));
    }

    // Here array type and array should be the 2 items on the stack
    // so we call `from`, and the array type class will be the first arg, and the array will be the second arg

    { // call the `from` method
        chunk->Append(Compiler::BuildCall(
            visitor,
            mod,
            nullptr, // no target -- handled above
            uint8(2) // self, array
            ));
    }

    // decrement stack size for array type expr
    chunk->Append(BytecodeUtil::Make<PopLocal>(2));

    // pop array and type from stack
    for (uint32 i = 0; i < 2; i++)
    {
        visitor->GetCompilationUnit()->GetInstructionStream().DecStackSize();
    }

    return chunk;
}

void AstArrayExpression::Optimize(AstVisitor* visitor, Module* mod)
{
    if (m_arrayTypeExpr != nullptr)
    {
        m_arrayTypeExpr->Optimize(visitor, mod);
    }

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

SymbolTypeRef AstArrayExpression::GetExprType() const
{
    if (m_exprType == nullptr)
    {
        return BuiltinTypes::UNDEFINED;
    }

    return m_exprType;
}

} // namespace hyperion::compiler
