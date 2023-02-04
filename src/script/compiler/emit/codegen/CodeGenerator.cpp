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
    new_params.block_offset = build_params.block_offset + m_ibs.GetSize();

    CodeGenerator chunk_generator(new_params);

    for (auto &buildable : chunk->buildables) {
        chunk_generator.BuildableVisitor::Visit(buildable.get());
    }

    // bake the chunk's byte stream
    const Array<UByte> &chunk_bytes = chunk_generator.GetInternalByteStream().Bake();

    // append bytes to this chunk's InternalByteStream
    m_ibs.Put(chunk_bytes.Data(), chunk_bytes.Size());
}

void CodeGenerator::Visit(LabelMarker *node)
{
    m_ibs.MarkLabel(node->id);
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
        m_ibs.Put(Instructions::POP_N);

        byte as_byte = (byte)node->amt;
        m_ibs.Put(as_byte);
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
    m_ibs.Put((byte*)&node->value, sizeof(node->value));
}

void CodeGenerator::Visit(ConstI64 *node)
{
    m_ibs.Put(Instructions::LOAD_I64);
    m_ibs.Put(node->reg);
    m_ibs.Put((byte*)&node->value, sizeof(node->value));
}

void CodeGenerator::Visit(ConstU32 *node)
{
    m_ibs.Put(Instructions::LOAD_U32);
    m_ibs.Put(node->reg);
    m_ibs.Put((byte*)&node->value, sizeof(node->value));
}

void CodeGenerator::Visit(ConstU64 *node)
{
    m_ibs.Put(Instructions::LOAD_U64);
    m_ibs.Put(node->reg);
    m_ibs.Put((byte*)&node->value, sizeof(node->value));
}

void CodeGenerator::Visit(ConstF32 *node)
{
    m_ibs.Put(Instructions::LOAD_F32);
    m_ibs.Put(node->reg);
    m_ibs.Put((byte*)&node->value, sizeof(node->value));
}

void CodeGenerator::Visit(ConstF64 *node)
{
    m_ibs.Put(Instructions::LOAD_F64);
    m_ibs.Put(node->reg);
    m_ibs.Put((byte*)&node->value, sizeof(node->value));
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

    uint16_t name_len = (uint16_t)node->name.length();
    m_ibs.Put((byte*)&name_len, sizeof(name_len));
    m_ibs.Put((byte*)&node->name[0], node->name.length());

    uint16_t size = (uint16_t)node->members.Size();
    m_ibs.Put((byte*)&size, sizeof(size));

    for (const std::string &member_name : node->members) {
        uint16_t member_name_len = (uint16_t)member_name.length();
        m_ibs.Put((byte*)&member_name_len, sizeof(member_name_len));
        m_ibs.Put((byte*)&member_name[0], member_name.length());
    }
}

