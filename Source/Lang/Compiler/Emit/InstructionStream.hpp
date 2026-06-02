#pragma once

#include <Lang/Compiler/Emit/Instruction.hpp>
#include <Lang/Compiler/Emit/StaticObject.hpp>

#include <Lang/Compiler/Tree.hpp>

#include <Core/Utilities/Optional.hpp>

#include <Core/Types.hpp>

namespace Hyperion {

enum InstructionStreamContextType : uint32
{
    INSTRUCTION_STREAM_CONTEXT_DEFAULT = 0,
    INSTRUCTION_STREAM_CONTEXT_LOOP,
    INSTRUCTION_STREAM_CONTEXT_SWITCH
};

class InstructionStreamContext
{
public:
    InstructionStreamContext(
        InstructionStreamContext* parent = nullptr,
        InstructionStreamContextType type = INSTRUCTION_STREAM_CONTEXT_DEFAULT)
        : m_parent(parent),
          m_type(type),
          m_nextLabelId(0)
    {
    }

    InstructionStreamContext(const InstructionStreamContext& other) = delete;
    InstructionStreamContext& operator=(const InstructionStreamContext& other) = delete;
    InstructionStreamContext(InstructionStreamContext&& other) noexcept = delete;
    InstructionStreamContext& operator=(InstructionStreamContext&& other) noexcept = delete;
    ~InstructionStreamContext() = default;

    InstructionStreamContextType GetType() const
    {
        return m_type;
    }

    LabelId NewLabel()
    {
        const LabelId labelId = NextLabelID();
        m_labels.PushBack(LabelInfo { labelId, LabelPosition(-1), HYP_NAME(LabelNotNamed) });
        return labelId;
    }

    LabelId NewLabel(Name name)
    {
        Assert(!FindLabelByName(name).HasValue(), "Cannot duplicate label identifier");

        const LabelId labelId = NextLabelID();
        m_labels.PushBack(LabelInfo { labelId, LabelPosition(-1), name });
        return labelId;
    }

    Optional<LabelId> FindLabelByName(Name name) const
    {
        const auto it = m_labels.FindIf([name](const LabelInfo& labelInfo)
            {
                return labelInfo.name == name;
            });

        if (it == m_labels.End())
        {
            return Optional<LabelId> {};
        }

        return it->labelId;
    }

private:
    LabelId NextLabelID()
    {
        if (m_parent)
        {
            return m_parent->NextLabelID();
        }

        return m_nextLabelId++;
    }

    InstructionStreamContext* m_parent;
    InstructionStreamContextType m_type;
    Array<LabelInfo> m_labels;

    uint32 m_nextLabelId;
};

struct InstructionStreamContextGuard : TreeNodeGuard<InstructionStreamContext>
{
    InstructionStreamContextGuard(
        Tree<InstructionStreamContext>* tree,
        InstructionStreamContextType type = INSTRUCTION_STREAM_CONTEXT_DEFAULT)
        : TreeNodeGuard(tree, &tree->Root(), type)
    {
    }
};

class InstructionStream
{
public:
    InstructionStream();
    InstructionStream(const InstructionStream& other);

    uint8 GetCurrentRegister() const
    {
        return m_registerCounter;
    }

    uint8 IncRegisterUsage()
    {
        Assert(m_registerCounter < UINT8_MAX);

        return ++m_registerCounter;
    }

    uint8 DecRegisterUsage()
    {
        Assert(m_registerCounter > 0);

        return --m_registerCounter;
    }

    int GetStackSize() const
    {
        return m_stackSize;
    }

    void SetStackSize(int stackSize)
    {
        m_stackSize = stackSize;
    }

    int IncStackSize()
    {
        return ++m_stackSize;
    }

    int DecStackSize()
    {
        Assert(m_stackSize > 0, "Compiler stack size record invalid");

        return --m_stackSize;
    }

    int NewStaticId()
    {
        return m_staticId++;
    }

    void AddStaticObject(const StaticObject& staticObject)
    {
        m_staticObjects.PushBack(staticObject);
    }

    int FindStaticObject(const StaticObject& staticObject) const;

    Tree<InstructionStreamContext>& GetContextTree()
    {
        return m_contextTree;
    }

    const Tree<InstructionStreamContext>& GetContextTree() const
    {
        return m_contextTree;
    }

private:
    // incremented and decremented each time a register
    // is used/unused
    uint8 m_registerCounter;
    // incremented each time a variable is pushed,
    // decremented each time a stack frame is closed
    int m_stackSize;
    // the current static object id
    int m_staticId;

    Array<StaticObject> m_staticObjects;

    Tree<InstructionStreamContext> m_contextTree;
};

} // namespace Hyperion
