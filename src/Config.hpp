/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_CONFIG_HPP
#define HYPERION_CONFIG_HPP

#include <core/utilities/Variant.hpp>
#include <core/utilities/EnumFlags.hpp>

#include <core/containers/HashMap.hpp>
#include <core/containers/FixedArray.hpp>
#include <core/containers/String.hpp>

#include <core/logging/LoggerFwd.hpp>

#include <core/Defines.hpp>

#include <util/ini/INIFile.hpp>

#include <Types.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Config);

class Engine;

enum OptionName
{
    CONFIG_NONE = 0,

    CONFIG_DEBUG_MODE,
    CONFIG_SHADER_COMPILATION,

    CONFIG_ENV_GRID_GI,
    CONFIG_ENV_GRID_GI_MODE,
    CONFIG_ENV_GRID_REFLECTIONS,
    
    CONFIG_HBAO,
    CONFIG_HBIL,

    CONFIG_TEMPORAL_AA,

    CONFIG_LIGHT_RAYS,

    CONFIG_DEBUG_SSR,
    CONFIG_DEBUG_HBAO,
    CONFIG_DEBUG_HBIL,
    CONFIG_DEBUG_REFLECTIONS,
    CONFIG_DEBUG_IRRADIANCE,
    CONFIG_DEBUG_REFLECTION_PROBES,
    CONFIG_DEBUG_ENV_GRID_PROBES,

    CONFIG_PATHTRACER,

    CONFIG_MAX
};

enum class OptionFlags : uint32
{
    NONE    = 0x0,
    SAVE    = 0x1,
};

HYP_MAKE_ENUM_FLAGS(OptionFlags)

class HYP_API Option : public Variant<bool, float, int, String>
{
    using Base = Variant<bool, float, int, String>;

    EnumFlags<OptionFlags>  m_flags;

public:
    Option()
        : Base(false),
          m_flags(OptionFlags::SAVE)
    {
    }

    Option(int int_value, EnumFlags<OptionFlags> flags = OptionFlags::SAVE)
        : Base(int_value),
          m_flags(flags)
    {
    }

    Option(float float_value, EnumFlags<OptionFlags> flags = OptionFlags::SAVE)
        : Base(float_value),
          m_flags(flags)
    {
    }

    Option(bool bool_value, EnumFlags<OptionFlags> flags = OptionFlags::SAVE)
        : Base(bool_value),
          m_flags(flags)
    {
    }

    Option(const String &string_value, EnumFlags<OptionFlags> flags = OptionFlags::SAVE)
        : Base(string_value),
          m_flags(flags)
    {
    }

    Option(const Option &other) = default;
    Option(Option &&other) noexcept = default;

    ~Option() = default;

    Option &operator=(const Option &other)
        { Base::operator=(other); return *this; }

    Option &operator=(Option &&other) noexcept
        { Base::operator=(std::move(other)); return *this; }

    Option operator|(const Option &other) const
        { return Option(GetInt() | other.GetInt()); }

    Option &operator|=(const Option &other)
    {
        if (auto *ptr = TryGet<int>()) {
            Base::Set(*ptr | other.GetInt());
        } else if (auto *ptr = TryGet<float>()) {
            Base::Set(static_cast<int>(*ptr) | other.GetInt());
        } else {
            Base::Set(GetBool() | other.GetBool());
        }

        return *this;
    }

    Option operator&(const Option &other) const
        { return Option(GetInt() & other.GetInt()); }

    Option &operator&=(const Option &other)
    {
        if (auto *ptr = TryGet<int>()) {
            Base::Set(*ptr & other.GetInt());
        } else if (auto *ptr = TryGet<float>()) {
            Base::Set(static_cast<int>(*ptr) & other.GetInt());
        } else {
            Base::Set(GetBool() & other.GetBool());
        }

        return *this;
    }

    HYP_FORCE_INLINE Option operator~() const
    {
        if (auto *ptr = TryGet<bool>()) {
            return Option(!*ptr);
        }

        return Option(~GetInt());
    }

    HYP_FORCE_INLINE Option operator!() const
        { return Option(!GetBool()); }

    HYP_FORCE_INLINE EnumFlags<OptionFlags> GetFlags() const
        { return m_flags; }

    explicit operator bool() const
        { return GetBool(); }

    HYP_FORCE_INLINE bool operator==(const Option &other) const
        { return Base::operator==(other); }

    HYP_FORCE_INLINE bool operator!=(const Option &other) const
        { return Base::operator!=(other); }

    int GetInt() const;
    float GetFloat() const;
    bool GetBool() const;
    String GetString() const;
};

class HYP_API Configuration
{
public:
    Configuration();
    ~Configuration() = default;

    HYP_FORCE_INLINE HYP_DEPRECATED Option &Get(OptionName option)
    {
        return m_variables[uint32(option)];
    }

    HYP_FORCE_INLINE HYP_DEPRECATED const Option &Get(OptionName option) const
    {
        return m_variables[uint32(option)];
    }

    bool LoadFromDefinitionsFile();
    bool SaveToDefinitionsFile();

    void SetToDefaultConfiguration();

    static OptionName StringToOptionName(const String &str);
    static String OptionNameToString(OptionName option);

private:
    FixedArray<Option, CONFIG_MAX>  m_variables;
};

} // namespace hyperion

#endif