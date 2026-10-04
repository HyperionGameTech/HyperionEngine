/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <HyperionPch.hpp>

#include <Framework/DeviceTier/DeviceTier.hpp>

#include <Core/Core.hpp>

#include <algorithm>

namespace Hyperion {

ENGINE_API HYP_DECLARE_LOG_CHANNEL(Engine);

DeviceTierRegistry& DeviceTierRegistry::GetInstance()
{
    static DeviceTierRegistry s_instance;

    return s_instance;
}

void DeviceTierRegistry::RegisterAxis(DeviceTierAxisBase* axis)
{
    m_axes.PushBack(axis);
}

void DeviceTierRegistry::RegisterTier(DeviceTierBase* tier)
{
    m_tiers.PushBack(tier);
}

const DeviceTierAxisBase* DeviceTierRegistry::FindAxis(const ANSIStringView& axisName) const
{
    for (const DeviceTierAxisBase* axis : m_axes)
    {
        if (IsSameName(axis->GetName(), axisName))
        {
            return axis;
        }
    }

    return nullptr;
}

const DeviceTierAxisBase* DeviceTierRegistry::FindAxis(TypeId axisTypeId) const
{
    for (const DeviceTierAxisBase* axis : m_axes)
    {
        if (axis->GetTypeId() == axisTypeId)
        {
            return axis;
        }
    }

    return nullptr;
}

void DeviceTierRegistry::Resolve(DeviceTierPhase phase, DeviceFacts& facts) const
{
    for (const DeviceTierAxisBase* axis : m_axes)
    {
        if (axis->GetPhase() != phase)
        {
            continue;
        }

        bool wasForced = false;
        const uint64 value = ResolveAxis(*axis, facts, wasForced);

        facts.resolvedAxes.Set(axis->GetTypeId(), value);

        if (wasForced)
        {
            facts.forcedAxes.Set(axis->GetTypeId(), true);
        }
    }
}

uint64 DeviceTierRegistry::ResolveAxis(const DeviceTierAxisBase& axis, const DeviceFacts& facts, bool& outWasForced) const
{
    outWasForced = false;

    const CommandLineArguments& commandLineArguments = CoreApi::GetCommandLineArguments();

    if (commandLineArguments.Contains(axis.GetCommandLineArgumentName()))
    {
        const CommandLineArgumentValue& argumentValue = commandLineArguments[axis.GetCommandLineArgumentName()];

        if (argumentValue.IsString())
        {
            const ANSIString valueName = argumentValue.ToString().ToAnsi();

            uint64 forcedValue = 0;

            if (axis.ParseValue(ANSIStringView(valueName.Data()), forcedValue))
            {
                outWasForced = true;

                return forcedValue;
            }

            HYP_LOG(Engine, Warning, "Ignoring --{}={}: not a value of {}", axis.GetCommandLineArgumentName(), valueName, axis.GetName());
        }
    }

    Array<const DeviceTierBase*> axisTiers;

    for (const DeviceTierBase* tier : m_tiers)
    {
        if (tier->GetAxisTypeId() == axis.GetTypeId())
        {
            axisTiers.PushBack(tier);
        }
    }

    std::sort(axisTiers.Begin(), axisTiers.End(), [](const DeviceTierBase* a, const DeviceTierBase* b)
        {
            return a->GetValue() > b->GetValue();
        });

    for (const DeviceTierBase* tier : axisTiers)
    {
        if (tier->Decide(facts))
        {
            return tier->GetValue();
        }
    }

    return 0;
}

} // namespace Hyperion
