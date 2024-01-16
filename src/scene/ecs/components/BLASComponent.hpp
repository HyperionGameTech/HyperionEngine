#ifndef HYPERION_V2_ECS_BLAS_COMPONENT_HPP
#define HYPERION_V2_ECS_BLAS_COMPONENT_HPP

#include <core/Handle.hpp>
#include <rendering/rt/BLAS.hpp>
#include <HashCode.hpp>

namespace hyperion::v2 {

struct BLASComponent
{
    Handle<BLAS>    blas;
    
    HashCode        transform_hash_code;
};

} // namespace hyperion::v2

#endif