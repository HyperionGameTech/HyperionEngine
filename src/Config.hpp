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

    CONFIG_SSR,
    
    CONFIG_HBAO,
    CONFIG_HBIL,

    CONFIG_VOXEL_GI,

    CONFIG_MAX
};

class Option : public Variant<bool, Float, Int>
{
    using Base = Variant<bool, Float, Int>;

public:
    Option()
        : Base(false)
    {
    }

    Option(Int int_value)
        : Base(int_value)
    {
    }

    Option(Float float_value)
        : Base(float_value)
    {
    }

    Option(bool bool_value)
        : Base(bool_value)
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
        } else if (auto *ptr = TryGet<bool>()) {
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

    Int GetInt() const
    {
        if (auto *ptr = TryGet<Int>()) {
            return *ptr;
        }

        if (auto *ptr = TryGet<Float>()) {
            return static_cast<Int>(*ptr);
        }

        if (auto *ptr = TryGet<bool>()) {
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

        if (auto *ptr = TryGet<bool>()) {
            return static_cast<Float>(*ptr);
        }

        return 0.0f;
    }

    bool GetBool() const
    {
        if (auto *ptr = TryGet<Int>()) {
            return static_cast<bool>(*ptr);
        }

        if (auto *ptr = TryGet<Float>()) {
            return static_cast<bool>(*ptr);
        }

        if (auto *ptr = TryGet<bool>()) {
            return *ptr;
        }

        return false;
    }
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

    bool LoadFromDefinitionsFile(Engine *engine);
    bool SaveToDefinitionsFile(Engine *engine);

    void SetToDefaultConfiguration(Engine *engine);

    static OptionName StringToOptionName(const String &str);
    static String OptionNameToString(OptionName option);
    static bool IsRTOption(OptionName option);

private:
    FixedArray<Option, CONFIG_MAX> m_variables;
};

} // namespace hyperion::v2

#endif