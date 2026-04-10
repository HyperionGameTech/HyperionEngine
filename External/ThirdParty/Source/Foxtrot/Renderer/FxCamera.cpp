#include "FxCamera.hpp"

void FxCamera::Update()
{
    if (mbRequireMatrixUpdate) {
        UpdateProjectionMatrix();
    }

    if (!mbUpdateTransform) {
        return;
    }

    float32 s_anglex, c_anglex;
    float32 s_angley, c_angley;

    FxMath::SinCos(mAngleX, &s_anglex, &c_anglex);
    FxMath::SinCos(mAngleY, &s_angley, &c_angley);

    Direction.Set(c_angley * s_anglex, s_angley, c_angley * c_anglex);
    Direction.NormalizeIP();

    UpdateViewMatrix();

    mbUpdateTransform = false;
}


void FxCamera::UpdateViewMatrix()
{
    ViewMatrix.LookAt(Position, Position + Direction, FxVec3f::sUp);
    UpdateCameraMatrix();
}
