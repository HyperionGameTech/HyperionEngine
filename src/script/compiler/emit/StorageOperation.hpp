#pragma once

#include <script/compiler/emit/Instruction.hpp>

#include <core/debug/Debug.hpp>
#include <core/Types.hpp>
#include <core/HashCode.hpp>

namespace Hyperion {

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
            uint16 index;
            uint16 offset;
            uint64 hash;

            struct
            {
                RegIndex reg;

                union
                {
                    RegIndex reg;
                    uint32 index;
                    uint64 hash;
                } member;
            } objectData;
        } b;

        bool isRef = false;
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
        OperationBuilder(StorageOperation* op)
            : op(op)
        {
        }

        virtual ~OperationBuilder() = default;

        MethodBuilder Load(RegIndex dst, bool isRef = false);
        MethodBuilder Store(RegIndex src);

    private:
        StorageOperation* op;
    };

    struct MethodBuilder : public StorageOperationBuilder
    {
        MethodBuilder(StorageOperation* op, OperationBuilder* parent)
            : op(op),
              parent(parent)
        {
        }

        virtual ~MethodBuilder() = default;

        StrategyBuilder Local();
        StrategyBuilder Static();
        StrategyBuilder Array(RegIndex arrayReg);
        StrategyBuilder Member(RegIndex objectReg);

        Methods method;

    private:
        StorageOperation* op;
        OperationBuilder* parent;
    };

    struct StrategyBuilder : public StorageOperationBuilder
    {
        StrategyBuilder(StorageOperation* op, MethodBuilder* parent)
            : op(op),
              parent(parent)
        {
        }

        void ByIndex(int index);
        void ByOffset(int offset);
        void ByHash(HashCode::ValueType hash);

        Strategies strategy;

    private:
        StorageOperation* op;
        MethodBuilder* parent;
    };
};

} // namespace Hyperion
