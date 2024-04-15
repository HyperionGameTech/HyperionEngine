/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/backend/RendererDescriptorSet2.hpp>

#include <rendering/Buffers.hpp>

namespace hyperion {

using namespace v2;

namespace renderer {

DescriptorTableDeclaration *g_static_descriptor_table_decl = new DescriptorTableDeclaration();

DescriptorDeclaration *DescriptorSetDeclaration::FindDescriptorDeclaration(Name name) const
{
    for (uint slot_index = 0; slot_index < DESCRIPTOR_SLOT_MAX; slot_index++) {
        for (const DescriptorDeclaration &decl : slots[slot_index]) {
            if (decl.name == name) {
                return const_cast<DescriptorDeclaration *>(&decl);
            }
        }
    }

    return nullptr;
}

uint DescriptorSetDeclaration::CalculateFlatIndex(DescriptorSlot slot, Name name) const
{
    AssertThrow(slot != DESCRIPTOR_SLOT_NONE && slot < DESCRIPTOR_SLOT_MAX);

    uint flat_index = 0;

    for (uint slot_index = 0; slot_index < uint(slot); slot_index++) {
        if (slot_index == uint(slot) - 1) {
            uint decl_index = 0;

            for (const DescriptorDeclaration &decl : slots[slot_index]) {
                if (decl.name == name) {
                    return flat_index + decl_index;
                }

                decl_index++;
            }
        }

        flat_index += slots[slot_index].Size();
    }

    return uint(-1);
}

DescriptorSetDeclaration *DescriptorTableDeclaration::FindDescriptorSetDeclaration(Name name) const
{
    for (const DescriptorSetDeclaration &decl : m_elements) {
        if (decl.name == name) {
            return const_cast<DescriptorSetDeclaration *>(&decl);
        }
    }

    return nullptr;
}

DescriptorSetDeclaration *DescriptorTableDeclaration::AddDescriptorSetDeclaration(DescriptorSetDeclaration descriptor_set)
{
    m_elements.PushBack(std::move(descriptor_set));

    return &m_elements.Back();
}

static struct GlobalDescriptorSetsDeclarations
{
    GlobalDescriptorSetsDeclarations()
    {
        #define HYP_DESCRIPTOR_SETS_DEFINE
        #define HYP_DESCRIPTOR_SETS_GLOBAL_STATIC_DESCRIPTOR_TABLE g_static_descriptor_table_decl
        #include <rendering/inl/DescriptorSets.inl>
        #undef HYP_DESCRIPTOR_SETS_GLOBAL_STATIC_DESCRIPTOR_TABLE
        #undef HYP_DESCRIPTOR_SETS_DEFINE
    }
} s_global_descriptor_sets_declarations;

} // namespace renderer
} // namespace hyperion