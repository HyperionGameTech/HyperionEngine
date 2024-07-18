/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_CONFIG_HPP
#define HYPERION_CONFIG_HPP

#include <core/utilities/Variant.hpp>

#include <core/containers/FlatMap.hpp>
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
    
    CONFIG_RT_ENABLED,
    CONFIG_RT_REFLECTIONS,
    CONFIG_RT_GI,

    CONFIG_RT_GI_DEBUG_PROBES,

    CONFIG_SSR,

    CONFIG_ENV_GRID_GI,
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

class Option : public Variant<bool, float, int>
{
    using Base = Variant<bool, float, int>;

    bool m_save = true;

public:
    Option()
        : Base(false)
    {
    }

    Option(int int_value, bool save = true)
        : Base(int_value),
          m_save(save)
    {
    }

    Option(float float_value, bool save = true)
        : Base(float_value),
          m_save(save)
    {
    }

    Option(bool bool_value, bool save = true)
        : Base(bool_value),
          m_save(save)
    {
    }

    Option(const Option &other) = default;
    Option(Option &&other) noexcept = default;

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
        } else if (auto *ptr = TryGet<bool>()) {
            Base::Set(*ptr | other.GetBool());
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
        } else if (auto *ptr = TryGet<bool>()) {
            Base::Set(*ptr & other.GetBool());
        }

        return *this;
    }

    Option operator~() const
    {
        if (auto *ptr = TryGet<bool>()) {
            return Option(!*ptr);
        }

        return Option(~GetInt());
    }

    Option operator!() const
        { return Option(!GetBool()); }

    ~Option() = default;

    operator bool() const
        { return GetBool(); }

    bool operator==(const Option &other) const
        { return Base::operator==(other); }

    bool operator!=(const Option &other) const
        { return Base::operator!=(other); }

    int GetInt() const
    {
        if (auto *ptr = TryGet<int>()) {
            return *ptr;
        }

        if (auto *ptr = TryGet<float>()) {
            return static_cast<int>(*ptr);
        }

        if (auto *ptr = TryGet<bool>()) {
            return static_cast<int>(*ptr);
        }

        return 0;
    }

    float GetFloat() const
    {
        if (auto *ptr = TryGet<int>()) {
            return static_cast<float>(*ptr);
        }

        if (auto *ptr = TryGet<float>()) {
            return *ptr;
        }

        if (auto *ptr = TryGet<bool>()) {
            return static_cast<float>(*ptr);
        }

        return 0.0f;
    }

    bool GetBool() const
    {
        if (auto *ptr = TryGet<int>()) {
            return static_cast<bool>(*ptr);
        }

        if (auto *ptr = TryGet<float>()) {
            return static_cast<bool>(*ptr);
        }

        if (auto *ptr = TryGet<bool>()) {
            return *ptr;
        }

        return false;
    }

    bool GetIsSaved() const
        { return m_save; }

    void SetIsSaved(bool save)
        { m_save = save; }
};

class Configuration
{
    static const FlatMap<OptionName, String> option_name_strings;

public:
    Configuration();
    ~Configuration() = default;

    HYP_FORCE_INLINE Option &Get(OptionName option)
    {
        return m_variables[uint(option)];
    }

    HYP_FORCE_INLINE const Option &Get(OptionName option) const
    {
        return m_variables[uint(option)];
    }

    // HYP_FORCE_INLINE Option &Get(OptionName option, ThreadType thread_type)
    // {
    //     return m_variables[uint(option)][thread_type];
    // }

    // HYP_FORCE_INLINE const Option &Get(OptionName option, ThreadType thread_type) const
    // {
    //     return m_variables[uint(option)][thread_type];
    // }

    // HYP_FORCE_INLINE Option &Get(OptionName option)
    // {
    //     ThreadType thread_type = Threads::GetThreadType();

    //     if (thread_type == THREAD_TYPE_INVALID) {
    //         thread_type = THREAD_TYPE_GAME;
    //     }

    //     return m_variables[uint(option)][thread_type];
    // }

    // HYP_FORCE_INLINE const Option &Get(OptionName option) const
    // {
    //     return const_cast<Configuration *>(this)->Get(option);
    // }

    bool LoadFromDefinitionsFile();
    bool SaveToDefinitionsFile();

    void SetToDefaultConfiguration();

    static OptionName StringToOptionName(const String &str);
    static String OptionNameToString(OptionName option);
    static bool IsRTOption(OptionName option);

private:
    FixedArray<Option, CONFIG_MAX> m_variables;
};

} // namespace hyperion

#endif