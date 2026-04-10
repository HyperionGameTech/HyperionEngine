#pragma once

#include <FxObjectLayer.hpp>
#include <Math/FxMat4.hpp>
#include <Math/FxMathConsts.hpp>
#include <Math/FxMathUtil.hpp>
#include <Math/FxQuat.hpp>
#include <Math/FxVec2.hpp>
#include <Math/FxVec3.hpp>

enum class FxCameraType
{
    eNone,
    eOrthographic,
    ePerspective,
};

class FxCamera
{
public:
    void Update();

    FX_FORCE_INLINE FxVec3f GetForwardVector() { return Direction; }
    FX_FORCE_INLINE FxVec3f GetRightVector() { return GetForwardVector().Cross(FxVec3f::sUp).Normalize(); }
    FX_FORCE_INLINE FxVec3f GetUpVector() { return GetRightVector().Cross(Direction).Normalize(); }

    virtual const FxMat4f& GetCameraMatrix(FxObjectLayer layer) const { return mCameraMatrix; }

    FX_FORCE_INLINE void SetNearPlane(float32 near)
    {
        RequireMatrixUpdate();
        mNearPlane = near;
    }

    FX_FORCE_INLINE void SetFarPlane(float32 far)
    {
        RequireMatrixUpdate();
        mFarPlane = far;
    }

    FX_FORCE_INLINE void MoveBy(const FxVec3f& offset)
    {
        Position += offset;
        RequireTransformUpdate();
    }

    FX_FORCE_INLINE void MoveTo(const FxVec3f& position)
    {
        Position = position;
        RequireTransformUpdate();
    }


    inline void RequireTransformUpdate() { mbUpdateTransform = true; }

    /**
     * @brief Notify that values for the projection matrix have changed and the matrix is out of date.
     * This is used to defer recalculating the matrix until all modifications have been made.
     */
    inline void RequireMatrixUpdate() { mbRequireMatrixUpdate = true; }

    virtual ~FxCamera() {}

private:
    void UpdateViewMatrix();


protected:
    virtual void UpdateProjectionMatrix() = 0;

    /**
     * @brief Updates and calculates the final matrix to be presented to the renderer.
     */
    virtual void UpdateCameraMatrix() = 0;

public:
    FxMat4f ViewMatrix = FxMat4f::sIdentity;
    FxMat4f ProjectionMatrix = FxMat4f::sIdentity;

    FxMat4f InvViewMatrix = FxMat4f::sIdentity;
    FxMat4f InvProjectionMatrix = FxMat4f::sIdentity;

    FxVec3f Position = FxVec3f::sZero;
    FxVec3f Direction = FxVec3f::sForward;

    float mAngleX = 0.0f;
    float mAngleY = 0.0f;

    bool mbUpdateTransform : 1 = true;
    bool mbRequireMatrixUpdate : 1 = true;

    float32 mNearPlane = 1000.0f;
    float32 mFarPlane = 0.01f;

protected:
    FxMat4f mCameraMatrix = FxMat4f::sIdentity;
};

///////////////////////////////////////////////
// Orthographic Camera
///////////////////////////////////////////////

class FxOrthoCamera final : public FxCamera
{
public:
    static constexpr FxCameraType scType = FxCameraType::eOrthographic;

public:
    FxOrthoCamera() { Update(); }

    void UpdateProjectionMatrix() override;
    void UpdateCameraMatrix() override;

    void ResolveViewToTexels(float32 texture_res);

    FX_FORCE_INLINE void SetBounds(float32 width, float32 height)
    {
        mWidth = width;
        mHeight = height;

        RequireMatrixUpdate();
    }

    ~FxOrthoCamera() override {}

private:
    float32 mWidth = 50.0f;
    float32 mHeight = 50.0f;
};

///////////////////////////////////////////////
// Perspective Camera
///////////////////////////////////////////////

class FxPerspectiveCamera final : public FxCamera
{
public:
    static constexpr FxCameraType scType = FxCameraType::ePerspective;

    static constexpr float32 scWeaponFarPlane = 0.01f;
    static constexpr float32 scWeaponNearPlane = 20.0f;

public:
    FxPerspectiveCamera() { Update(); }

    FxPerspectiveCamera(float32 fov_degrees, float32 aspect_ratio, float32 near_plane, float32 far_plane)
    {
        mNearPlane = near_plane;
        mFarPlane = far_plane;
        SetFov(fov_degrees);
        SetAspectRatio(aspect_ratio);

        Update();
    }

    void UpdateProjectionMatrix() override;
    void UpdateCameraMatrix() override;

    float32 GetFov() const { return FxRadToDeg(mFovRad); }
    float32 GetFovRad() const { return mFovRad; }

    void SetFov(float32 fov) { SetFovRad(FxDegToRad(fov)); }

    void SetFovRad(float32 fov_rad)
    {
        mFovRad = fov_rad;

        RequireMatrixUpdate();
    }

    inline void Rotate(float32 angle_x, float32 angle_y)
    {
        mAngleX = LimitRotation(mAngleX + angle_x);
        mAngleY = LimitRotation(mAngleY + angle_y);

        constexpr float cOffset = 0.01f;

        constexpr float cMinY = -FX_HALF_PI + cOffset;
        constexpr float cMaxY = FX_HALF_PI - cOffset;

        mAngleY = FxMath::Clamp(mAngleY, cMinY, cMaxY);

        RequireTransformUpdate();
    }


    FX_FORCE_INLINE void SetAspectRatio(float32 aspect_ratio)
    {
        mAspectRatio = aspect_ratio;

        RequireMatrixUpdate();
    }

    FX_FORCE_INLINE FxVec3f GetRotation() { return FxVec3f(mAngleY, mAngleX, 0); }

    const FxMat4f& GetCameraMatrix(FxObjectLayer layer) const override
    {
        switch (layer) {
        case FxObjectLayer::eWorldLayer:
            return mCameraMatrix;
        case FxObjectLayer::ePlayerLayer:
            return mWeaponCameraMatrix;
        }

        return mCameraMatrix;
    }

    ~FxPerspectiveCamera() override {}

private:
    inline float32 LimitRotation(float32 v)
    {
        constexpr float32 cf2Pi = FX_2PI;

        if (v < -cf2Pi) {
            return cf2Pi - 1e-5;
        }
        else if (v > cf2Pi) {
            return -cf2Pi + 1e-5;
        }
        return v;
    }

private:
    float32 mFovRad = FxDegToRad(80.0f);

    float32 mAspectRatio = 1.0f;

    /**
     * @brief A separate matrix for objects rendered from the perspective of the player.
     * Weapons, body, or additional attached objects use this.
     */
    FxMat4f mWeaponCameraMatrix = FxMat4f::sIdentity;
    FxMat4f mWeaponProjectionMatrix = FxMat4f::sIdentity;

    float32 mWeaponFov = FxDegToRad(75.0f);
};
