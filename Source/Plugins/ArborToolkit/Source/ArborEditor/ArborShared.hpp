/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Containers/Array.hpp>
#include <Core/Containers/String.hpp>

#include <Core/Memory/ByteBuffer.hpp>

#include <Core/Types.hpp>

struct ArborRecipe;

namespace Hyperion {

enum class ArborKind : uint32
{
    Tree = 0, // or shrub, vine
    Cover,
    Rock,

    Max
};

enum class ArborControlKind : uint32
{
    Ranged = 0,
    Count,
    Choice,
    Toggle
};

struct ArborControlDesc
{
    String id;
    String section;
    String group;
    String label;
    String help;
    
    ArborControlKind kind = ArborControlKind::Ranged;

    float min = 0.0f;
    float max = 1.0f;
    
    bool log = false;
    
    uint32 countMin = 0;
    uint32 countMax = 0;

    Array<String> options;
    
    bool regrows = true;
};

struct ArborControlValue
{
    bool isRange = false;
    float lo = 0.0f;
    float hi = 0.0f;
    float landed = 0.0f;
    uint32 integer = 0;
};

class ArborRecipeHandle
{
public:
    static bool IsAvailable();
    static String GetLastError();

    static uint32 GetPresetCount(uint32 kind);
    static String GetPresetName(uint32 kind, uint32 index);

    static ArborRecipeHandle FromPreset(uint32 kind, const String& name);
    static ArborRecipeHandle FromRon(uint32 kind, const String& ron);

    ArborRecipeHandle() = default;
    ArborRecipeHandle(const ArborRecipeHandle& other) = delete;
    ArborRecipeHandle& operator=(const ArborRecipeHandle& other) = delete;
    ArborRecipeHandle(ArborRecipeHandle&& other) noexcept;
    ArborRecipeHandle& operator=(ArborRecipeHandle&& other) noexcept;
    ~ArborRecipeHandle();

    HYP_FORCE_INLINE bool IsValid() const
    {
        return m_recipe != nullptr;
    }

    ArborRecipeHandle Clone() const;

    String GetName() const;
    String ToRon() const;

    uint64 GetSeed() const;
    void SetSeed(uint64 seed);

    uint32 GetControlCount() const;
    bool GetControl(uint32 index, ArborControlDesc& outDesc) const;
    bool GetValue(uint32 index, ArborControlValue& outValue) const;
    bool SetValue(uint32 index, const ArborControlValue& value, bool& outReshaped);

    bool GenerateGlb(ByteBuffer& outGlb) const;

private:
    explicit ArborRecipeHandle(ArborRecipe* recipe)
        : m_recipe(recipe)
    {
    }

    ArborRecipe* m_recipe = nullptr;
};

} // namespace Hyperion
