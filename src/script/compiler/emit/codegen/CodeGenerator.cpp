#include <script/compiler/emit/codegen/CodeGenerator.hpp>

#include <core/HashCode.hpp>
#include <core/object/HypField.hpp>
#include <core/object/HypMethod.hpp>
#include <core/object/HypConstant.hpp>
#include <core/object/HypProperty.hpp>
#include <core/object/HypClassAttribute.hpp>

#include <iostream>

namespace hyperion {

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
    // Use new unified instruction format
    const uint8 subcmd = MAKE_LOAD_SUBCMD(DTYPE_OBJECT, 1, LSRC_REGISTER);

    m_ibs.Put(Instructions::LOAD_UNIFIED);
    m_ibs.Put(subcmd);
    m_ibs.Put(node->dst);
    m_ibs.Put(node->src);
}

void CodeGenerator::Visit(LoadDeref* node)
{
    // Use new unified instruction format (deref is regular load from register)
    const uint8 subcmd = MAKE_LOAD_SUBCMD(DTYPE_OBJECT, 0, LSRC_REGISTER);

    m_ibs.Put(Instructions::LOAD_UNIFIED);
    m_ibs.Put(subcmd);
    m_ibs.Put(node->dst);
    m_ibs.Put(node->src);
}

void CodeGenerator::Visit(ConstI32* node)
{
    // Use new unified instruction format
    const uint8 subcmd = MAKE_LOAD_SUBCMD(DTYPE_I32, 0, LSRC_IMMEDIATE);

    m_ibs.Put(Instructions::LOAD_UNIFIED);
    m_ibs.Put(subcmd);
    m_ibs.Put(node->reg);
    m_ibs.Put(reinterpret_cast<ubyte*>(&node->value), sizeof(node->value));
}

void CodeGenerator::Visit(ConstI64* node)
{
    // Use new unified instruction format
    const uint8 subcmd = MAKE_LOAD_SUBCMD(DTYPE_I64, 0, LSRC_IMMEDIATE);

    m_ibs.Put(Instructions::LOAD_UNIFIED);
    m_ibs.Put(subcmd);
    m_ibs.Put(node->reg);
    m_ibs.Put(reinterpret_cast<ubyte*>(&node->value), sizeof(node->value));
}

void CodeGenerator::Visit(ConstU32* node)
{
    // Use new unified instruction format
    const uint8 subcmd = MAKE_LOAD_SUBCMD(DTYPE_U32, 0, LSRC_IMMEDIATE);

    m_ibs.Put(Instructions::LOAD_UNIFIED);
    m_ibs.Put(subcmd);
    m_ibs.Put(node->reg);
    m_ibs.Put(reinterpret_cast<ubyte*>(&node->value), sizeof(node->value));
}

void CodeGenerator::Visit(ConstU64* node)
{
    // Use new unified instruction format
    const uint8 subcmd = MAKE_LOAD_SUBCMD(DTYPE_U64, 0, LSRC_IMMEDIATE);

    m_ibs.Put(Instructions::LOAD_UNIFIED);
    m_ibs.Put(subcmd);
    m_ibs.Put(node->reg);
    m_ibs.Put(reinterpret_cast<ubyte*>(&node->value), sizeof(node->value));
}

void CodeGenerator::Visit(ConstF32* node)
{
    // Use new unified instruction format
    const uint8 subcmd = MAKE_LOAD_SUBCMD(DTYPE_F32, 0, LSRC_IMMEDIATE);

    m_ibs.Put(Instructions::LOAD_UNIFIED);
    m_ibs.Put(subcmd);
    m_ibs.Put(node->reg);
    m_ibs.Put(reinterpret_cast<ubyte*>(&node->value), sizeof(node->value));
}

void CodeGenerator::Visit(ConstF64* node)
{
    // Use new unified instruction format
    const uint8 subcmd = MAKE_LOAD_SUBCMD(DTYPE_F64, 0, LSRC_IMMEDIATE);

    m_ibs.Put(Instructions::LOAD_UNIFIED);
    m_ibs.Put(subcmd);
    m_ibs.Put(node->reg);
    m_ibs.Put(reinterpret_cast<ubyte*>(&node->value), sizeof(node->value));
}

void CodeGenerator::Visit(ConstBool* node)
{
    // Use new unified instruction format
    const uint8 subcmd = MAKE_LOAD_SUBCMD(DTYPE_BOOL, 0, LSRC_IMMEDIATE);

    m_ibs.Put(Instructions::LOAD_UNIFIED);
    m_ibs.Put(subcmd);
    m_ibs.Put(node->reg);

    uint8 boolValue = node->value ? 1 : 0;
    m_ibs.Put(boolValue);
}

