/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Defines.hpp>
#include <Core/Types.hpp>

#include <Core/Containers/Array.hpp>
#include <Core/Containers/String.hpp>

#include <Core/CLI/CommandLine.hpp>

#include <Core/Name/Name.hpp>

#include <Core/Reflection/Enum.hpp>
#include <Core/Reflection/TypeId.hpp>

#include <Core/Utilities/StringView.hpp>

#include <Framework/DeviceTier/DeviceFacts.hpp>

#include <type_traits>

namespace Hyperion {

enum class DeviceTierPhase : uint8
{
    PreGpu,
    PostGpu
};

// StringView::operator== only compares up to the shorter length, so the lengths have to match too
// https://github.com/HyperionGameTech/HyperionEngine/issues/359
HYP_FORCE_INLINE bool IsSameName(const ANSIStringView& lhs, const ANSIStringView& rhs)
{
    return lhs.Length() == rhs.Length() && lhs == rhs;
}

HYP_FORCE_INLINE ANSIStringView GetNameView(const Name& name)
{
    return ANSIStringView(name.LookupString());
}

class DeviceTierBase
{
public:
    virtual ~DeviceTierBase() = default;

    virtual bool Decide(const DeviceFacts& facts) const = 0;

    HYP_FORCE_INLINE TypeId GetAxisTypeId() const
    {
        return m_axisTypeId;
    }

    HYP_FORCE_INLINE uint64 GetValue() const
    {
        return m_value;
    }

protected:
    DeviceTierBase(TypeId axisTypeId, uint64 value)
        : m_axisTypeId(axisTypeId),
          m_value(value)
    {
    }

private:
    TypeId m_axisTypeId;
    uint64 m_value;
};

template <auto Value>
class DeviceTier : public DeviceTierBase
{
    static_assert(std::is_enum_v<decltype(Value)>, "DeviceTier value must be an enum member");
    static_assert(uint64(Value) != 0, "The enum member with value 0 is the default tier and does not have a class");

protected:
    DeviceTier()
        : DeviceTierBase(TypeId::ForType<decltype(Value)>(), uint64(Value))
    {
    }
};

class DeviceTierAxisBase
{
public:
    virtual ~DeviceTierAxisBase() = default;

    virtual bool ParseValue(const ANSIStringView& valueName, uint64& outValue) const = 0;
    virtual ANSIStringView GetValueName(uint64 value) const = 0;

    HYP_FORCE_INLINE const ANSIStringView& GetName() const
    {
        return m_name;
    }

    HYP_FORCE_INLINE const UTF8StringView& GetCommandLineArgumentName() const
    {
        return m_commandLineArgumentName;
    }

    HYP_FORCE_INLINE TypeId GetTypeId() const
    {
        return m_typeId;
    }

    HYP_FORCE_INLINE DeviceTierPhase GetPhase() const
    {
        return m_phase;
    }

protected:
    DeviceTierAxisBase(const ANSIStringView& name, const UTF8StringView& commandLineArgumentName, TypeId typeId, DeviceTierPhase phase)
        : m_name(name),
          m_commandLineArgumentName(commandLineArgumentName),
          m_typeId(typeId),
          m_phase(phase)
    {
    }

private:
    ANSIStringView m_name;
    UTF8StringView m_commandLineArgumentName;
    TypeId m_typeId;
    DeviceTierPhase m_phase;
};

template <class EnumType>
class DeviceTierAxis final : public DeviceTierAxisBase
{
public:
    DeviceTierAxis(const ANSIStringView& name, const UTF8StringView& commandLineArgumentName, DeviceTierPhase phase)
        : DeviceTierAxisBase(name, commandLineArgumentName, TypeId::ForType<EnumType>(), phase)
    {
    }

    bool ParseValue(const ANSIStringView& valueName, uint64& outValue) const override
    {
        bool wasFound = false;

        ForEachEnumMember<EnumType>([&](Name memberName, EnumType memberValue, bool* stopIteration)
            {
                if (IsSameName(GetNameView(memberName), valueName))
                {
                    outValue = uint64(memberValue);
                    wasFound = true;
                    *stopIteration = true;
                }
            });

        return wasFound;
    }

    ANSIStringView GetValueName(uint64 value) const override
    {
        Name memberName;

        if (EnumMemberName(EnumType(value), memberName))
        {
            return GetNameView(memberName);
        }

        return ANSIStringView();
    }
};

class ENGINE_API DeviceTierRegistry
{
public:
    static DeviceTierRegistry& GetInstance();

    void RegisterAxis(DeviceTierAxisBase* axis);
    void RegisterTier(DeviceTierBase* tier);

    const DeviceTierAxisBase* FindAxis(const ANSIStringView& axisName) const;
    const DeviceTierAxisBase* FindAxis(TypeId axisTypeId) const;

    const Array<DeviceTierAxisBase*>& GetAxes() const
    {
        return m_axes;
    }

    void Resolve(DeviceTierPhase phase, DeviceFacts& facts) const;

private:
    uint64 ResolveAxis(const DeviceTierAxisBase& axis, const DeviceFacts& facts, bool& outWasForced) const;

    Array<DeviceTierAxisBase*> m_axes;
    Array<DeviceTierBase*> m_tiers;
};

template <class TierClass>
struct DeviceTierRegistration
{
    DeviceTierRegistration()
    {
        static TierClass s_instance;

        DeviceTierRegistry::GetInstance().RegisterTier(&s_instance);
    }
};

template <class EnumType>
struct DeviceTierAxisRegistration
{
    DeviceTierAxisRegistration(const ANSIStringView& name, const UTF8StringView& commandLineArgumentName, DeviceTierPhase phase)
        : commandLineArgument(String(commandLineArgumentName), String::empty, "Forces the device tier of this axis, skipping detection", CommandLineArgumentFlags::NONE, CommandLineArgumentType::STRING)
    {
        static DeviceTierAxis<EnumType> s_axis(name, commandLineArgumentName, phase);

        DeviceTierRegistry::GetInstance().RegisterAxis(&s_axis);
    }

    CommandLineArgumentRegistration commandLineArgument;
};

#define HYP_REGISTER_DEVICE_TIER(TierClass) \
    static ::Hyperion::DeviceTierRegistration<TierClass> g_deviceTierRegistration_##TierClass;

#define HYP_DEFINE_DEVICE_TIER_AXIS(EnumType, Phase, CommandLineArgumentName) \
    static ::Hyperion::DeviceTierAxisRegistration<EnumType> g_deviceTierAxisRegistration_##EnumType(#EnumType, CommandLineArgumentName, Phase);

} // namespace Hyperion
