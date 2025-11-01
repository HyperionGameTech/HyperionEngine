#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>

namespace hyperion {

#pragma region StoreOperation Reflection Data

HYP_BEGIN_ENUM(StoreOperation, 279, 0, {})
    HypConstant(NAME(HYP_STR(UNDEFINED)), StoreOperation::UNDEFINED),
    HypConstant(NAME(HYP_STR(NONE)), StoreOperation::NONE),
    HypConstant(NAME(HYP_STR(STORE)), StoreOperation::STORE)
HYP_END_ENUM

#pragma endregion StoreOperation Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region LoadOperation Reflection Data

HYP_BEGIN_ENUM(LoadOperation, 280, 0, {})
    HypConstant(NAME(HYP_STR(UNDEFINED)), LoadOperation::UNDEFINED),
    HypConstant(NAME(HYP_STR(NONE)), LoadOperation::NONE),
    HypConstant(NAME(HYP_STR(CLEAR)), LoadOperation::CLEAR),
    HypConstant(NAME(HYP_STR(LOAD)), LoadOperation::LOAD)
HYP_END_ENUM

#pragma endregion LoadOperation Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region AttachmentBase Reflection Data

HYP_BEGIN_CLASS(AttachmentBase, 80, 1, NAME("HypObjectBase"), HypClassAttribute("abstract", true),HypClassAttribute("noscriptbindings", true))
HYP_END_CLASS

#pragma endregion AttachmentBase Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region RenderPassStage Reflection Data

HYP_BEGIN_ENUM(RenderPassStage, 281, 0, {})
    HypConstant(NAME(HYP_STR(NONE)), RenderPassStage::NONE),
    HypConstant(NAME(HYP_STR(PRESENT)), RenderPassStage::PRESENT),
    HypConstant(NAME(HYP_STR(SHADER)), RenderPassStage::SHADER)
HYP_END_ENUM

#pragma endregion RenderPassStage Reflection Data

} // namespace hyperion

