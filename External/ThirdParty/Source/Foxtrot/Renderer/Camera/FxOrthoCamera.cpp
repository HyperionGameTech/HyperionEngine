#include <Core/FxPanic.hpp>
#include <Renderer/FxCamera.hpp>

void FxOrthoCamera::UpdateProjectionMatrix()
{
    ProjectionMatrix.LoadOrthographicMatrix(mWidth, mHeight, mNearPlane, mFarPlane);

    mbRequireMatrixUpdate = false;
}

void FxOrthoCamera::UpdateCameraMatrix()
{
    mCameraMatrix = ViewMatrix * ProjectionMatrix;

    InvViewMatrix = ViewMatrix.Inverse();
    InvProjectionMatrix = ProjectionMatrix.Inverse();
}

void FxOrthoCamera::ResolveViewToTexels(float32 texture_res)
{
    FxAssert(texture_res > 0.0f);

    const float32 texel_size = mWidth / texture_res;
    const float32 texel_size_recip = 1.0 / texel_size;

    float32 snapped_x = floor(Position.X * texel_size_recip) * texel_size;
    float32 snapped_y = floor(Position.Y * texel_size_recip) * texel_size;

    Position.X = snapped_x;
    Position.Y = snapped_y;
}
