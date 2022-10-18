#ifndef HYPERION_V2_CONFIG_HPP
#define HYPERION_V2_CONFIG_HPP

#include <core/Containers.hpp>
#include <util/Defines.hpp>
#include <Types.hpp>

namespace hyperion::v2 {

class Engine;

enum OptionName
{
    CONFIG_NONE = 0,

    CONFIG_RT_SUPPORTED,
    CONFIG_RT_ENABLED,
    CONFIG_RT_REFLECTIONS,
    CONFIG_RT_GI,

    CONFIG_SSR,

    CONFIG_VOXEL_GI,

    CONFIG_MAX
};

class Configuration
{
public:
    class Option : Variant<bool, Float, Int>
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
            if (auto *ptr = TryGet<Int>()) {
                return Option(~*ptr);
            } else if (auto *ptr = TryGet<Float>()) {
                return Option(~static_cast<Int>(*ptr));
            } else if (auto *ptr = TryGet<bool>()) {
                return Option(~*ptr);
            } else {
                return Option(1);
            }
        }

        ~Option() = default;

        operator bool() const
            { return GetBool(); }

        bool operator!() const
            { return !GetBool(); }

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

    Configuration();
    ~Configuration() = default;

    HYP_FORCE_INLINE Option &Get(OptionName name)
        { return m_variables[static_cast<UInt>(name)]; }

    HYP_FORCE_INLINE const Option &Get(OptionName name) const
        { return m_variables[static_cast<UInt>(name)]; }

    void SetToDefaultConfiguration(Engine *engine);

private:
    FixedArray<Option, CONFIG_MAX> m_variables;
};

} // namespace hyperion::v2

#endif