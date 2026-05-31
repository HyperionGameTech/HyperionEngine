#include <Lang/Compiler/Emit/StorageOperation.hpp>

namespace Hyperion {

void StorageOperation::StrategyBuilder::ByIndex(int index)
{
    op->strategy = strategy = Strategies::BY_INDEX;

    switch (parent->method)
    {
    case Methods::ARRAY:
    case Methods::MEMBER:
        op->op.b.objectData.member.index = index;
        break;
    default:
        op->op.b.index = index;
        break;
    }
}

void StorageOperation::StrategyBuilder::ByOffset(int offset)
{
    op->strategy = strategy = Strategies::BY_OFFSET;

    switch (parent->method)
    {
    case Methods::ARRAY:
    case Methods::MEMBER:
        Assert(false, "Not implemented");
        break;
    default:
        op->op.b.offset = offset;
        break;
    }
}

void StorageOperation::StrategyBuilder::ByHash(HashCode::ValueType hash)
{
    op->strategy = strategy = Strategies::BY_HASH;

    switch (parent->method)
    {
    case Methods::ARRAY:
    case Methods::MEMBER:
        op->op.b.objectData.member.hash = hash;
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

StorageOperation::StrategyBuilder StorageOperation::MethodBuilder::Array(RegIndex arrayReg)
{
    op->method = method = Methods::ARRAY;
    op->op.b.objectData.reg = arrayReg;

    return StrategyBuilder(op, this);
}

StorageOperation::StrategyBuilder StorageOperation::MethodBuilder::Member(RegIndex objectReg)
{
    op->method = method = Methods::MEMBER;
    op->op.b.objectData.reg = objectReg;

    return StrategyBuilder(op, this);
}

StorageOperation::MethodBuilder StorageOperation::OperationBuilder::Load(RegIndex dst, bool isRef)
{
    op->operation = Operations::LOAD;
    op->op.a.reg = dst;
    op->op.isRef = isRef;

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

} // namespace Hyperion
