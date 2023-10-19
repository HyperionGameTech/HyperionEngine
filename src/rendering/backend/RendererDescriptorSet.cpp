#include <rendering/backend/RendererDescriptorSet.hpp>
#include <rendering/backend/RendererInstance.hpp>

namespace hyperion {
namespace renderer {

DescriptorTable *g_static_descriptor_table = new DescriptorTable();

DescriptorDeclaration *DescriptorSetDeclaration::FindDescriptorDeclaration(Name name) const
{
    for (UInt slot_index = 0; slot_index < DESCRIPTOR_SLOT_MAX; slot_index++) {
        for (const DescriptorDeclaration &decl : slots[slot_index]) {
            if (decl.name == name) {
                return const_cast<DescriptorDeclaration *>(&decl);
            }
        }
    }

    return nullptr;
}

UInt DescriptorSetDeclaration::CalculateFlatIndex(DescriptorSlot slot, Name name) const
{
    AssertThrow(slot != DESCRIPTOR_SLOT_NONE && slot < DESCRIPTOR_SLOT_MAX);

    UInt flat_index = 0;

    for (UInt slot_index = 0; slot_index < UInt(slot); slot_index++) {
        if (slot_index == UInt(slot) - 1) {
            UInt decl_index = 0;

            for (const DescriptorDeclaration &decl : slots[slot_index]) {
                if (decl.name == name) {
                    return flat_index + decl_index;
                }

                decl_index++;
            }
        }

        flat_index += slots[slot_index].Size();
    }

    return UInt(-1);
}

DescriptorSetDeclaration *DescriptorTable::FindDescriptorSetDeclaration(Name name) const
{
    for (const DescriptorSetDeclaration &decl : declarations) {
        if (decl.name == name) {
            return const_cast<DescriptorSetDeclaration *>(&decl);
        }
    }

    return nullptr;
}

DescriptorSetDeclaration *DescriptorTable::AddDescriptorSet(DescriptorSetDeclaration descriptor_set)
{
    declarations.PushBack(std::move(descriptor_set));

    return &declarations.Back();
}

Array<DescriptorSetRef> GlobalDescriptorManager::GetOrCreateDescriptorSets(
    Instance *instance,
    Device *device,
    const DescriptorTable &table
)
{
    Array<DescriptorSetRef> results;
    results.Reserve(table.GetDescriptorSetDeclarations().Size());

    Mutex::Guard guard(m_mutex);

    for (const DescriptorSetDeclaration &decl : table.GetDescriptorSetDeclarations()) {
        const auto it = m_weak_refs.Find(decl.name);

        DescriptorSetRef descriptor_set_ref;

        if (it != m_weak_refs.End()) {
            descriptor_set_ref = it->second.Lock();

            if (descriptor_set_ref) {
                results.PushBack(std::move(descriptor_set_ref));
                continue;
            }
        }

        descriptor_set_ref = MakeRenderObject<DescriptorSet>(decl);
        m_weak_refs.Set(decl.name, descriptor_set_ref);

        DeferCreate(descriptor_set_ref, device, &instance->GetDescriptorPool());

        // HYPERION_ASSERT_RESULT(descriptor_set_ref->Create(device, &instance->GetDescriptorPool()));

        results.PushBack(std::move(descriptor_set_ref));
    }

    return results;
}

#define HYP_DESCRIPTOR_SETS_DEFINE
#define HYP_DESCRIPTOR_SETS_GLOBAL_STATIC_DESCRIPTOR_TABLE g_static_descriptor_table
#include <rendering/inl/DescriptorSets.inl>
#undef HYP_DESCRIPTOR_SETS_GLOBAL_STATIC_DESCRIPTOR_TABLE
#undef HYP_DESCRIPTOR_SETS_DEFINE

} // namespace renderer
} // namespace hyperion