/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <UIPch.hpp>

#include <UI/UIText.hpp>
#include <UI/UIStage.hpp>

#include <Rendering/RenderInterface.hpp>
#include <Rendering/RenderCommand.hpp>
#include <Rendering/RenderConfig.hpp>
#include <Rendering/Frame.hpp>
#include <Rendering/GraphicsPipeline.hpp>
#include <Rendering/Texture.hpp>
#include <Rendering/InstancedMeshData.hpp>

#include <Asset/AssetRegistry.hpp>

#include <Scene/Camera/OrthoCamera.hpp>

#include <Scene/EntityManager.hpp>
#include <Scene/Components/MeshComponent.hpp>
#include <Scene/Components/BoundingBoxComponent.hpp>

#include <Core/Math/Vector3.hpp>
#include <Core/Math/Quat4f.hpp>

#include <Core/Threading/ThreadLocalStorage.hpp>

#include <Core/Memory/Allocator/ThreadAllocator.hpp>

#include <System/AppContext.hpp>

#include <Rendering/Util/MeshBuilder.hpp>

#include <Framework/EngineDriver.hpp>

#include <UIText.generated.inl>

namespace Hyperion {

ENGINE_API HYP_DECLARE_LOG_CHANNEL(UI);

struct FontAtlasCharacterIterator
{
    Vec2f placement;
    Vec2f atlasPixelSize;
    Vec2f cellDimensions;

    Vec2i charOffset;

    Vec2f glyphDimensions;
    Vec2f glyphScaling;

