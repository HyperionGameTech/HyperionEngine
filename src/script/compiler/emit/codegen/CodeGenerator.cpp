#include <script/compiler/emit/codegen/CodeGenerator.hpp>
#include <core/HashCode.hpp>
#include <iostream>

namespace hyperion::compiler {

CodeGenerator::CodeGenerator(BuildParams& buildParams)
    : buildParams(buildParams)
{
}

void CodeGenerator::Visit(BytecodeChunk* chunk)
{
    BuildParams newParams;
    newParams.blockOffset = buildParams.blockOffset + m_ibs.GetPosition();
    newParams.labels = buildParams.labels;

    for (const LabelId labelId : chunk->labels)
    {
        newParams.labels.Insert({ labelId,
            LabelPosition(-1),
            HYP_NAME(LabelNameRemoved) });
    }

    CodeGenerator codeGenerator(newParams);

    for (auto& buildable : chunk->buildables)
    {
        codeGenerator.BuildableVisitor::Visit(buildable.Get());
    }

    // bake the chunk's byte stream
    // codeGenerator.GetInternalByteStream().Bake(newParams);

    const ByteBuffer& byteBuffer = codeGenerator.GetInternalByteStream().GetData();
    const Array<Fixup>& fixups = codeGenerator.GetInternalByteStream().GetFixups();

    const SizeType fixupOffset = m_ibs.GetPosition();

    // append bytes to this chunk's InternalByteStream
    m_ibs.Put(byteBuffer.Data(), byteBuffer.Size());

    // Copy fixups from the chunk's InternalByteStream to this one's
    for (const Fixup& fixup : fixups)
    {
        m_ibs.AddFixup(fixup.labelId, fixup.position + fixupOffset, fixup.offset);
    }

    buildParams.labels = newParams.labels;
}

void CodeGenerator::Bake()
{
    m_ibs.Bake(buildParams);
}

void CodeGenerator::Visit(LabelMarker* node)
{
    const LabelId labelId = node->id;

    const auto it = buildParams.labels.FindIf([labelId](const LabelInfo& labelInfo)
        {
            return labelInfo.labelId == labelId;
        });

    Assert(it != buildParams.labels.End(), "Label with ID %d not found", labelId);

    Assert(it->position == LabelPosition(-1), "Label position already set");

    it->position = LabelPosition(m_ibs.GetPosition() + buildParams.blockOffset);
}

void CodeGenerator::Visit(Jump* node)
{
    switch (node->jumpClass)
    {
    case Jump::JumpClass::JMP:
        m_ibs.Put(Instructions::JMP);
        break;
    case Jump::JumpClass::JE:
        m_ibs.Put(Instructions::JE);
        break;
    case Jump::JumpClass::JNE:
        m_ibs.Put(Instructions::JNE);
        break;
    case Jump::JumpClass::JG:
        m_ibs.Put(Instructions::JG);
        break;
    case Jump::JumpClass::JGE:
        m_ibs.Put(Instructions::JGE);
        break;
    }

    // tell the InternalByteStream to set this to the label position
    // when it is available
    m_ibs.AddFixup(node->labelId, buildParams.blockOffset);
}

void CodeGenerator::Visit(Comparison* node)
{
    switch (node->comparisonClass)
    {
    case Comparison::ComparisonClass::CMP:
        m_ibs.Put(Instructions::CMP);
        break;
    case Comparison::ComparisonClass::CMPZ:
        m_ibs.Put(Instructions::CMPZ);
        break;
    }

    m_ibs.Put(node->regLhs);

    if (node->comparisonClass == Comparison::ComparisonClass::CMP)
    {
        m_ibs.Put(node->regRhs);
    }
}

void CodeGenerator::Visit(FunctionCall* node)
{
    m_ibs.Put(Instructions::CALL);
    m_ibs.Put(node->reg);
    m_ibs.Put(node->nargs);
}

void CodeGenerator::Visit(Return* node)
{
    m_ibs.Put(Instructions::RET);
}

void CodeGenerator::Visit(StoreLocal* node)
{
    m_ibs.Put(Instructions::PUSH);
    m_ibs.Put(node->reg);
}

void CodeGenerator::Visit(PopLocal* node)
{
    if (node->amt > 1)
    {
        m_ibs.Put(Instructions::SUB_SP);

        uint16 asU16 = (uint16)node->amt;
        m_ibs.Put(reinterpret_cast<ubyte*>(&asU16), sizeof(asU16));
    }
    else
    {
        m_ibs.Put(Instructions::POP);
    }
}

void CodeGenerator::Visit(LoadRef* node)
{
    m_ibs.Put(Instructions::REF);
    m_ibs.Put(node->dst);
    m_ibs.Put(node->src);
}

void CodeGenerator::Visit(LoadDeref* node)
{
    m_ibs.Put(Instructions::DEREF);
    m_ibs.Put(node->dst);
    m_ibs.Put(node->src);
}

void CodeGenerator::Visit(ConstI32* node)
{
    m_ibs.Put(Instructions::LOAD_I32);
    m_ibs.Put(node->reg);
    m_ibs.Put(reinterpret_cast<ubyte*>(&node->value), sizeof(node->value));
}

void CodeGenerator::Visit(ConstI64* node)
{
    m_ibs.Put(Instructions::LOAD_I64);
    m_ibs.Put(node->reg);
    m_ibs.Put(reinterpret_cast<ubyte*>(&node->value), sizeof(node->value));
}

void CodeGenerator::Visit(ConstU32* node)
{
    m_ibs.Put(Instructions::LOAD_U32);
    m_ibs.Put(node->reg);
    m_ibs.Put(reinterpret_cast<ubyte*>(&node->value), sizeof(node->value));
}

void CodeGenerator::Visit(ConstU64* node)
{
    m_ibs.Put(Instructions::LOAD_U64);
    m_ibs.Put(node->reg);
    m_ibs.Put(reinterpret_cast<ubyte*>(&node->value), sizeof(node->value));
}

void CodeGenerator::Visit(ConstF32* node)
{
    m_ibs.Put(Instructions::LOAD_F32);
    m_ibs.Put(node->reg);
    m_ibs.Put(reinterpret_cast<ubyte*>(&node->value), sizeof(node->value));
}

void CodeGenerator::Visit(ConstF64* node)
{
    m_ibs.Put(Instructions::LOAD_F64);
    m_ibs.Put(node->reg);
    m_ibs.Put(reinterpret_cast<ubyte*>(&node->value), sizeof(node->value));
}

void CodeGenerator::Visit(ConstBool* node)
{
    m_ibs.Put(node->value
            ? Instructions::LOAD_TRUE
            : Instructions::LOAD_FALSE);
    m_ibs.Put(node->reg);
}

void CodeGenerator::Visit(ConstNull* node)
{
    m_ibs.Put(Instructions::LOAD_NULL);
    m_ibs.Put(node->reg);
}

void CodeGenerator::Visit(LoadClass* node)
{
    m_ibs.Put(Instructions::LOAD_CLASS);
    m_ibs.Put(node->reg);
    m_ibs.Put(reinterpret_cast<ubyte*>(&node->nameHash), sizeof(node->nameHash));
}

void CodeGenerator::Visit(BuildableTryCatch* node)
{
    m_ibs.Put(Instructions::BEGIN_TRY);
    m_ibs.AddFixup(node->catchLabelId, buildParams.blockOffset);
}

void CodeGenerator::Visit(BuildableFunction* node)
{
    // TODO: make it store and load statically
    m_ibs.Put(Instructions::LOAD_FUNC);
    m_ibs.Put(node->reg);
    m_ibs.AddFixup(node->labelId, buildParams.blockOffset);
    m_ibs.Put(node->nargs);
    m_ibs.Put(node->flags);
}

void CodeGenerator::Visit(BuildableType* node)
{
    // TODO: make it store and load statically
    m_ibs.Put(Instructions::NEW_CLASS);
    m_ibs.Put(node->reg);

    uint16_t nameLen = (uint16_t)node->name.Size();
    m_ibs.Put(reinterpret_cast<ubyte*>(&nameLen), sizeof(nameLen));
    m_ibs.Put(reinterpret_cast<ubyte*>(node->name.Data()), node->name.Size());

    uint16_t size = (uint16_t)node->members.Size();
    m_ibs.Put(reinterpret_cast<ubyte*>(&size), sizeof(size));

    for (const String& memberName : node->members)
    {
        uint16_t memberNameLen = (uint16_t)memberName.Size();
        m_ibs.Put(reinterpret_cast<ubyte*>(&memberNameLen), sizeof(memberNameLen));
        m_ibs.Put(reinterpret_cast<const ubyte*>(memberName.Data()), memberName.Size());
    }
}

void CodeGenerator::Visit(BuildableString* node)
{
    const uint32 len = uint32(node->value.Size());

    // TODO: make it store and load statically
    m_ibs.Put(Instructions::LOAD_STRING);
    m_ibs.Put(node->reg);
    m_ibs.Put(reinterpret_cast<const ubyte*>(&len), sizeof(len));
    m_ibs.Put(reinterpret_cast<const ubyte*>(node->value.Data()), node->value.Size());
}

void CodeGenerator::Visit(StorageOperation* node)
{
    switch (node->method)
    {
    case Methods::LOCAL:
        switch (node->strategy)
        {
        case Strategies::BY_OFFSET:
            switch (node->operation)
            {
            case Operations::LOAD:
                m_ibs.Put(node->op.isRef ? Instructions::LOAD_OFFSET_REF : Instructions::LOAD_OFFSET);
                m_ibs.Put(reinterpret_cast<ubyte*>(&node->op.a.reg), sizeof(node->op.a.reg));
                m_ibs.Put(reinterpret_cast<ubyte*>(&node->op.b.offset), sizeof(node->op.b.offset));

                break;
            case Operations::STORE:
                m_ibs.Put(Instructions::MOV_OFFSET);
                m_ibs.Put(reinterpret_cast<ubyte*>(&node->op.b.offset), sizeof(node->op.b.offset));
                m_ibs.Put(reinterpret_cast<ubyte*>(&node->op.a.reg), sizeof(node->op.a.reg));

                break;
            }

            break;

        case Strategies::BY_INDEX:
            switch (node->operation)
            {
            case Operations::LOAD:
                m_ibs.Put(node->op.isRef ? Instructions::LOAD_INDEX_REF : Instructions::LOAD_INDEX);
                m_ibs.Put(reinterpret_cast<ubyte*>(&node->op.a.reg), sizeof(node->op.a.reg));
                m_ibs.Put(reinterpret_cast<ubyte*>(&node->op.b.index), sizeof(node->op.b.index));

                break;
            case Operations::STORE:
                m_ibs.Put(Instructions::MOV_INDEX);
                m_ibs.Put(reinterpret_cast<ubyte*>(&node->op.b.index), sizeof(node->op.b.index));
                m_ibs.Put(reinterpret_cast<ubyte*>(&node->op.a.reg), sizeof(node->op.a.reg));

                break;
            }

            break;

        case Strategies::BY_HASH:
            Assert(false, "Not implemented");

            break;
        }

        break;

    case Methods::STATIC:
        switch (node->strategy)
        {
        case Strategies::BY_OFFSET:
            Assert(false, "Not implemented");

            break;

        case Strategies::BY_INDEX:
            switch (node->operation)
            {
            case Operations::LOAD:
                m_ibs.Put(Instructions::LOAD_STATIC);
                m_ibs.Put(reinterpret_cast<ubyte*>(&node->op.a.reg), sizeof(node->op.a.reg));
                m_ibs.Put(reinterpret_cast<ubyte*>(&node->op.b.index), sizeof(node->op.b.index));

                break;
            case Operations::STORE:
                m_ibs.Put(Instructions::MOV_STATIC);
                m_ibs.Put(reinterpret_cast<ubyte*>(&node->op.b.index), sizeof(node->op.b.index));
                m_ibs.Put(reinterpret_cast<ubyte*>(&node->op.a.reg), sizeof(node->op.a.reg));

                break;
            }

            break;

        case Strategies::BY_HASH:
            Assert(false, "Not implemented");

            break;
        }

        break;

    case Methods::ARRAY:
        switch (node->strategy)
        {
        case Strategies::BY_OFFSET:
            Assert(false, "Not implemented");

            break;

        case Strategies::BY_INDEX:
            switch (node->operation)
            {
            case Operations::LOAD:
                m_ibs.Put(Instructions::LOAD_ARRAYIDX);
                m_ibs.Put(reinterpret_cast<ubyte*>(&node->op.a.reg), sizeof(node->op.a.reg));
                m_ibs.Put(reinterpret_cast<ubyte*>(&node->op.b.objectData.reg), sizeof(node->op.b.objectData.reg));
                m_ibs.Put(reinterpret_cast<ubyte*>(&node->op.b.objectData.member.index), sizeof(node->op.b.objectData.member.index));

                break;
            case Operations::STORE:
                m_ibs.Put(Instructions::MOV_ARRAYIDX);
                m_ibs.Put(reinterpret_cast<ubyte*>(&node->op.b.objectData.reg), sizeof(node->op.b.objectData.reg));
                m_ibs.Put(reinterpret_cast<ubyte*>(&node->op.b.objectData.member.index), sizeof(node->op.b.objectData.member.index));
                m_ibs.Put(reinterpret_cast<ubyte*>(&node->op.a.reg), sizeof(node->op.a.reg));

                break;
            }

            break;

        case Strategies::BY_HASH:
            Assert(false, "Not implemented");

            break;
        }

        break;

    case Methods::MEMBER:
        switch (node->strategy)
        {
        case Strategies::BY_OFFSET:
            Assert(false, "Not implemented");

            break;

        case Strategies::BY_INDEX:
            switch (node->operation)
            {
            case Operations::LOAD:
                m_ibs.Put(Instructions::LOAD_MEM);
                m_ibs.Put(reinterpret_cast<ubyte*>(&node->op.a.reg), sizeof(node->op.a.reg));
                m_ibs.Put(reinterpret_cast<ubyte*>(&node->op.b.objectData.reg), sizeof(node->op.b.objectData.reg));
                m_ibs.Put(reinterpret_cast<ubyte*>(&node->op.b.objectData.member.index), sizeof(node->op.b.objectData.member.index));

                break;
            case Operations::STORE:
                m_ibs.Put(Instructions::MOV_MEM);
                m_ibs.Put(reinterpret_cast<ubyte*>(&node->op.b.objectData.reg), sizeof(node->op.b.objectData.reg));
                m_ibs.Put(reinterpret_cast<ubyte*>(&node->op.b.objectData.member.index), sizeof(node->op.b.objectData.member.index));
                m_ibs.Put(reinterpret_cast<ubyte*>(&node->op.a.reg), sizeof(node->op.a.reg));

                break;
            }

            break;

        case Strategies::BY_HASH:
            switch (node->operation)
            {
            case Operations::LOAD:
                m_ibs.Put(Instructions::LOAD_MEM_HASH);
                m_ibs.Put(reinterpret_cast<ubyte*>(&node->op.a.reg), sizeof(node->op.a.reg));
                m_ibs.Put(reinterpret_cast<ubyte*>(&node->op.b.objectData.reg), sizeof(node->op.b.objectData.reg));
                m_ibs.Put(reinterpret_cast<ubyte*>(&node->op.b.objectData.member.hash), sizeof(node->op.b.objectData.member.hash));

                break;
            case Operations::STORE:
                m_ibs.Put(Instructions::MOV_MEM_HASH);
                m_ibs.Put(reinterpret_cast<ubyte*>(&node->op.b.objectData.reg), sizeof(node->op.b.objectData.reg));
                m_ibs.Put(reinterpret_cast<ubyte*>(&node->op.b.objectData.member.hash), sizeof(node->op.b.objectData.member.hash));
                m_ibs.Put(reinterpret_cast<ubyte*>(&node->op.a.reg), sizeof(node->op.a.reg));

                break;
            }

            break;
        }

        break;
    }
}

void CodeGenerator::Visit(Comment* node)
{
    const SizeType size = node->value.Size();
    uint32 len = uint32(size);

    m_ibs.Put(Instructions::REM);
    m_ibs.Put(reinterpret_cast<ubyte*>(&len), sizeof(len));
    m_ibs.Put(reinterpret_cast<ubyte*>(node->value.Data()), size);
}

void CodeGenerator::Visit(SymbolExport* node)
{
    const HashCode::ValueType hash = HashCode::GetHashCode(node->name.Data()).Value();

    static_assert(sizeof(hash) == 8, "Expected hash to be 64 bits");

    m_ibs.Put(Instructions::EXPORT);
    m_ibs.Put(reinterpret_cast<ubyte*>(&node->reg), sizeof(node->reg));
    m_ibs.Put(reinterpret_cast<const ubyte*>(&hash), sizeof(hash));
}

void CodeGenerator::Visit(CastOperation* node)
{
    const uint8 castInstruction = uint8(Instructions::CAST_U8) + uint8(node->type);
    Assert(
        castInstruction >= Instructions::CAST_U8 && castInstruction <= uint8(Instructions::CAST_DYNAMIC),
        "Invalid cast type");

    m_ibs.Put(castInstruction);
    m_ibs.Put(reinterpret_cast<ubyte*>(&node->regDst), sizeof(node->regDst));
    m_ibs.Put(reinterpret_cast<ubyte*>(&node->regSrc), sizeof(node->regSrc));
}

void CodeGenerator::Visit(RawOperation<>* node)
{
    m_ibs.Put(node->opcode);

    if (node->data.Any())
    {
        m_ibs.Put(reinterpret_cast<ubyte*>(&node->data[0]), node->data.Size());
    }
}

} // namespace hyperion::compiler
