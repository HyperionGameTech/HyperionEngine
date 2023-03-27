#ifndef HYPERION_V2_CONFIG_HPP
#define HYPERION_V2_CONFIG_HPP

#include <core/Containers.hpp>
#include <util/Defines.hpp>
#include <util/definitions/DefinitionsFile.hpp>
#include <Types.hpp>

namespace hyperion::v2 {

class Engine;

enum OptionName
{
    CONFIG_NONE = 0,

    CONFIG_DEBUG_MODE,
    CONFIG_SHADER_COMPILATION,

    CONFIG_RT_SUPPORTED,
    CONFIG_RT_ENABLED,
    CONFIG_RT_REFLECTIONS,
    CONFIG_RT_GI,

    CONFIG_RT_GI_DEBUG_PROBES,

    CONFIG_SSR,

    CONFIG_ENV_GRID_GI,
    CONFIG_ENV_GRID_REFLECTIONS,
    
    CONFIG_HBAO,
    CONFIG_HBIL,

    CONFIG_VOXEL_GI,
    CONFIG_VOXEL_GI_SVO,

    CONFIG_TEMPORAL_AA,

    CONFIG_LIGHT_RAYS,

    CONFIG_DEBUG_SSR,
    CONFIG_DEBUG_HBAO,
    CONFIG_DEBUG_HBIL,
    CONFIG_DEBUG_REFLECTIONS,
    CONFIG_DEBUG_IRRADIANCE,

    CONFIG_MAX
};

class Option : public Variant<Bool, Float, Int>
{
    using Base = Variant<Bool, Float, Int>;

    bool m_save = true;

public:
    Option()
        : Base(false)
    {
    }

    Option(Int int_value, bool save = true)
        : Base(int_value),
          m_save(save)
    {
    }

    Option(Float float_value, bool save = true)
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
        if (auto *ptr = TryGet<Int>()) {
            Base::Set(*ptr | other.GetInt());
        } else if (auto *ptr = TryGet<Float>()) {
            Base::Set(static_cast<Int>(*ptr) | other.GetInt());
        } else if (auto *ptr = TryGet<Bool>()) {
            Base::Set(*ptr | other.GetBool());
        }

        return *this;
    }

    Option operator&(const Option &other) const
        { return Option(GetInt() & other.GetInt()); }

    Option &operator&=(const Option &other)
    {
        if (auto *ptr = TryGet<Int>()) {
            Base::Set(*ptr & other.GetInt());
        } else if (auto *ptr = TryGet<Float>()) {
            Base::Set(static_cast<Int>(*ptr) & other.GetInt());
        } else if (auto *ptr = TryGet<Bool>()) {
            Base::Set(*ptr & other.GetBool());
        }

        return *this;
    }

    Option operator~() const
    {
        if (auto *ptr = TryGet<Bool>()) {
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

    Int GetInt() const
    {
        if (auto *ptr = TryGet<Int>()) {
            return *ptr;
        }

        if (auto *ptr = TryGet<Float>()) {
            return static_cast<Int>(*ptr);
        }

        if (auto *ptr = TryGet<Bool>()) {
            return static_cast<Int>(*ptr);
        }

        return 0;
    }

    Float GetFloat() const
    {
        if (auto *ptr = TryGet<Int>()) {
            return static_cast<Float>(*ptr);
        }

        if (auto *ptr = TryGet<Float>()) {
            return *ptr;
        }

        if (auto *ptr = TryGet<Bool>()) {
            return static_cast<Float>(*ptr);
        }

        return 0.0f;
    }

    Bool GetBool() const
    {
        if (auto *ptr = TryGet<Int>()) {
            return static_cast<Bool>(*ptr);
        }

        if (auto *ptr = TryGet<Float>()) {
            return static_cast<Bool>(*ptr);
        }

        if (auto *ptr = TryGet<Bool>()) {
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
        { return m_variables[UInt(option)]; }

    HYP_FORCE_INLINE const Option &Get(OptionName option) const
        { return m_variables[UInt(option)]; }

    bool LoadFromDefinitionsFile();
    bool SaveToDefinitionsFile();

    void SetToDefaultConfiguration();

    static OptionName StringToOptionName(const String &str);
    static String OptionNameToString(OptionName option);
    static bool IsRTOption(OptionName option);

private:
    FixedArray<Option, CONFIG_MAX> m_variables;
};

} // namespace hyperion::v2

#endif