#include <Renderer/FxCamera.hpp>

void FxPerspectiveCamera::UpdateProjectionMatrix()
{
    ProjectionMatrix.LoadPerspectiveMatrix(mFovRad, mAspectRatio, mNearPlane, mFarPlane);
    mWeaponProjectionMatrix.LoadPerspectiveMatrix(mWeaponFov, mAspectRatio, scWeaponNearPlane, scWeaponFarPlane);

    mbRequireMatrixUpdate = false;
}

void FxPerspectiveCamera::UpdateCameraMatrix()
{
    mCameraMatrix = ViewMatrix * ProjectionMatrix;
    mWeaponCameraMatrix = ViewMatrix * mWeaponProjectionMatrix;

    InvViewMatrix = ViewMatrix.Inverse();
    InvProjectionMatrix = ProjectionMatrix.Inverse();
}