void CodeGenerator::Visit(ConstNull* node)
{
    // Use new unified instruction format
    const uint8 subcmd = MAKE_LOAD_SUBCMD(DTYPE_OBJECT, 0, LSRC_IMMEDIATE);

    m_ibs.Put(Instructions::LOAD_UNIFIED);
    m_ibs.Put(subcmd);
    m_ibs.Put(node->reg);
    // No additional data needed for null
}

void CodeGenerator::Visit(LoadClass* node)
{
    m_ibs.Put(Instructions::LOAD_CLASS);
    m_ibs.Put(node->reg);

    const uint64 nameHashValue = (uint64)node->className.hashCode;
    m_ibs.Put(reinterpret_cast<const ubyte*>(&nameHashValue), sizeof(nameHashValue));
}

void CodeGenerator::Visit(TryCatchInfo* node)
{
    m_ibs.Put(Instructions::BEGIN_TRY);
    m_ibs.AddFixup(node->catchLabelId, buildParams.blockOffset);
}

void CodeGenerator::Visit(ScriptFunction* node)
{
    // TODO: make it store and load statically
    m_ibs.Put(Instructions::LOAD_FUNC);
    m_ibs.Put(node->reg);
    m_ibs.AddFixup(node->labelId, buildParams.blockOffset);
    m_ibs.Put(node->nargs);
    m_ibs.Put(node->flags);
}

void CodeGenerator::Visit(ClassTable* node)
{
    m_ibs.Put(Instructions::BEGIN_CLASS);
    m_ibs.Put(node->reg);

    uint16 nameLen = (uint16)node->name.Size();
    m_ibs.Put(reinterpret_cast<ubyte*>(&nameLen), sizeof(nameLen));
    m_ibs.Put(reinterpret_cast<ubyte*>(node->name.Data()), node->name.Size());

    // write type id
    TypeId::ValueType typeIdValue = TypeId::ForManagedType(*node->name).Value();
    m_ibs.Put(reinterpret_cast<ubyte*>(&typeIdValue), sizeof(typeIdValue));

    uint8 flags = (uint8)node->flags;
    m_ibs.Put(flags);

    // begin members. write member type (uint8) and count (uint16)
    auto writeMembers = [&](const auto& members, HypMemberType memberType)
    {
        if (members.Any())
        {
            m_ibs.Put(uint8(memberType));

            uint16 size = (uint16)members.Size();
            m_ibs.Put(reinterpret_cast<ubyte*>(&size), sizeof(size));

            for (const ClassTable::MemberInfo& member : members)
            {
                const String& nameStr = member.name;

                uint16 nameLen = (uint16)nameStr.Size();
                Assert(nameLen > 0, "Invalid member name length");

                m_ibs.Put(reinterpret_cast<ubyte*>(&nameLen), sizeof(nameLen));
                m_ibs.Put(reinterpret_cast<const ubyte*>(nameStr.Data()), nameLen);

                // write attrs
                uint16 numAttrs = (uint16)member.attrs.Size();
                m_ibs.Put(reinterpret_cast<ubyte*>(&numAttrs), sizeof(numAttrs));

                for (const HypClassAttribute& attr : member.attrs)
                {
                    const char* attrStr = attr.name.LookupString();
                    Assert(attrStr != nullptr, "Invalid attribute name");

                    uint16 attrLen = (uint16)String(attrStr).Size();
                    Assert(attrLen > 0, "Invalid attribute name length");

                    m_ibs.Put(reinterpret_cast<ubyte*>(&attrLen), sizeof(attrLen));
                    m_ibs.Put(reinterpret_cast<const ubyte*>(attrStr), attrLen);

                    const HypClassAttributeValue& value = attr.value;
                    uint8 attrType = uint8(value.GetType());
                    m_ibs.Put(attrType);

                    switch (value.GetType())
                    {
                    case HypClassAttributeType::STRING:
                    {
                        const String& str = value.GetString();
                        uint32 strLen = (uint32)str.Size();
                        m_ibs.Put(reinterpret_cast<ubyte*>(&strLen), sizeof(strLen));
                        m_ibs.Put(reinterpret_cast<const ubyte*>(str.Data()), str.Size());

                        break;
                    }
                    case HypClassAttributeType::INT:
                    {
                        int intValue = value.GetInt();
                        m_ibs.Put(reinterpret_cast<ubyte*>(&intValue), sizeof(intValue));

                        break;
                    }
                    case HypClassAttributeType::BOOLEAN:
                    {
                        bool boolValue = value.GetBool();
                        m_ibs.Put(boolValue ? 1 : 0);
                        break;
                    }
                    case HypClassAttributeType::NONE: // fallthrough
                    default:
                        HYP_UNREACHABLE();
                        break;
                    }
                }

                // write type id
                TypeId::ValueType typeIdValue = member.typeId.Value();
                m_ibs.Put(reinterpret_cast<ubyte*>(&typeIdValue), sizeof(typeIdValue));

                // field writes target type id, offset, size
                if (memberType == HypMemberType::TYPE_FIELD)
                {
                    const ClassTable::FieldInfo& fieldInfo = static_cast<const ClassTable::FieldInfo&>(member);

                    TypeId::ValueType targetTypeIdValue = fieldInfo.targetTypeId.Value();
                    m_ibs.Put(reinterpret_cast<ubyte*>(&targetTypeIdValue), sizeof(targetTypeIdValue));

                    uint32 offset = fieldInfo.offset;
                    m_ibs.Put(reinterpret_cast<ubyte*>(&offset), sizeof(offset));

                    uint32 size = fieldInfo.size;
                    m_ibs.Put(reinterpret_cast<ubyte*>(&size), sizeof(size));
                }
                else if (memberType == HypMemberType::TYPE_METHOD)
                {
                    const ClassTable::MethodInfo& methodInfo = static_cast<const ClassTable::MethodInfo&>(member);

                    TypeId::ValueType targetTypeIdValue = methodInfo.targetTypeId.Value();
                    m_ibs.Put(reinterpret_cast<ubyte*>(&targetTypeIdValue), sizeof(targetTypeIdValue));

                    uint8 flags = (uint8)methodInfo.flags;
                    m_ibs.Put(flags);

                    // put stack offset
                    Assert(methodInfo.stackOffset != UINT16_MAX, "Method stack offset not set");
                    m_ibs.Put(reinterpret_cast<const ubyte*>(&methodInfo.stackOffset), sizeof(methodInfo.stackOffset));
                }
                else
                {
                    HYP_NOT_IMPLEMENTED();
                }
            }
        }
    };

    writeMembers(node->fields, HypMemberType::TYPE_FIELD);
    writeMembers(node->methods, HypMemberType::TYPE_METHOD);

    m_ibs.Put(Instructions::END_CLASS);
}

