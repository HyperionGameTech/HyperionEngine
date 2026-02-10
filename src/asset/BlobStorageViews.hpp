/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Types.hpp>

#include <core/containers/LinkedList.hpp>

#include <core/threading/SharedMutex.hpp>

#include <core/io/MemoryMappedFile.hpp>

#include <core/memory/ByteBuffer.hpp>
#include <core/memory/allocator/Allocator.hpp>

#include <core/filesystem/FilePath.hpp>

namespace Hyperion {

HYP_API extern Pool* g_assetPool;
using AssetAllocator = AllocatorInstance<Pool, &g_assetPool>;

// storage for blob data before package is saved
class MemoryBlobStorage
{
public:
    TByteBuffer<AssetAllocator>& Get(uint32 index)
    {
        TUniqueLock lock(m_mutex);

        if (m_list.Empty())
        {
            m_list.EmplaceBack();
        }

        typename decltype(m_list)::Iterator iter = m_list.Begin();

        for (uint32 i = 0; i <= index; ++i, ++iter)
        {
            if (i + 1 >= m_list.Size())
            {
                m_list.EmplaceBack();
            }
        }

        return *iter;
    }

    SizeType Size() const
    {
        TSharedLock lock(m_mutex);
        return m_list.Size();
    }

    void Clear()
    {
        TUniqueLock lock(m_mutex);

        m_list.Clear();
    }

private:
    LinkedList<TByteBuffer<AssetAllocator>, AssetAllocator> m_list;
    SharedMutex m_mutex;
};

class MappedBlobStorage
{
public:
    MappedBlobStorage(const FilePath& baseDir, bool readOnly)
        : m_baseDir(baseDir),
          m_readOnly(readOnly)
    {
    }

    MappedBlobStorage(const MappedBlobStorage& other) = delete;
    MappedBlobStorage& operator=(const MappedBlobStorage& other) = delete;

    ~MappedBlobStorage()
    {
        Clear();
    }

    MemoryMappedFile* Get(uint32 index)
    {
        TUniqueLock lock(m_mutex);

        const FilePath dir = m_baseDir / "Blobs";

        if (!dir.IsDirectory() && (m_readOnly || !dir.MkDir()))
        {
            // cannot map if the dir doesnt exist.
            return nullptr;
        }

        if (m_list.Empty())
        {
            const FilePath filePath = m_baseDir / "Blobs" / "_0.bin";

            m_list.EmplaceBack(filePath, m_readOnly ? MemoryMappedFile::Mode::READ_ONLY : MemoryMappedFile::Mode::READ_WRITE);
        }

        typename decltype(m_list)::Iterator iter = m_list.Begin();

        for (uint32 i = 0; i <= index; ++i, ++iter)
        {
            if (i + 1 >= m_list.Size())
            {
                const FilePath filePath = m_baseDir / "Blobs" / ("_" + String::ToString(index) + ".bin");

                m_list.EmplaceBack(filePath, m_readOnly ? MemoryMappedFile::Mode::READ_ONLY : MemoryMappedFile::Mode::READ_WRITE);
            }
        }

        if (!iter->IsOpen())
        {
            if (!iter->Open())
            {
                HYP_FAIL("Failed to open mapped file!");
            }
        }

        return &*iter;
    }

    SizeType Size() const
    {
        TSharedLock lock(m_mutex);
        return m_list.Size();
    }

    void Clear()
    {
        TUniqueLock lock(m_mutex);

        for (MemoryMappedFile& file : m_list)
        {
            file.Close();
        }

        m_list.Clear();
    }


private:
    FilePath m_baseDir;
    bool m_readOnly;

    LinkedList<MemoryMappedFile, AssetAllocator> m_list;
    SharedMutex m_mutex;
};

} // namespace Hyperion