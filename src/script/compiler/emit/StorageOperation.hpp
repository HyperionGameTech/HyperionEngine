#ifndef STORAGE_OPERATION_HPP
#define STORAGE_OPERATION_HPP

#include <script/compiler/emit/Instruction.hpp>

#include <system/debug.h>

#include <vector>
#include <memory>

namespace hyperion {
namespace compiler {

enum class Operations {
    LOAD,
    STORE,
};

enum class Methods {
    LOCAL,
    STATIC,
    ARRAY,
    MEMBER,
};

enum class Strategies {
    BY_OFFSET,
    BY_INDEX,
    BY_HASH,
};

struct StorageOperation : public Buildable {
    // fwd decls
    struct OperationBuilder;
    struct MethodBuilder;
    struct StrategyBuilder;

    StorageOperation() = default;
    virtual ~StorageOperation() = default;

    OperationBuilder GetBuilder();

    struct {
        union {
            RegIndex reg;
        } a;

        union {
            uint16_t index;
            uint16_t offset;
            uint32_t hash;

            struct {
                RegIndex reg;

                union {
                    RegIndex reg;
                    uint8_t index;
                    uint32_t hash;
                } member;
            } object_data;
        } b;
    } op;

    Operations operation;
    Methods method;
    Strategies strategy;

    struct StorageOperationBuilder {
        virtual ~StorageOperationBuilder() = default;
    };

    struct OperationBuilder : public StorageOperationBuilder {
        OperationBuilder(StorageOperation *op)
            : op(op)
        {
        }

        virtual ~OperationBuilder() = default;

        MethodBuilder Load(RegIndex dst);
        MethodBuilder Store(RegIndex src);

    private:
        StorageOperation *op;
    };

    struct MethodBuilder : public StorageOperationBuilder {
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

    struct StrategyBuilder : public StorageOperationBuilder {
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

} // namespace compiler
} // namespace hyperion

#endif