void CodeGenerator::Visit(ConstString* node)
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
            {
                // Use unified load instruction
                const uint8 subcmd = MAKE_LOAD_SUBCMD(
                    DTYPE_OBJECT,
                    node->op.isRef,
                    LSRC_OFFSET);

                m_ibs.Put(Instructions::LOAD_UNIFIED);
                m_ibs.Put(subcmd);
                m_ibs.Put(reinterpret_cast<ubyte*>(&node->op.a.reg), sizeof(node->op.a.reg));
                m_ibs.Put(reinterpret_cast<ubyte*>(&node->op.b.offset), sizeof(node->op.b.offset));
            }
            break;

            case Operations::STORE:
            {
                // Use unified move instruction
                const uint8 subcmd = MAKE_MOV_SUBCMD(MDST_OFFSET, MSRC_REGISTER);

                m_ibs.Put(Instructions::MOV_UNIFIED);
                m_ibs.Put(subcmd);
                m_ibs.Put(reinterpret_cast<ubyte*>(&node->op.b.offset), sizeof(node->op.b.offset));
                m_ibs.Put(reinterpret_cast<ubyte*>(&node->op.a.reg), sizeof(node->op.a.reg));
            }
            break;
            }
            break;

        case Strategies::BY_INDEX:
            switch (node->operation)
            {
            case Operations::LOAD:
            {
                // Use unified load instruction
                const uint8 subcmd = MAKE_LOAD_SUBCMD(
                    DTYPE_OBJECT,
                    node->op.isRef,
                    LSRC_INDEX);

                m_ibs.Put(Instructions::LOAD_UNIFIED);
                m_ibs.Put(subcmd);
                m_ibs.Put(reinterpret_cast<ubyte*>(&node->op.a.reg), sizeof(node->op.a.reg));
                m_ibs.Put(reinterpret_cast<ubyte*>(&node->op.b.index), sizeof(node->op.b.index));
            }
            break;

            case Operations::STORE:
            {
                // Use unified move instruction
                const uint8 subcmd = MAKE_MOV_SUBCMD(MDST_INDEX, MSRC_REGISTER);

                m_ibs.Put(Instructions::MOV_UNIFIED);
                m_ibs.Put(subcmd);
                m_ibs.Put(reinterpret_cast<ubyte*>(&node->op.b.index), sizeof(node->op.b.index));
                m_ibs.Put(reinterpret_cast<ubyte*>(&node->op.a.reg), sizeof(node->op.a.reg));
            }
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
            {
                // Use unified load instruction
                const uint8 subcmd = MAKE_LOAD_SUBCMD(DTYPE_OBJECT, 0, LSRC_STATIC);

                m_ibs.Put(Instructions::LOAD_UNIFIED);
                m_ibs.Put(subcmd);
                m_ibs.Put(reinterpret_cast<ubyte*>(&node->op.a.reg), sizeof(node->op.a.reg));
                m_ibs.Put(reinterpret_cast<ubyte*>(&node->op.b.index), sizeof(node->op.b.index));
            }
            break;

            case Operations::STORE:
            {
                // Use unified move instruction
                const uint8 subcmd = MAKE_MOV_SUBCMD(MDST_STATIC, MSRC_REGISTER);

                m_ibs.Put(Instructions::MOV_UNIFIED);
                m_ibs.Put(subcmd);
                m_ibs.Put(reinterpret_cast<ubyte*>(&node->op.b.index), sizeof(node->op.b.index));
                m_ibs.Put(reinterpret_cast<ubyte*>(&node->op.a.reg), sizeof(node->op.a.reg));
            }
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
            {
                // Use unified load instruction
                const uint8 subcmd = MAKE_LOAD_SUBCMD(DTYPE_OBJECT, 0, LSRC_ARRAYIDX);

                m_ibs.Put(Instructions::LOAD_UNIFIED);
                m_ibs.Put(subcmd);
                m_ibs.Put(reinterpret_cast<ubyte*>(&node->op.a.reg), sizeof(node->op.a.reg));
                m_ibs.Put(reinterpret_cast<ubyte*>(&node->op.b.objectData.reg), sizeof(node->op.b.objectData.reg));
                m_ibs.Put(reinterpret_cast<ubyte*>(&node->op.b.objectData.member.index), sizeof(node->op.b.objectData.member.index));
            }
            break;

            case Operations::STORE:
            {
                // Use unified move instruction with array store flag
                const uint8 subcmd = MAKE_MOV_ARRAYSTORE_SUBCMD(MDST_REGISTER, MSRC_REGISTER);

                m_ibs.Put(Instructions::MOV_UNIFIED);
                m_ibs.Put(subcmd);
                m_ibs.Put(reinterpret_cast<ubyte*>(&node->op.b.objectData.reg), sizeof(node->op.b.objectData.reg));
                m_ibs.Put(reinterpret_cast<ubyte*>(&node->op.b.objectData.member.index), sizeof(node->op.b.objectData.member.index));
                m_ibs.Put(reinterpret_cast<ubyte*>(&node->op.a.reg), sizeof(node->op.a.reg));
            }
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
                Assert(false, "Not implemented");
                break;
            case Operations::STORE:
                Assert(false, "Not implemented");
                break;
            }
            break;

        case Strategies::BY_HASH:
            switch (node->operation)
            {
            case Operations::LOAD:
            {
                // Use unified load instruction for member access
                const uint8 subcmd = MAKE_LOAD_SUBCMD(DTYPE_OBJECT, 0, LSRC_MEMBER);

                m_ibs.Put(Instructions::LOAD_UNIFIED);
                m_ibs.Put(subcmd);
                m_ibs.Put(reinterpret_cast<ubyte*>(&node->op.a.reg), sizeof(node->op.a.reg));
                m_ibs.Put(reinterpret_cast<ubyte*>(&node->op.b.objectData.reg), sizeof(node->op.b.objectData.reg));
                m_ibs.Put(reinterpret_cast<ubyte*>(&node->op.b.objectData.member.hash), sizeof(node->op.b.objectData.member.hash));
            }
            break;

            case Operations::STORE:
            {
                // Use unified move instruction for member assignment
                const uint8 subcmd = MAKE_MOV_SUBCMD(MDST_REGISTER, MSRC_MEMBER);

                m_ibs.Put(Instructions::MOV_UNIFIED);
                m_ibs.Put(subcmd);
                m_ibs.Put(reinterpret_cast<ubyte*>(&node->op.b.objectData.reg), sizeof(node->op.b.objectData.reg));
                m_ibs.Put(reinterpret_cast<ubyte*>(&node->op.b.objectData.member.hash), sizeof(node->op.b.objectData.member.hash));
                m_ibs.Put(reinterpret_cast<ubyte*>(&node->op.a.reg), sizeof(node->op.a.reg));
            }
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
    // Use new unified cast instruction
    const uint8 subcmd = MAKE_CAST_SUBCMD(static_cast<CastType>(node->type));

    m_ibs.Put(Instructions::CAST_UNIFIED);
    m_ibs.Put(subcmd);
    m_ibs.Put(node->regDst);
    m_ibs.Put(node->regSrc);
}

void CodeGenerator::Visit(RawOperation<>* node)
{
    m_ibs.Put(node->opcode);

    if (node->data.Any())
    {
        m_ibs.Put(reinterpret_cast<ubyte*>(&node->data[0]), node->data.Size());
    }
}

} // namespace hyperion
