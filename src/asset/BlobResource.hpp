/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/memory/resource/Resource.hpp>

#include <core/io/MemoryMappedFile.hpp>

#include <asset/BlobStorageStructs.hpp>

namespace Hyperion {

class MemoryMappedFileView;

class BlobResource final : public ResourceBase
{
public:
    BlobResource(MemoryMappedFile* mappedFile, const BlobResourceKey& key)
        : m_mappedFile(mappedFile),
          m_key(key),
          m_ptr(nullptr)
    {
    }

    BlobResource(const BlobResource&) = delete;
    BlobResource& operator=(const BlobResource&) = delete;

    virtual ~BlobResource() override = default;

    HYP_FORCE_INLINE const BlobResourceKey& GetKey() const
    {
        return m_key;
    }

    HYP_FORCE_INLINE void* GetData() const
    {
        return m_ptr;
    }

protected:
    virtual void Initialize() override final
    {
        Assert(m_mappedFile != nullptr);

        if (!m_mappedView.Reopen(*m_mappedFile, m_key.offset, m_key.size))
        {
            HYP_FAIL("Failed to open mapped view!");
            return;
        }

        m_ptr = m_mappedView.Data();
        Assert(m_ptr != nullptr);
    }

    virtual void Destroy() override final
    {
        m_ptr = nullptr;
        m_mappedView.Close();
    }

    MemoryMappedFile* m_mappedFile;
    BlobResourceKey m_key;

    void* m_ptr;
    MemoryMappedFileView m_mappedView;
};

} // namespace Hyperion
