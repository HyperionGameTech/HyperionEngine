#ifndef STORAGE_OPERATION_HPP
#define STORAGE_OPERATION_HPP

#include <script/compiler/emit/Instruction.hpp>

#include <system/Debug.hpp>
#include <Types.hpp>

#include <vector>
#include <memory>

namespace hyperion::compiler {

enum class Operations
{
    LOAD,
    STORE
};

enum class Methods
{
    LOCAL,
    STATIC,
    ARRAY,
    MEMBER
};

enum class Strategies
{
    BY_OFFSET,
    BY_INDEX,
    BY_HASH,
};

struct StorageOperation : public Buildable
{
    // fwd decls
    struct OperationBuilder;
    struct MethodBuilder;
    struct StrategyBuilder;

    StorageOperation() = default;
    virtual ~StorageOperation() = default;

    OperationBuilder GetBuilder();

    struct
    {
        union
        {
            RegIndex reg;
        } a;

        union
        {
            UInt16 index;
            UInt16 offset;
            UInt32 hash;

            struct {
                RegIndex reg;

                union {
                    RegIndex reg;
                    UInt8 index;
                    UInt32 hash;
                } member;
            } object_data;
        } b;

        bool is_ref = false;
    } op;

    Operations operation;
    Methods method;
    Strategies strategy;

    struct StorageOperationBuilder
    {
        virtual ~StorageOperationBuilder() = default;
    };

    struct OperationBuilder : public StorageOperationBuilder
    {
        OperationBuilder(StorageOperation *op)
            : op(op)
        {
        }

        virtual ~OperationBuilder() = default;

        MethodBuilder Load(RegIndex dst, bool is_ref = false);
        MethodBuilder Store(RegIndex src);

    private:
        StorageOperation *op;
    };

    struct MethodBuilder : public StorageOperationBuilder
    {
        MethodBuilder(StorageOperation *op, OperationBuilder *parent)
            : op(op),
              parent(parent)
        {
        }

        virtual ~MethodBuilder() = default;

        StrategyBuilder Local();
        StrategyBuilder Static();
        StrategyBuilder Array(RegIndex array_reg);
        StrategyBuilder Member(RegIndex object_reg);

        Methods method;

    private:
        StorageOperation *op;
        OperationBuilder *parent;
    };

    struct StrategyBuilder : public StorageOperationBuilder
    {
        StrategyBuilder(StorageOperation *op, MethodBuilder *parent)
            : op(op),
              parent(parent)
        {
        }

        void ByIndex(int index);
        void ByOffset(int offset);
        void ByHash(int hash);

        Strategies strategy;

    private:
        StorageOperation *op;
        MethodBuilder *parent;
    };
};

} // namespace hyperion::compiler

#endif