void CodeGenerator::Visit(BuildableString *node)
{
    uint32_t len = node->value.length();

    // TODO: make it store and load statically
    m_ibs.Put(Instructions::LOAD_STRING);
    m_ibs.Put(node->reg);
    m_ibs.Put((byte*)&len, sizeof(len));
    m_ibs.Put((byte*)&node->value[0], node->value.length());
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
                            m_ibs.Put((byte*)&node->op.a.reg, sizeof(node->op.a.reg));
                            m_ibs.Put((byte*)&node->op.b.offset, sizeof(node->op.b.offset));

                            break;
                        case Operations::STORE:
                            m_ibs.Put(Instructions::MOV_OFFSET);
                            m_ibs.Put((byte*)&node->op.b.offset, sizeof(node->op.b.offset));
                            m_ibs.Put((byte*)&node->op.a.reg, sizeof(node->op.a.reg));

                            break;
                    }
                    
                    break;

                case Strategies::BY_INDEX:
                    switch (node->operation) {
                        case Operations::LOAD:
                            m_ibs.Put(node->op.is_ref ? Instructions::LOAD_INDEX_REF : Instructions::LOAD_INDEX);
                            m_ibs.Put((byte*)&node->op.a.reg, sizeof(node->op.a.reg));
                            m_ibs.Put((byte*)&node->op.b.index, sizeof(node->op.b.index));

                            break;
                        case Operations::STORE:
                            m_ibs.Put(Instructions::MOV_INDEX);
                            m_ibs.Put((byte*)&node->op.b.index, sizeof(node->op.b.index));
                            m_ibs.Put((byte*)&node->op.a.reg, sizeof(node->op.a.reg));

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
                            m_ibs.Put((byte*)&node->op.a.reg, sizeof(node->op.a.reg));
                            m_ibs.Put((byte*)&node->op.b.index, sizeof(node->op.b.index));

                            break;
                        case Operations::STORE:
                            AssertThrowMsg(false, "Not implemented");

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
                            m_ibs.Put((byte*)&node->op.a.reg, sizeof(node->op.a.reg));
                            m_ibs.Put((byte*)&node->op.b.object_data.reg, sizeof(node->op.b.object_data.reg));
                            m_ibs.Put((byte*)&node->op.b.object_data.member.index, sizeof(node->op.b.object_data.member.index));

                            break;
                        case Operations::STORE:
                            m_ibs.Put(Instructions::MOV_ARRAYIDX);
                            m_ibs.Put((byte*)&node->op.b.object_data.reg, sizeof(node->op.b.object_data.reg));
                            m_ibs.Put((byte*)&node->op.b.object_data.member.index, sizeof(node->op.b.object_data.member.index));
                            m_ibs.Put((byte*)&node->op.a.reg, sizeof(node->op.a.reg));

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
                            m_ibs.Put((byte*)&node->op.a.reg, sizeof(node->op.a.reg));
                            m_ibs.Put((byte*)&node->op.b.object_data.reg, sizeof(node->op.b.object_data.reg));
                            m_ibs.Put((byte*)&node->op.b.object_data.member.index, sizeof(node->op.b.object_data.member.index));

                            break;
                        case Operations::STORE:
                            m_ibs.Put(Instructions::MOV_MEM);
                            m_ibs.Put((byte*)&node->op.b.object_data.reg, sizeof(node->op.b.object_data.reg));
                            m_ibs.Put((byte*)&node->op.b.object_data.member.index, sizeof(node->op.b.object_data.member.index));
                            m_ibs.Put((byte*)&node->op.a.reg, sizeof(node->op.a.reg));

                            break;
                    }

                    break;
                
                case Strategies::BY_HASH:
                    switch (node->operation) {
                        case Operations::LOAD:
                            m_ibs.Put(Instructions::LOAD_MEM_HASH);
                            m_ibs.Put((byte*)&node->op.a.reg, sizeof(node->op.a.reg));
                            m_ibs.Put((byte*)&node->op.b.object_data.reg, sizeof(node->op.b.object_data.reg));
                            m_ibs.Put((byte*)&node->op.b.object_data.member.hash, sizeof(node->op.b.object_data.member.hash));

                            break;
                        case Operations::STORE:
                            m_ibs.Put(Instructions::MOV_MEM_HASH);
                            m_ibs.Put((byte*)&node->op.b.object_data.reg, sizeof(node->op.b.object_data.reg));
                            m_ibs.Put((byte*)&node->op.b.object_data.member.hash, sizeof(node->op.b.object_data.member.hash));
                            m_ibs.Put((byte*)&node->op.a.reg, sizeof(node->op.a.reg));

                            break;
                    }

                    break;
            }

            break;
    }
}

void CodeGenerator::Visit(Comment *node)
{
    const SizeType size = node->value.length();
    UInt32 len = UInt32(size);

    m_ibs.Put(Instructions::REM);
    m_ibs.Put((byte*)&len, sizeof(len));
    m_ibs.Put((byte*)&node->value[0], size);
}

void CodeGenerator::Visit(SymbolExport *node)
{
    const UInt32 hash = hash_fnv_1(node->name.c_str());

    m_ibs.Put(Instructions::EXPORT);
    m_ibs.Put((byte*)&node->reg, sizeof(node->reg));
    m_ibs.Put((byte*)&hash, sizeof(hash));
}

void CodeGenerator::Visit(RawOperation<> *node)
{
    m_ibs.Put(node->opcode);

    if (node->data.Any()) {
        m_ibs.Put((byte*)&node->data[0], node->data.Size());
    }
}

} // namespace hyperion::compiler
