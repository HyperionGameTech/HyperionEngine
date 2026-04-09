/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <asset/AssetPath.hpp>
#include <asset/AssetTypes.hpp>
#include <asset/BlobStorageStructs.hpp>

#include <Core/reflection/ObjectBase.hpp>
#include <Core/reflection/Handle.hpp>

#include <Core/utilities/Result.hpp>

#include <Core/filesystem/FilePath.hpp>

#include <Core/memory/resource/Resource.hpp>

#include <Core/memory/pool/Pool.hpp>

#include <Core/logging/LoggerFwd.hpp>

#include <Core/Constants.hpp>
#include <Core/Defines.hpp>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Assets);

enum class ChunkId : uint32;

class AssetPackage;
class AssetObject;
class ByteWriter;
class BlobStorage;

class BufferedReader;
using BufferedByteReader = BufferedReader;

namespace functional {

template <class FunctionSignature>
class ProcRef;

} // namespace functional

using functional::ProcRef;

namespace JSON {
class Object;
} // namespace JSON

HYP_API extern Pool* g_assetPool;
using AssetAllocator = AllocatorInstance<Pool, &g_assetPool>;

HYP_CLASS(Abstract)
class HYP_API AssetObject : public ObjectBase
{
    HYP_OBJECT_BODY(AssetObject);

public:
    friend class AssetRegistry;
    friend class AssetPackage;

    AssetObject();
    explicit AssetObject(Name name);

    AssetObject(const AssetObject& other) = delete;
    AssetObject& operator=(const AssetObject& other) = delete;

    AssetObject(AssetObject&& other) noexcept = delete;
    AssetObject& operator=(AssetObject&& other) noexcept = delete;

    virtual ~AssetObject();

    HYP_METHOD(Property = "Name", Serialize, EditOrder = 1)
    Name GetName() const
    {
        return m_name;
    }

    HYP_METHOD(Property = "Name")
    HYP_FORCE_INLINE void SetName(Name name)
    {
        (void)Rename(name);
    }

    HYP_METHOD()
    virtual Result Rename(Name name);

    HYP_METHOD()
    bool IsDirty() const
    {
        return AtomicAdd(&m_isDirty, 0) != 0;
    }

    HYP_METHOD()
    void MarkDirty();

    HYP_METHOD(Property = "FriendlyName")
    Name GetFriendlyName() const
    {
        return m_friendlyName.IsValid() ? m_friendlyName : m_name;
    }

    HYP_METHOD(Property = "FriendlyName")
    void SetFriendlyName(Name friendlyName)
    {
        m_friendlyName = friendlyName;
    }

    HYP_METHOD()
    const FilePath& GetOriginalFilepath() const
    {
        return m_originalFilepath;
    }

    HYP_METHOD()
    void SetOriginalFilepath(const FilePath& originalFilepath)
    {
        m_originalFilepath = originalFilepath;
    }

    HYP_METHOD()
    Handle<AssetPackage> GetPackage() const
    {
        return m_package.Lock();
    }

    HYP_METHOD()
    const AssetPath& GetPath() const
    {
        AssertDebug(IsRegistered(), "Calling GetPath() on an unregistered asset object");

        return m_assetPath;
    }

    HYP_METHOD()
    bool IsRegistered() const
    {
        return m_package.IsValid();
    }

    HYP_METHOD(Property = "AssetFlags", Editor = false)
    EnumFlags<AssetObjectFlags> GetAssetFlags() const
    {
        return m_flags;
    }

    HYP_METHOD(Property = "AssetFlags")
    void SetAssetFlags(EnumFlags<AssetObjectFlags> flags);

    HYP_METHOD()
    void SetPersistentRequested(bool persistentlyLoaded, bool setFlag = true, bool markDirty = true);

    HYP_METHOD()
    bool IsTransient() const
    {
        return bool(m_flags & (AssetObjectFlags::Transient | AssetObjectFlags::TransientByProxy));
    }

    HYP_METHOD()
    bool IsTransientByProxy() const
    {
        return (m_flags & (AssetObjectFlags::Transient | AssetObjectFlags::TransientByProxy)) == AssetObjectFlags::TransientByProxy;
    }

    HYP_METHOD()
    void SetIsTransient(bool isTransient);

    HYP_METHOD()
    void SetIsTransientByProxy(bool isTransientByProxy);

    HYP_METHOD()
    bool IsSaved() const;

    HYP_METHOD()
    Result Save();

    HYP_METHOD()
    Result SaveAs(const FilePath& manifestPath);

    TUniqueLock<AssetObject> GetWriteScope() const;
    TSharedLock<AssetObject> GetReadScope() const;
    
    void LockWriter(bool doInitialize = true);
    void UnlockWriter(bool doDeinitialize = true);

    void LockReader();
    void UnlockReader();

    void GetNumUsers(int64& outReaders, int64& outWriters) const;

    HYP_METHOD()
    Result Register(
        const UTF8StringView& path,
        AddAssetConflictMode conflictMode = AddAssetConflictMode::Default);

    virtual void RegisterRecursive(
        const UTF8StringView& path,
        AddAssetConflictMode conflictMode = AddAssetConflictMode::Default);
        
    virtual void Init() override
    {
        SetReady(true);
    }

    static Result LoadDesc(
        JSON::Object& manifestData,
        AssetDesc& outAssetDesc);

    static Result Load(
        JSON::Object& manifestData,
        Handle<AssetObject>& outAssetObject);

protected:
    virtual void OnDirtyStateChanged(bool isDirty)
    {
        // do nothing by default
    }

    virtual void OnLoaded()
    {
        // do nothing
    }

    virtual void OnUnloaded()
    {
        // do nothing
    }

    virtual void PageBlobData()
    {
        // do nothing
    }

    virtual void UnpageBlobData()
    {
        // do nothing
    }

    void AllocateBlobData(BlobDataReference& reference, const void* inData, size_t count, size_t alignment = 16);
    void FreeBlobData(BlobDataReference& reference);

    void SetBlobDataResident(bool resident);
    void SetBlobDataResident(bool resident, BlobDataReference& reference);

    virtual void CollectBlobDataReferences(Array<Tuple<const char*, uint16, BlobDataReference*>>& outReferences)
    {
    }

    Result SaveManifest(ByteWriter& stream) const;

    Result SaveBlobData(BlobStorage& storage,
        const Optional<FilePath>& localBlobDirectory = {});

    HYP_FIELD(Property = "Name")
    Name m_name;

    HYP_FIELD(Property = "FriendlyName")
    Name m_friendlyName;

    HYP_FIELD(Property = "AssetFlags", Transient, EditHide)
    EnumFlags<AssetObjectFlags> m_flags;

    HYP_FIELD(EditEnabled = false)
    FilePath m_originalFilepath; // used to determine if we should skip importing an asset

    HYP_FIELD(Transient)
    WeakHandle<AssetPackage> m_package;

    HYP_FIELD(Transient)
    AssetPath m_assetPath;

    HYP_FIELD(Transient)
    FilePath m_manifestPath;

    HYP_FIELD(NoScriptBindings, Transient)
    mutable volatile int32 m_isDirty;
    
    mutable volatile int64 m_rwState;

    mutable Mutex m_initMutex;
    ConditionVariable m_initCV;
    bool m_isBlobLoaded;
};

} // namespace Hyperion
