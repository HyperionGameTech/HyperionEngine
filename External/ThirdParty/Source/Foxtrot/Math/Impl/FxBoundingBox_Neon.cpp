#include <Math/FxBoundingBox.hpp>

FxBoundingBox::FxBoundingBox(FxVec3f min, FxVec3f max) : Min(min), Max(max) {}

FxBoundingBox& FxBoundingBox::operator+=(const FxBoundingBox& other)
{
    /*

            Min X          Max X
            |                |

    Max Y - +---------+ . . .
            |      +=========+
            |      |  |      |
            +------|--+      |
    Min Y -  . . . +=========+

     */


    Min = FxVec3f::Min(other.Min, Min);
    Max = FxVec3f::Max(other.Max, Max);

    return *this;
}


FxBoundingBox FxBoundingBox::operator+(const FxBoundingBox& other) const
{
    FxBoundingBox result = (*this);
    result += other;

    return result;
}


FxBoundingBox& FxBoundingBox::operator=(const FxBoundingBox& other)
{
    this->Min = other.Min;
    this->Max = other.Max;

    return *this;
}
