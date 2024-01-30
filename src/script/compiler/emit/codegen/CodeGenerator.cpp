#include <script/compiler/emit/codegen/CodeGenerator.hpp>
#include <script/Hasher.hpp>
#include <iostream>

namespace hyperion::compiler {

CodeGenerator::CodeGenerator(BuildParams &build_params)
    : build_params(build_params)
{
}

void CodeGenerator::Visit(BytecodeChunk *chunk)
{
    BuildParams new_params;
    new_params.block_offset = build_params.block_offset + m_ibs.GetPosition();
    new_params.labels       = build_params.labels;

    for (const LabelId label_id : chunk->labels) {
        new_params.labels.Insert({
            label_id,
            LabelPosition(-1),
            HYP_NAME(LabelNameRemoved)
        });
    }

    CodeGenerator code_generator(new_params);

    for (auto &buildable : chunk->buildables) {
        code_generator.BuildableVisitor::Visit(buildable.get());
    }

    // bake the chunk's byte stream
    //code_generator.GetInternalByteStream().Bake(new_params);

    const Array<UByte> &bytes = code_generator.GetInternalByteStream().GetData();
    const Array<Fixup> &fixups = code_generator.GetInternalByteStream().GetFixups();

    const SizeType fixup_offset = m_ibs.GetPosition();

    // append bytes to this chunk's InternalByteStream
    m_ibs.Put(bytes.Data(), bytes.Size());

    // Copy fixups from the chunk's InternalByteStream to this one's
    for (const Fixup &fixup : fixups) {
        m_ibs.AddFixup(fixup.label_id, fixup.position + fixup_offset, fixup.offset);
    }

    build_params.labels = new_params.labels;
}

void CodeGenerator::Bake()
{
    m_ibs.Bake(build_params);
}

void CodeGenerator::Visit(LabelMarker *node)
{
    const LabelId label_id = node->id;

    const auto it = build_params.labels.FindIf([label_id](const LabelInfo &label_info)
    {
        return label_info.label_id == label_id;
    });

    AssertThrowMsg(it != build_params.labels.End(), "Label with ID %d not found", label_id);

    AssertThrowMsg(it->position == LabelPosition(-1), "Label position already set");

    it->position = LabelPosition(m_ibs.GetPosition() + build_params.block_offset);
}

void CodeGenerator::Visit(Jump *node)
{
    switch (node->jump_class) {
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
    m_ibs.AddFixup(node->label_id, build_params.block_offset);
}

void CodeGenerator::Visit(Comparison *node)
{
    switch (node->comparison_class) {
        case Comparison::ComparisonClass::CMP:
            m_ibs.Put(Instructions::CMP);
            break;
        case Comparison::ComparisonClass::CMPZ:
            m_ibs.Put(Instructions::CMPZ);
            break;
    }

    m_ibs.Put(node->reg_lhs);

    if (node->comparison_class == Comparison::ComparisonClass::CMP) {
        m_ibs.Put(node->reg_rhs);
    }
}

void CodeGenerator::Visit(FunctionCall *node)
{
    m_ibs.Put(Instructions::CALL);
    m_ibs.Put(node->reg);
    m_ibs.Put(node->nargs);
}

void CodeGenerator::Visit(Return *node)
{
    m_ibs.Put(Instructions::RET);
}

void CodeGenerator::Visit(StoreLocal *node)
{
    m_ibs.Put(Instructions::PUSH);
    m_ibs.Put(node->reg);
}

void CodeGenerator::Visit(PopLocal *node)
{
    if (node->amt > 1) {
        m_ibs.Put(Instructions::SUB_SP);

        UInt16 as_u16 = (UInt16)node->amt;
        m_ibs.Put(reinterpret_cast<UByte *>(&as_u16), sizeof(as_u16));
    } else {
        m_ibs.Put(Instructions::POP);
    }
}

void CodeGenerator::Visit(LoadRef *node)
{
    m_ibs.Put(Instructions::REF);
    m_ibs.Put(node->dst);
    m_ibs.Put(node->src);
}

void CodeGenerator::Visit(LoadDeref *node)
{
    m_ibs.Put(Instructions::DEREF);
    m_ibs.Put(node->dst);
    m_ibs.Put(node->src);
}

void CodeGenerator::Visit(ConstI32 *node)
{
    m_ibs.Put(Instructions::LOAD_I32);
    m_ibs.Put(node->reg);
    m_ibs.Put(reinterpret_cast<UByte *>(&node->value), sizeof(node->value));
}

void CodeGenerator::Visit(ConstI64 *node)
{
    m_ibs.Put(Instructions::LOAD_I64);
    m_ibs.Put(node->reg);
    m_ibs.Put(reinterpret_cast<UByte *>(&node->value), sizeof(node->value));
}

void CodeGenerator::Visit(ConstU32 *node)
{
    m_ibs.Put(Instructions::LOAD_U32);
    m_ibs.Put(node->reg);
    m_ibs.Put(reinterpret_cast<UByte *>(&node->value), sizeof(node->value));
}

void CodeGenerator::Visit(ConstU64 *node)
{
    m_ibs.Put(Instructions::LOAD_U64);
    m_ibs.Put(node->reg);
    m_ibs.Put(reinterpret_cast<UByte *>(&node->value), sizeof(node->value));
}

void CodeGenerator::Visit(ConstF32 *node)
{
    m_ibs.Put(Instructions::LOAD_F32);
    m_ibs.Put(node->reg);
    m_ibs.Put(reinterpret_cast<UByte *>(&node->value), sizeof(node->value));
}

void CodeGenerator::Visit(ConstF64 *node)
{
    m_ibs.Put(Instructions::LOAD_F64);
    m_ibs.Put(node->reg);
    m_ibs.Put(reinterpret_cast<UByte *>(&node->value), sizeof(node->value));
}

void CodeGenerator::Visit(ConstBool *node)
{
    m_ibs.Put(node->value
        ? Instructions::LOAD_TRUE
        : Instructions::LOAD_FALSE);
    m_ibs.Put(node->reg);
}

void CodeGenerator::Visit(ConstNull *node)
{
    m_ibs.Put(Instructions::LOAD_NULL);
    m_ibs.Put(node->reg);
}

void CodeGenerator::Visit(BuildableTryCatch *node)
{
    m_ibs.Put(Instructions::BEGIN_TRY);
    m_ibs.AddFixup(node->catch_label_id, build_params.block_offset);
}

void CodeGenerator::Visit(BuildableFunction *node)
{
    // TODO: make it store and load statically
    m_ibs.Put(Instructions::LOAD_FUNC);
    m_ibs.Put(node->reg);
    m_ibs.AddFixup(node->label_id, build_params.block_offset);
    m_ibs.Put(node->nargs);
    m_ibs.Put(node->flags);
}

void CodeGenerator::Visit(BuildableType *node)
{
    // TODO: make it store and load statically
    m_ibs.Put(Instructions::LOAD_TYPE);
    m_ibs.Put(node->reg);

    uint16_t name_len = (uint16_t)node->name.Size();
    m_ibs.Put(reinterpret_cast<UByte *>(&name_len), sizeof(name_len));
    m_ibs.Put(reinterpret_cast<UByte *>(node->name.Data()), node->name.Size());

    uint16_t size = (uint16_t)node->members.Size();
    m_ibs.Put(reinterpret_cast<UByte *>(&size), sizeof(size));

    for (const String &member_name : node->members) {
        uint16_t member_name_len = (uint16_t)member_name.Size();
        m_ibs.Put(reinterpret_cast<UByte *>(&member_name_len), sizeof(member_name_len));
        m_ibs.Put(reinterpret_cast<const UByte *>(member_name.Data()), member_name.Size());
    }
}

void CodeGenerator::Visit(BuildableString *node)
{
    const UInt32 len = UInt32(node->value.Size());

    // TODO: make it store and load statically
    m_ibs.Put(Instructions::LOAD_STRING);
    m_ibs.Put(node->reg);
    m_ibs.Put(reinterpret_cast<const UByte *>(&len), sizeof(len));
    m_ibs.Put(reinterpret_cast<const UByte *>(node->value.Data()), node->value.Size());
}

void CodeGenerator::Visit(StorageOperation *node)
{
    switch (node->method) {
    case Methods::LOCAL:
        switch (node->strategy) {
        case Strategies::BY_OFFSET:
            switch (node->operation) {
            case Operations::LOAD:
                m_ibs.Put(node->op.is_ref ? Instructions::LOAD_OFFSET_REF : Instructions::LOAD_OFFSET);
                m_ibs.Put(reinterpret_cast<UByte *>(&node->op.a.reg), sizeof(node->op.a.reg));
                m_ibs.Put(reinterpret_cast<UByte *>(&node->op.b.offset), sizeof(node->op.b.offset));

                break;
            case Operations::STORE:
                m_ibs.Put(Instructions::MOV_OFFSET);
                m_ibs.Put(reinterpret_cast<UByte *>(&node->op.b.offset), sizeof(node->op.b.offset));
                m_ibs.Put(reinterpret_cast<UByte *>(&node->op.a.reg), sizeof(node->op.a.reg));

                break;
            }
            
            break;

        case Strategies::BY_INDEX:
            switch (node->operation) {
            case Operations::LOAD:
                m_ibs.Put(node->op.is_ref ? Instructions::LOAD_INDEX_REF : Instructions::LOAD_INDEX);
                m_ibs.Put(reinterpret_cast<UByte *>(&node->op.a.reg), sizeof(node->op.a.reg));
                m_ibs.Put(reinterpret_cast<UByte *>(&node->op.b.index), sizeof(node->op.b.index));

                break;
            case Operations::STORE:
                m_ibs.Put(Instructions::MOV_INDEX);
                m_ibs.Put(reinterpret_cast<UByte *>(&node->op.b.index), sizeof(node->op.b.index));
                m_ibs.Put(reinterpret_cast<UByte *>(&node->op.a.reg), sizeof(node->op.a.reg));

                break;
            }

            break;
        
        case Strategies::BY_HASH:
            AssertThrowMsg(false, "Not implemented");

            break;
        }

        break;

    case Methods::STATIC:
        switch (node->strategy) {
        case Strategies::BY_OFFSET:
            AssertThrowMsg(false, "Not implemented");
            
            break;

        case Strategies::BY_INDEX:
            switch (node->operation) {
            case Operations::LOAD:
                m_ibs.Put(Instructions::LOAD_STATIC);
                m_ibs.Put(reinterpret_cast<UByte *>(&node->op.a.reg), sizeof(node->op.a.reg));
                m_ibs.Put(reinterpret_cast<UByte *>(&node->op.b.index), sizeof(node->op.b.index));

                break;
            case Operations::STORE:
                m_ibs.Put(Instructions::MOV_STATIC);
                m_ibs.Put(reinterpret_cast<UByte *>(&node->op.b.index), sizeof(node->op.b.index));
                m_ibs.Put(reinterpret_cast<UByte *>(&node->op.a.reg), sizeof(node->op.a.reg));

                break;
            }

            break;
        
        case Strategies::BY_HASH:
            AssertThrowMsg(false, "Not implemented");

            break;
        }

        break;

    case Methods::ARRAY:
        switch (node->strategy) {
        case Strategies::BY_OFFSET:
            AssertThrowMsg(false, "Not implemented");
            
            break;

        case Strategies::BY_INDEX:
            switch (node->operation) {
            case Operations::LOAD:
                m_ibs.Put(Instructions::LOAD_ARRAYIDX);
                m_ibs.Put(reinterpret_cast<UByte *>(&node->op.a.reg), sizeof(node->op.a.reg));
                m_ibs.Put(reinterpret_cast<UByte *>(&node->op.b.object_data.reg), sizeof(node->op.b.object_data.reg));
                m_ibs.Put(reinterpret_cast<UByte *>(&node->op.b.object_data.member.index), sizeof(node->op.b.object_data.member.index));

                break;
            case Operations::STORE:
                m_ibs.Put(Instructions::MOV_ARRAYIDX);
                m_ibs.Put(reinterpret_cast<UByte *>(&node->op.b.object_data.reg), sizeof(node->op.b.object_data.reg));
                m_ibs.Put(reinterpret_cast<UByte *>(&node->op.b.object_data.member.index), sizeof(node->op.b.object_data.member.index));
                m_ibs.Put(reinterpret_cast<UByte *>(&node->op.a.reg), sizeof(node->op.a.reg));

                break;
            }

            break;
        
        case Strategies::BY_HASH:
            AssertThrowMsg(false, "Not implemented");

            break;
        }

        break;

    case Methods::MEMBER:
        switch (node->strategy) {
        case Strategies::BY_OFFSET:
            AssertThrowMsg(false, "Not implemented");
            
            break;

        case Strategies::BY_INDEX:
            switch (node->operation) {
            case Operations::LOAD:
                m_ibs.Put(Instructions::LOAD_MEM);
                m_ibs.Put(reinterpret_cast<UByte *>(&node->op.a.reg), sizeof(node->op.a.reg));
                m_ibs.Put(reinterpret_cast<UByte *>(&node->op.b.object_data.reg), sizeof(node->op.b.object_data.reg));
                m_ibs.Put(reinterpret_cast<UByte *>(&node->op.b.object_data.member.index), sizeof(node->op.b.object_data.member.index));

                break;
            case Operations::STORE:
                m_ibs.Put(Instructions::MOV_MEM);
                m_ibs.Put(reinterpret_cast<UByte *>(&node->op.b.object_data.reg), sizeof(node->op.b.object_data.reg));
                m_ibs.Put(reinterpret_cast<UByte *>(&node->op.b.object_data.member.index), sizeof(node->op.b.object_data.member.index));
                m_ibs.Put(reinterpret_cast<UByte *>(&node->op.a.reg), sizeof(node->op.a.reg));

                break;
            }

            break;
        
        case Strategies::BY_HASH:
            switch (node->operation) {
            case Operations::LOAD:
                m_ibs.Put(Instructions::LOAD_MEM_HASH);
                m_ibs.Put(reinterpret_cast<UByte *>(&node->op.a.reg), sizeof(node->op.a.reg));
                m_ibs.Put(reinterpret_cast<UByte *>(&node->op.b.object_data.reg), sizeof(node->op.b.object_data.reg));
                m_ibs.Put(reinterpret_cast<UByte *>(&node->op.b.object_data.member.hash), sizeof(node->op.b.object_data.member.hash));

                break;
            case Operations::STORE:
                m_ibs.Put(Instructions::MOV_MEM_HASH);
                m_ibs.Put(reinterpret_cast<UByte *>(&node->op.b.object_data.reg), sizeof(node->op.b.object_data.reg));
                m_ibs.Put(reinterpret_cast<UByte *>(&node->op.b.object_data.member.hash), sizeof(node->op.b.object_data.member.hash));
                m_ibs.Put(reinterpret_cast<UByte *>(&node->op.a.reg), sizeof(node->op.a.reg));

                break;
            }

            break;
        }

        break;
    }
}

void CodeGenerator::Visit(Comment *node)
{
    const SizeType size = node->value.Size();
    UInt32 len = UInt32(size);

    m_ibs.Put(Instructions::REM);
    m_ibs.Put(reinterpret_cast<UByte *>(&len), sizeof(len));
    m_ibs.Put(reinterpret_cast<UByte *>(node->value.Data()), size);
}

void CodeGenerator::Visit(SymbolExport *node)
{
    const UInt32 hash = hash_fnv_1(node->name.Data());

    m_ibs.Put(Instructions::EXPORT);
    m_ibs.Put(reinterpret_cast<UByte *>(&node->reg), sizeof(node->reg));
    m_ibs.Put(reinterpret_cast<const UByte *>(&hash), sizeof(hash));
}

void CodeGenerator::Visit(CastOperation *node)
{
    const UInt8 cast_instruction = UInt8(Instructions::CAST_U8) + UInt8(node->type);
    AssertThrowMsg(
        cast_instruction >= Instructions::CAST_U8 && cast_instruction <= UInt8(Instructions::CAST_DYNAMIC),
        "Invalid cast type"
    );

    m_ibs.Put(cast_instruction);
    m_ibs.Put(reinterpret_cast<UByte *>(&node->reg_dst), sizeof(node->reg_dst));
    m_ibs.Put(reinterpret_cast<UByte *>(&node->reg_src), sizeof(node->reg_src));
}

void CodeGenerator::Visit(RawOperation<> *node)
{
    m_ibs.Put(node->opcode);

    if (node->data.Any()) {
        m_ibs.Put(reinterpret_cast<UByte *>(&node->data[0]), node->data.Size());
    }
}

} // namespace hyperion::compiler