    float bearingY;
    float charWidth;
};

template <class Callback>
static void ForEachCharacter(
    const FontAtlas& fontAtlas,
    const String& text,
    const Vec2i& parentBounds,
    float textSize,
    Array<Vec2f>* outCharacterPlacements,
    Callback&& callback)
{
    HYP_SCOPE;

    static thread_local bool s_isInFunction = false;
    Assert(!s_isInFunction, "Re-entry detected");

    s_isInFunction = true;
    HYP_DEFER({ s_isInFunction = false; });

    Vec2f placement = Vec2f::Zero();

    const size_t length = text.Length();

    if (outCharacterPlacements)
    {
        outCharacterPlacements->Reserve(length + 1);
        outCharacterPlacements->PushBack(placement);
    }

    const Vec2f cellDimensions = Vec2f(fontAtlas.GetCellDimensions()) / 64.0f;

    const Handle<Texture>& mainTextureAtlas = fontAtlas.GetAtlasTextures().GetMainAtlas();

    if (!mainTextureAtlas.IsValid())
    {
        HYP_LOG_ONCE(UI, Warning, "No valid font atlas texture.");
        return;
    }

    Vec2f atlasPixelSize;

    if (mainTextureAtlas->GetExtent().Volume() != 0)
    {
        atlasPixelSize = Vec2f::One() / Vec2f(mainTextureAtlas->GetExtent().GetXY());
    }

    static thread_local Array<FontAtlasCharacterIterator>* s_currentWordChars = nullptr;

    if (HYP_UNLIKELY(!s_currentWordChars))
    {
        s_currentWordChars = new Array<FontAtlasCharacterIterator>;
        s_currentWordChars->Reserve(16);

        CurrentThreadObject()->AddOnExitCallback(
            []()
            {
                delete s_currentWordChars;
                s_currentWordChars = nullptr;
            });
    }

    s_currentWordChars->Reserve(static_cast<size_t>(text.Size() * 1.1));

    auto& currentWordChars = *s_currentWordChars;

    const auto iterateCurrentWord = [&currentWordChars, &callback]()
    {
        for (const FontAtlasCharacterIterator& characterIterator : currentWordChars)
        {
            callback(characterIterator);
        }

        currentWordChars.Clear();
    };

    for (size_t i = 0; i < length; i++)
    {
        const utf::Char32 ch = text.GetChar(i);

        if (ch == utf::Char32(' '))
        {
            if (currentWordChars.Any() && parentBounds.x != 0 && (currentWordChars.Back().placement.x + currentWordChars.Back().charWidth) * textSize >= float(parentBounds.x))
            {
                // newline
                placement.x = 0.0f;
                placement.y += cellDimensions.y;
            }
            else
            {
                // add room for space
                placement.x += cellDimensions.x * 0.5f;
            }

            if (outCharacterPlacements)
            {
                outCharacterPlacements->PushBack(placement);
            }

            iterateCurrentWord();

            continue;
        }

        if (ch == utf::Char32('\n'))
        {
            // reset placement, add room for newline
            placement.x = 0.0f;
            placement.y += cellDimensions.y;

            if (outCharacterPlacements)
            {
                outCharacterPlacements->PushBack(placement);
            }

            iterateCurrentWord();

            continue;
        }

        Optional<const GlyphMetrics&> glyphMetrics = fontAtlas.GetGlyphMetricsForChar(ch);

        if (!glyphMetrics.HasValue() || (glyphMetrics->width == 0 || glyphMetrics->height == 0))
        {
            HYP_LOG_ONCE(UI, Warning, "Ensure how to render char: {}", (int)ch);

            if (outCharacterPlacements)
            {
                outCharacterPlacements->PushBack(placement);
            }

            continue;
        }

        FontAtlasCharacterIterator& characterIterator = currentWordChars.EmplaceBack();
        characterIterator.placement = placement;
        characterIterator.atlasPixelSize = atlasPixelSize;
        characterIterator.cellDimensions = cellDimensions;
        characterIterator.charOffset = glyphMetrics->imagePosition;
        characterIterator.glyphDimensions = Vec2f(float(glyphMetrics->width), float(glyphMetrics->height)) / 64.0f;
        characterIterator.glyphScaling = Vec2f(characterIterator.glyphDimensions) / MathUtil::Min(Vec2f(cellDimensions), Vec2f(MathUtil::epsilonF));
        characterIterator.bearingY = float(glyphMetrics->height - glyphMetrics->bearingY) / 64.0f;
        characterIterator.charWidth = float(glyphMetrics->advance / 64) / 64.0f;

        placement.x += characterIterator.charWidth;

        if (outCharacterPlacements)
        {
            outCharacterPlacements->PushBack(placement);
        }
    }

    iterateCurrentWord();
}

static BoundingBox CalculateTextAABB(const FontAtlas& fontAtlas, const String& text, const Vec2i& parentBounds, float textSize, bool includeBearing, Array<Vec2f>* outCharacterPlacements = nullptr)
{
    HYP_SCOPE;

    BoundingBox aabb = BoundingBox::Zero();

    ForEachCharacter(fontAtlas, text, parentBounds, textSize, outCharacterPlacements, [includeBearing, &aabb](const FontAtlasCharacterIterator& iter)
                     {
                         BoundingBox characterAabb = BoundingBox::Zero();

                         if (includeBearing)
                         {
                             const float offsetY = (iter.cellDimensions.y - iter.glyphDimensions.y) + iter.bearingY;

                             characterAabb = characterAabb.Union(Vec3f(iter.placement.x, iter.placement.y + offsetY, 0.0f));
                             characterAabb = characterAabb.Union(Vec3f(iter.placement.x + iter.glyphDimensions.x, iter.placement.y + offsetY + iter.cellDimensions.y, 0.0f));
                         }
                         else
                         {
                             characterAabb = characterAabb.Union(Vec3f(iter.placement.x, iter.placement.y, 0.0f));
                             characterAabb = characterAabb.Union(Vec3f(iter.placement.x + iter.glyphDimensions.x, iter.placement.y + iter.cellDimensions.y, 0.0f));
                         }

                         aabb = aabb.Union(characterAabb);
                     });

    return aabb;
}

#pragma region UIText

UIText::UIText()
    : UIObject()
{
    OnComputedVisibilityChange
        .Bind(this, [this]()
              {
                  if (GetComputedVisibility())
                  {
                      UpdateSize(false);
                      UpdateMaterial(false);
                      UpdateMeshData(false);
                  }

                  return UIEventHandlerResult::OK;
              })
        .Detach();

    OnEnabled
        .Bind(this, [this]()
              {
                  UpdateMaterial(false);

                  return UIEventHandlerResult::OK;
              })
        .Detach();

    OnDisabled
        .Bind(this, [this]()
              {
                  UpdateMaterial(false);

                  return UIEventHandlerResult::OK;
              })
        .Detach();
}

UIText::~UIText()
{
}

void UIText::Init()
{
    HYP_SCOPE;

    UIObject::Init();
}

void UIText::SetText(const String& text)
{
    HYP_SCOPE;

    UIObject::SetText(text);

    if (!IsInitCalled())
    {
        return;
    }

    UpdateSize(false);
    UpdateMaterial(false);
    UpdateMeshData(false);
}

const Handle<FontAtlas>& UIText::GetFontAtlasOrDefault() const
{
    HYP_SCOPE;

    const UIStage* stage = GetStage();

    if (!stage)
    {
        return m_fontAtlas;
    }

    return m_fontAtlas != nullptr
        ? m_fontAtlas
        : stage->GetDefaultFontAtlas();
}

void UIText::SetFontAtlas(const Handle<FontAtlas>& fontAtlas)
{
    HYP_SCOPE;

    m_fontAtlas = fontAtlas;

    if (!IsInitCalled())
    {
        return;
    }

    UpdateSize(false);
    UpdateMaterial(false);
    UpdateMeshData(false);
}

Vec2f UIText::GetCharacterOffset(int characterIndex) const
{
    HYP_SCOPE;

    if (characterIndex < 0 || m_characterOffsets.Empty())
    {
        return Vec2f::Zero();
    }

    if (characterIndex >= int(m_characterOffsets.Size()))
    {
        characterIndex = int(m_characterOffsets.Size()) - 1;
    }

    return m_characterOffsets[characterIndex] * GetTextSize();
}

void UIText::UpdateTextAABB()
{
    HYP_SCOPE;

    if (const Handle<FontAtlas>& fontAtlas = GetFontAtlasOrDefault())
    {
        const Vec2i parentBounds = GetParentBounds();
        const float textSize = GetTextSize();

        m_characterOffsets.Clear();

        m_textAabbWithBearing = CalculateTextAABB(*fontAtlas, m_text, parentBounds, textSize, true, nullptr);
        m_textAabbWithoutBearing = CalculateTextAABB(*fontAtlas, m_text, parentBounds, textSize, false, &m_characterOffsets);
    }
    else
    {
        HYP_LOG_ONCE(UI, Verbose, "No font atlas set for UIText {} (text: \"{}\")", GetName(), GetText());
    }
}

void UIText::UpdateMeshData_Internal()
{
    HYP_SCOPE;

    UIObject::UpdateMeshData_Internal();

    const Handle<FontAtlas>& fontAtlas = GetFontAtlasOrDefault();

    if (!fontAtlas)
    {
        HYP_LOG_ONCE(UI, Verbose, "No font atlas, cannot update text mesh data yet");

        return;
    }

    BoundingBox parentAabbClamped;

    if (UIObject* parent = GetParentUIObject())
    {
        parentAabbClamped = parent->GetAABBClamped();
    }

    MeshComponent& meshComponent = GetScene()->GetEntityManager()->GetComponent<MeshComponent>(GetEntity());

    const Vec2f position = GetAbsolutePosition();
    const float textSize = GetTextSize();

    Array<Mat4f, ThreadAllocator> instanceTransforms;
    Array<Vec4f, ThreadAllocator> instanceTexcoords;
    Array<Vec4f, ThreadAllocator> instanceOffsets;
    Array<Vec4f, ThreadAllocator> instanceSizes;
    Array<Vec4u, ThreadAllocator> instanceProperties;

    ForEachCharacter(*fontAtlas, m_text, GetParentBounds(), textSize, nullptr, [&](const FontAtlasCharacterIterator& iter)
                     {
                         Transform characterTransform;
                         characterTransform.scale = Vec3f(iter.glyphDimensions.x * textSize, iter.glyphDimensions.y * textSize, 1.0f);
                         characterTransform.translation.y += (iter.cellDimensions.y - iter.glyphDimensions.y) * textSize;
                         characterTransform.translation.y += iter.bearingY * textSize;
                         characterTransform.translation += Vec3f(iter.placement.x, iter.placement.y, 0.0f) * textSize;

                         BoundingBox characterAabb = characterTransform * BoundingBox(Vec3f::Zero(), Vec3f::One());
                         characterAabb.min += Vec3f(position, 0.0f);
                         characterAabb.max += Vec3f(position, 0.0f);

                         BoundingBox characterAabbClamped = characterAabb.Intersection(parentAabbClamped);

                         Mat4f instanceTransform;
                         instanceTransform[0][0] = characterAabbClamped.max.x - characterAabbClamped.min.x;
                         instanceTransform[1][1] = characterAabbClamped.max.y - characterAabbClamped.min.y;
                         instanceTransform[2][2] = 1.0f;
                         instanceTransform[0][3] = characterAabbClamped.min.x;
                         instanceTransform[1][3] = characterAabbClamped.min.y;
                         instanceTransform[2][3] = 0.0f;

                         instanceTransforms.PushBack(instanceTransform);

                         const Vec2f size = characterAabb.GetExtent().GetXY();
                         const Vec2f clampedSize = characterAabbClamped.GetExtent().GetXY();
                         const Vec2f clampedOffset = characterAabb.min.GetXY() - characterAabbClamped.min.GetXY();

                         Vec2f texcoordStart = Vec2f(iter.charOffset) * iter.atlasPixelSize;
                         Vec2f texcoordEnd = (Vec2f(iter.charOffset) + (iter.glyphDimensions * 64.0f)) * iter.atlasPixelSize;

                         instanceTexcoords.PushBack(Vec4f(texcoordStart, texcoordEnd));
                         instanceOffsets.PushBack(Vec4f(clampedOffset, 0.0f, 0.0f));
                         instanceSizes.PushBack(Vec4f(size, clampedSize));
                     });

    instanceProperties.Resize(instanceTransforms.Size());

    meshComponent.numInstances = uint32(instanceTransforms.Size());

    Handle<InstancedMeshData> instancedMesh = DynamicCast<InstancedMeshData>(meshComponent.instanceData.Resolve());

    if (!instancedMesh)
    {
        instancedMesh = MakeHandle<InstancedMeshData>(NAME_FMT("IMD_{}_{}", InstanceClass()->GetName(), GetName()));
        instancedMesh->SetIsTransient(true);

        GetEngineAssetRegistry()->PutAssetUnique(instancedMesh);

        Assert(instancedMesh->IsRegistered());

        meshComponent.instanceData = AssetReference(instancedMesh);
    }

    // Only update the buffer data if we have any data -- otherwise, setting data to 0 will free all data
    // and force us to realloc a lot which can chew up a lot of resources doing that at a high frequency
    if (instanceTransforms.Any())
    {
        auto writeScope = instancedMesh->GetWriteScope();

        instancedMesh->SetBufferData(0, instanceTransforms.Data(), instanceTransforms.Size());
        instancedMesh->SetBufferData(1, instanceTexcoords.Data(), instanceTexcoords.Size());
        instancedMesh->SetBufferData(2, instanceOffsets.Data(), instanceOffsets.Size());
        instancedMesh->SetBufferData(3, instanceSizes.Data(), instanceSizes.Size());
        instancedMesh->SetBufferData(4, instanceProperties.Data(), instanceProperties.Size());
    }

    GetEntity()->SetNeedsRenderProxyUpdate();
}

void UIText::UpdateMaterial_Internal()
{
    HYP_SCOPE;

    if (const Handle<FontAtlas>& fontAtlas = GetFontAtlasOrDefault())
    {
        const float textSize = GetTextSize();

        m_currentFontAtlasTexture = fontAtlas->GetAtlasTextures().GetAtlasForPixelSize(textSize);
    }

    if (!m_currentFontAtlasTexture.IsValid())
    {
        HYP_LOG_ONCE(UI, Warning, "No font atlas texture for text \"{}\"", GetText());
    }

    UIObject::UpdateMaterial_Internal();
}

MaterialAttributes UIText::GetMaterialAttributes() const
{
    return UIObject::GetMaterialAttributes();
}

MaterialParameters UIText::GetMaterialParameters() const
{
    Color color = GetTextColor();

    if (!IsEnabled())
    {
        color.a *= 0.5f;
    }

    MaterialParameters parameters;
    parameters.albedo = Vec4f(color);
    parameters.userParams.x = 1.0f; // text param

    return parameters;
}

MaterialTextures UIText::GetMaterialTextures() const
{
    if (!m_currentFontAtlasTexture.IsValid())
    {
        return UIObject::GetMaterialTextures();
    }

    return {
        { MaterialTextureKey::Diffuse, m_currentFontAtlasTexture }
    };
}

void UIText::Update_Internal(float delta)
{
    HYP_SCOPE;

    UIObject::Update_Internal(delta);
}

void UIText::UpdateSize_Internal(bool updateChildren)
{
    HYP_SCOPE;

    UpdateTextAABB();

    UIObject::UpdateSize_Internal(updateChildren);

    const Vec2f extentWithBearing = m_textAabbWithBearing.GetExtent().GetXY();
    const Vec2f extentWithoutBearing = m_textAabbWithoutBearing.GetExtent().GetXY();

    if (extentWithBearing.y <= MathUtil::epsilonF || extentWithoutBearing.y <= MathUtil::epsilonF)
    {
        HYP_LOG_ONCE(UI, Warning, "Text AABB has zero height, cannot update size for UIText {} (text: \"{}\")\tExtent with bearing: {}\tExtent without bearing: {}",
                     GetName(), GetText(), extentWithBearing, extentWithoutBearing);

        return;
    }

    m_actualSize.y *= extentWithBearing.y / extentWithoutBearing.y;
}

BoundingBox UIText::CalculateInnerAABB_Internal() const
{
    float textSize = GetTextSize();

    if (textSize <= FLT_EPSILON)
    {
        HYP_LOG_ONCE(UI, Warning, "Invalid text size for text element {} (text: \"{}\"): {}", GetName(), GetText(), textSize);
    }

    return m_textAabbWithoutBearing * Vec3f(Vec2f(textSize), 1.0f);
}

void UIText::OnFontAtlasUpdate_Internal()
{
    HYP_SCOPE;

    UIObject::OnFontAtlasUpdate_Internal();

    UpdateSize(false);
    UpdateMaterial(false);
    UpdateMeshData(false);
}

void UIText::OnTextSizeUpdate_Internal()
{
    HYP_SCOPE;

    UIObject::OnTextSizeUpdate_Internal();

    UpdateSize(false);
    UpdateMaterial(false);
    UpdateMeshData(false);
}

Vec2i UIText::GetParentBounds() const
{
    Vec2i parentBounds = Vec2i::Zero();

    if (const UIObject* parent = GetParentUIObject())
    {
        const UIObjectSize parentSize = parent->GetSize();

        if (!(parentSize.GetFlagsX() & UIObjectSize::AUTO))
        {
            parentBounds.x = parent->GetActualSize().x;
        }

        if (!(parentSize.GetFlagsY() & UIObjectSize::AUTO))
        {
            parentBounds.y = parent->GetActualSize().y;
        }
    }

    return parentBounds;
}

#pragma endregion UIText

} // namespace Hyperion
