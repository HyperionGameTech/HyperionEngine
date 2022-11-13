#include <script/compiler/emit/StorageOperation.hpp>

namespace hyperion::compiler {

void StorageOperation::StrategyBuilder::ByIndex(int index)
{
    op->strategy = strategy = Strategies::BY_INDEX;

    switch (parent->method) {
        case Methods::ARRAY:
        case Methods::MEMBER:
            op->op.b.object_data.member.index = index;
            break;
        default:
            op->op.b.index = index;
            break;
    }
}

void StorageOperation::StrategyBuilder::ByOffset(int offset)
{
    op->strategy = strategy = Strategies::BY_OFFSET;
    
    switch (parent->method) {
        case Methods::ARRAY:
        case Methods::MEMBER:
            AssertThrowMsg(false, "Not implemented");
            break;
        default:
            op->op.b.offset = offset;
            break;
    }
}

void StorageOperation::StrategyBuilder::ByHash(int hash)
{
    op->strategy = strategy = Strategies::BY_HASH;
    
    switch (parent->method) {
        case Methods::ARRAY:
        case Methods::MEMBER:
            op->op.b.object_data.member.hash = hash;
            break;
        default:
            op->op.b.hash = hash;
            break;
    }
}

StorageOperation::StrategyBuilder StorageOperation::MethodBuilder::Local()
{
    op->method = method = Methods::LOCAL;

    return StrategyBuilder(op, this);
}

StorageOperation::StrategyBuilder StorageOperation::MethodBuilder::Static()
{
    op->method = method = Methods::STATIC;

    return StrategyBuilder(op, this);
}

StorageOperation::StrategyBuilder StorageOperation::MethodBuilder::Array(RegIndex array_reg)
{
    op->method = method = Methods::ARRAY;
    op->op.b.object_data.reg = array_reg;

    return StrategyBuilder(op, this);
}

StorageOperation::StrategyBuilder StorageOperation::MethodBuilder::Member(RegIndex object_reg)
{
    op->method = method = Methods::MEMBER;
    op->op.b.object_data.reg = object_reg;

    return StrategyBuilder(op, this);
}

StorageOperation::MethodBuilder StorageOperation::OperationBuilder::Load(RegIndex dst, bool is_ref)
{
    op->operation = Operations::LOAD;
    op->op.a.reg = dst;
    op->op.is_ref = is_ref;

    return MethodBuilder(op, this);
}

StorageOperation::MethodBuilder StorageOperation::OperationBuilder::Store(RegIndex src)
{
    op->operation = Operations::STORE;
    op->op.a.reg = src;

    return MethodBuilder(op, this);
}

StorageOperation::OperationBuilder StorageOperation::GetBuilder()
{
    return OperationBuilder(this);
}

} // namespace hyperion::compiler
