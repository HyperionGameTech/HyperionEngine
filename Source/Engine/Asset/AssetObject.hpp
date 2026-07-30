/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Asset/AssetPath.hpp>
#include <Asset/AssetTypes.hpp>
#include <Asset/BlobStorageStructs.hpp>

#include <Core/Reflection/ObjectBase.hpp>
#include <Core/Reflection/Handle.hpp>

#include <Core/Utilities/Result.hpp>

#include <Core/FileSystem/FilePath.hpp>

#include <Core/Resource/Resource.hpp>
#include <Core/Resource/ResLock.hpp>

#include <Core/Memory/Pool/Pool.hpp>

#include <Core/Logging/LoggerFwd.hpp>

#include <Core/Constants.hpp>
#include <Core/Defines.hpp>

namespace Hyperion {

ENGINE_API HYP_DECLARE_LOG_CHANNEL(Assets);

#define HYP_ASSET_OBJECT_THREAD_SAFE

class AssetRegistry;
class ByteWriter;
class BlobStorage;

struct BoxedValue;

HYP_CLASS(Abstract)
class ENGINE_API AssetObject : public ObjectBase
{
    HYP_OBJECT_BODY(AssetObject);

public:
    friend class AssetRegistry;
    friend class AssetBucketData;

    AssetObject();
    explicit AssetObject(Name name);

    AssetObject(const AssetObject& other) = delete;
    AssetObject& operator=(const AssetObject& other) = delete;

    AssetObject(AssetObject&& other) noexcept = delete;
    AssetObject& operator=(AssetObject&& other) noexcept = delete;

    virtual ~AssetObject();

    HYP_METHOD(Property = "Name", Serialize, EditorOrder = 1)
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
    void MarkDirty();

    HYP_METHOD()
    uint32 GetAssetIndex() const
    {
        return m_assetIndex;
    }

    HYP_METHOD()
    bool IsRegistered() const
    {
        return m_assetIndex != AssetDesc::InvalidIndex
            && m_assetPath.IsValid();
    }

    HYP_METHOD()
    const AssetPath& GetPath() const
    {
        return m_assetPath;
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
        return bool(m_flags & AssetObjectFlags::Transient);
    }

    HYP_METHOD()
    void SetIsTransient(bool isTransient);

    HYP_METHOD()
    bool IsSaved() const;

    HYP_METHOD()
    Result Save();

    HYP_METHOD()
    Result SaveAs(const FilePath& manifestPath);

    HYP_NODISCARD TUniqueResLock<AssetObject> GetWriteScope() const;
    HYP_NODISCARD TSharedResLock<AssetObject> GetReadScope() const;

    void LockWriter();
    void UnlockWriter();

    void LockReader();
    void UnlockReader();

    void GetNumUsers(int64& outReaders, int64& outWriters) const;

    Handle<AssetRegistry> GetAssetRegistry();

    virtual void Init() override
    {
        SetReady(true);
    }

    static Result Load(
        BoxedValue& manifestData,
        Handle<AssetObject>& outAssetObject);

protected:
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

    Result SaveBlobData(BlobStorage* storage, const Optional<FilePath>& localBlobDirectory = {});

    HYP_FIELD(Property = "Name")
    Name m_name;

    HYP_FIELD(Property = "AssetFlags", Transient, Editor = false)
    EnumFlags<AssetObjectFlags> m_flags;

    HYP_FIELD(Property = "AssetIndex", Transient, Editor = false)
    uint32 m_assetIndex;

    HYP_FIELD(Transient)
    AssetPath m_assetPath;
    
#ifdef HYP_ASSET_OBJECT_THREAD_SAFE
    mutable volatile int64 m_rwState;

    AtomicVar<bool> m_isInit;
    AtomicVar<bool> m_isBlobLoaded;
#else
    uint32 m_numReaders;
#endif // HYP_ASSET_OBJECT_THREAD_SAFE
};

} // namespace Hyperion
