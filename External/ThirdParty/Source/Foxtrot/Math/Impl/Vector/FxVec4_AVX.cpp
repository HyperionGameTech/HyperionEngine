#include <Core/FxDefines.hpp>

#ifdef FX_USE_AVX

#include <Math/FxSSE.hpp>
#include <Math/FxVec4.hpp>


const FxVec4f FxVec4f::sZero = FxVec4f(0.0f, 0.0f, 0.0f, 0.0f);
const FxVec4f FxVec4f::sOne = FxVec4f(1.0f, 1.0f, 1.0f, 1.0f);

#endif // #ifdef FX_USE_AVX
