#pragma once

#include <Asset/AxImage.hpp>
#include <Core/FxPagedArray.hpp>
#include <Math/FxMat4.hpp>
#include <Math/FxQuat.hpp>
#include <Math/FxVec3.hpp>
#include <Renderer/FxCamera.hpp>
#include <Renderer/RxConstants.hpp>

#define FX_VALIDATE_ENTITY_TYPE(TType_) static_assert(C_IsEntity<TType_>);

class FxScene;

enum class FxEntityType
{
    eUnknown,
    eObject,
    eLight
};

enum class FxTransformMode
{
    eDefault,
    eTransformFromOrigin
};


class FxEntity
{
public:
    virtual void MoveTo(const FxVec3f& position)
    {
        mPosition = position;
        MarkTransformOutOfDate();
    }

    virtual void MoveBy(const FxVec3f& offset) { MoveTo(mPosition + offset); }

    virtual void Scale(const float scale);
    virtual void SetScale(const float scale);

    virtual void OnAttached(FxScene* scene) {}

    void RotateX(float32 rad);
    void RotateY(float32 rad);
    void RotateZ(float32 rad);

    void SetModelMatrix(const FxMat4f& other);

    FxMat4f& GetModelMatrix();
    // const FxMat4f& GetNormalMatrix();

    const FxVec3f& GetPosition() const { return mPosition; }

    FX_FORCE_INLINE void MarkTransformOutOfDate()
    {
        // Note that this is handled and acted on by FxObject
        mbPhysicsTransformOutOfDate = true;
        MarkMatrixOutOfDate();
    }

    FX_FORCE_INLINE void SetRotationOrigin(const FxVec3f& origin)
    {
        RotationOrigin = origin;
        TransformMode = FxTransformMode::eTransformFromOrigin;
        MarkMatrixOutOfDate();
    }

    void UpdateIfOutOfDate()
    {
        if (mbMatrixOutOfDate) {
            RecalculateModelMatrix();
        }

        SubmitMatrixIfNeeded();
    }

    FX_FORCE_INLINE void MarkMatrixOutOfDate() { mbMatrixOutOfDate = true; }

    virtual ~FxEntity();

protected:
    void RecalculateModelMatrix();

    /**
     * @brief Submit the matrix to the object gpu buffer if there have been changes, or if it has not been synched with
     * the current frame yet.
     */
    void SubmitMatrixIfNeeded();

public:
    uint32 ObjectId = UINT32_MAX;

    FxVec3f mPosition = FxVec3f::sZero;
    FxQuat mRotation = FxQuat::sIdentity;
    float32 mScale = 1.0f;

    FxVec3f RotationOrigin = FxVec3f::sZero;

    FxTransformMode TransformMode = FxTransformMode::eDefault;

protected:
    bool mbPhysicsTransformOutOfDate : 1 = true;
    bool mbMatrixOutOfDate : 1 = true;

    FxMat4f mModelMatrix = FxMat4f::sIdentity;

    uint32 mMatrixUpdateFramesRemaining = RxFramesInFlight;
    // FxMat4f mNormalMatrix = FxMat4f::Identity;
};

class FxEntityManager
{
public:
    static FxEntityManager& GetGlobalManager();

    void Create(uint32 entities_per_page = 64);

    static FxRef<FxEntity> New();

private:
    FxPagedArray<FxEntity> mEntityPool;
};


template <typename TEntityType>
concept C_IsEntity = requires() {
    // Ensure that there is a const(expr) scEntity type parameter
    requires std::is_same_v<const FxEntityType, decltype(TEntityType::scEntityType)>;

    // TEntity type is derived from FxEntity
    requires std::is_base_of_v<FxEntity, TEntityType>;
};
