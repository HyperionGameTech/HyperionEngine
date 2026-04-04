/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/Types.hpp>

#include <Core/threading/SharedMutex.hpp>

#include <Core/io/MemoryMappedFile.hpp>

#include <Core/memory/ByteBuffer.hpp>
#include <Core/memory/allocator/Allocator.hpp>

#include <Core/filesystem/FilePath.hpp>

namespace Hyperion {

HYP_API extern Pool* g_assetPool;
using AssetAllocator = AllocatorInstance<Pool, &g_assetPool>;

class MappedBlobStorage
{
public:
    MappedBlobStorage(const FilePath& baseDir, size_t pageSize, bool readOnly)
        : m_baseDir(baseDir),
          m_pageSize(pageSize),
          m_readOnly(readOnly)
    {
    }

    MappedBlobStorage(const MappedBlobStorage& other) = delete;
    MappedBlobStorage& operator=(const MappedBlobStorage& other) = delete;

    ~MappedBlobStorage()
    {
        Clear();
    }

    HYP_FORCE_INLINE const FilePath& GetBaseDirectory() const
    {
        return m_baseDir;
    }

    MemoryMappedFile* Get(ANSIStringView name, bool createIfNotFound = true)
    {
        TUniqueLock lock(m_mutex);

        if (!m_baseDir.IsDirectory() && (m_readOnly || !m_baseDir.MkDir()))
        {
            // cannot map if the dir doesnt exist.
            return nullptr;
        }

        auto it = m_mappedFiles.FindAs(name);
        if (it != m_mappedFiles.End())
        {
            AssertDebug(it->second != nullptr);
            return it->second;
        }

        if (!createIfNotFound)
        {
            return nullptr;
        }

        const ANSIString nameStr = ANSIString(name);

        const FilePath filePath = m_baseDir / (nameStr + ".bin");

        MemoryMappedFile* mappedFile = HYP_POOL_NEW(g_assetPool, MemoryMappedFile, filePath, m_readOnly ? MemoryMappedFile::Mode::READ_ONLY : MemoryMappedFile::Mode::READ_WRITE);

        Assert(mappedFile != nullptr);

        if (!mappedFile->Open())
        {
            // failed to open file

            PoolDelete(*g_assetPool, mappedFile);

            return nullptr;
        }

        if (!m_readOnly)
        {
            Assert(mappedFile->EnsureCapacity(m_pageSize));
        }

        m_mappedFiles[nameStr] = mappedFile;

        return mappedFile;
    }

    size_t Size() const
    {
        TSharedLock lock(m_mutex);
        return m_mappedFiles.Size();
    }

    void Clear()
    {
        TUniqueLock lock(m_mutex);

        for (auto& pair : m_mappedFiles)
        {
            pair.second->Close();
            PoolDelete(*g_assetPool, pair.second);
        }

        m_mappedFiles.Clear();
    }

private:
    FilePath m_baseDir;
    size_t m_pageSize;
    bool m_readOnly;

    HashMap<ANSIString, MemoryMappedFile*> m_mappedFiles;
    SharedMutex m_mutex;
};

} // namespace Hyperion