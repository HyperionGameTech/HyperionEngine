/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <asset/AssetPath.hpp>

#include <core/reflection/ObjectBase.hpp>
#include <core/reflection/Handle.hpp>

#include <core/filesystem/FilePath.hpp>

#include <core/containers/HashSet.hpp>

#include <core/utilities/StringView.hpp>
#include <core/utilities/ForEach.hpp>
#include <core/utilities/Result.hpp>

#include <core/functional/ScriptableDelegate.hpp>

#include <core/threading/SharedMutex.hpp>
#include <core/threading/SchedulerFwd.hpp>

#include <core/memory/resource/Resource.hpp>

#include <core/logging/LoggerFwd.hpp>

#include <core/Constants.hpp>
#include <core/Defines.hpp>

#include <core/utilities/ClockTimer.hpp>

#include <algorithm>
#include <type_traits>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Assets);

HYP_API extern Pool* g_assetPool;
using AssetAllocator = AllocatorInstance<Pool, &g_assetPool>;

class AssetRegistry;
class AssetPackage;
class AssetObject;
struct BoxedValue;
class ByteWriter;
class BlobStorage;
class MemoryMappedFile;

extern StringHash AssetPackage_KeyByFunction(const Handle<AssetPackage>& assetPackage);
extern StringHash AssetObject_KeyByFunction(const Handle<AssetObject>& assetObject);

using AssetPackageSet = HashSet<Handle<AssetPackage>, &AssetPackage_KeyByFunction, NodeAllocator<AssetAllocator>>;
using AssetObjectSet = HashSet<Handle<AssetObject>, &AssetObject_KeyByFunction, NodeAllocator<AssetAllocator>>;

HYP_ENUM()
enum class AssetPackageFlags : uint32
{
    None = 0x0,
    Transient = 0x1,    //!< Not saved to disk
    Hidden = 0x2,       //!< Hide in content browser
    HasBlobStorage = 0x4
};

HYP_MAKE_ENUM_FLAGS(AssetPackageFlags);

HYP_ENUM()
enum class AddAssetConflictMode : uint8
{
    FailOnConflict = 0,
    GenerateNewName,
    ReplaceExisting,

    Default //!< per-package type default
};

HYP_CLASS()
class HYP_API AssetPackage final : public ObjectBase
{
    HYP_OBJECT_BODY(AssetPackage);

    friend class AssetRegistry;

public:
    static Pool* GetAllocator() { return g_assetPool; }

    AssetPackage();

    explicit AssetPackage(Name name, EnumFlags<AssetPackageFlags> flags = AssetPackageFlags::None);

    AssetPackage(const AssetPackage& other) = delete;
    AssetPackage& operator=(const AssetPackage& other) = delete;

    AssetPackage(AssetPackage&& other) noexcept = delete;
    AssetPackage& operator=(AssetPackage&& other) noexcept = delete;

    ~AssetPackage();

    HYP_METHOD()
    HYP_FORCE_INLINE Name GetName() const
    {
        return m_name;
    }

    HYP_METHOD()
    void Rename(Name name);

    HYP_METHOD()
    HYP_FORCE_INLINE Name GetFriendlyName() const
    {
        return m_friendlyName.IsValid() ? m_friendlyName : m_name;
    }

    HYP_METHOD()
    HYP_FORCE_INLINE void SetFriendlyName(Name friendlyName)
    {
        m_friendlyName = friendlyName;
    }

    HYP_METHOD()
    HYP_FORCE_INLINE EnumFlags<AssetPackageFlags> GetFlags() const
    {
        return m_flags;
    }

    void SetBlobStorageEnabled(bool enabled);

    HYP_METHOD()
    HYP_FORCE_INLINE bool IsTransient() const
    {
        return m_flags[AssetPackageFlags::Transient];
    }

    HYP_METHOD()
    HYP_FORCE_INLINE bool IsHidden() const
    {
        return m_flags[AssetPackageFlags::Hidden];
    }

    HYP_METHOD()
    HYP_FORCE_INLINE bool IsLoading() const
    {
        return (AtomicAdd(&m_stateFlags, 0) & SF_Loading) != 0;
    }

    HYP_FORCE_INLINE const WeakHandle<AssetRegistry>& GetRegistry() const
    {
        return m_registry;
    }

    HYP_FORCE_INLINE const WeakHandle<AssetPackage>& GetParentPackage() const
    {
        return m_parentPackage;
    }

    /*! \internal Serialization only */
    HYP_FORCE_INLINE const AssetPackageSet& GetSubpackages() const
    {
        return m_subpackages;
    }

    /*! \internal Serialization only */
    HYP_FORCE_INLINE void SetSubpackages(const AssetPackageSet& subpackages)
    {
        m_subpackages = subpackages;
    }

    HYP_METHOD()
    bool IsSubpackageOf(const AssetPackage& other) const;

    template <class Callback>
    void ForEachSubpackage(Callback&& callback) const
    {
        AssetPackageSet set;

        {
            TUniqueLock guard(m_mutex);
            set = m_subpackages;
        }

        ForEach(set, std::forward<Callback>(callback));
    }

    HYP_FORCE_INLINE const AssetObjectSet& GetAssets() const
    {
        return m_assetObjects;
    }

    void SetAssets(const AssetObjectSet& assetObjects);

    template <class Callback>
    void ForEachAssetObject(Callback&& callback) const
    {
        AssetObjectSet set;

        {
            TUniqueLock guard(m_mutex);
            set = m_assetObjects;
        }

        ForEach(set, std::forward<Callback>(callback));
    }

    Result AddAssetObject(const Handle<AssetObject>& assetObject, bool replaceOnConflict);
    Result RemoveAssetObject(const Handle<AssetObject>& assetObject);

    BlobStorage* GetBlobStorage() const;

    /*! \brief Initialize (owned) BlobStorage for this Package */
    void InitBlobStorage(bool readOnly = true);

    /*! \brief Merges the contents of another package into this one.
     *  Transfers ownership of all asset objects and subpackages from the source package
     *  to this package. Assets are renamed if conflicts occur, and subpackages are merged recursively.
     *  After successful merge, the source package will be empty.
     *  \param package The package to merge into this one.
     *  \return Result indicating success or failure of the merge operation. */
    Result MergePackage(const Handle<AssetPackage>& package);

    HYP_METHOD()
    String BuildPackagePath() const;

    HYP_METHOD()
    AssetPath BuildAssetPath(Name assetName) const;

    HYP_METHOD()
    bool HasAssetWithName(StringHash assetName) const;

    HYP_METHOD()
    Name GetUniqueAssetName(Name baseName) const;

    HYP_METHOD()
    Result Save(const FilePath& outputDirectory, bool saveEvenIfNotDirty = false);

    HYP_METHOD()
    HYP_FORCE_INLINE const Array<AssetPath>& GetDependencies() const
    {
        return m_dependencies;
    }

    /*! \brief Method to save dependencies as relative paths to this package rather than absolute
     *   \warning Not thread-safe.
     *  \return Array of relative dependency paths. */
    HYP_METHOD(Property = "Dependencies", Serialize = true)
    Array<String> GetRelativeDependencies() const;

    /*! \brief Sets dependencies from relative paths to this package rather than absolute
     *   \warning Not thread-safe.
     *  \param relativePaths Array of relative dependency paths. */
    HYP_METHOD(Property = "Dependencies", Serialize = true)
    void SetRelativeDependencies(const Array<String>& relativePaths);

    HYP_METHOD()
    void AddDependency(const AssetPath& dependency);

    /*! \brief For transient packages, removes any asset objects and subpackages that are
     *   no longer referenced outside of the package itself.
     *   \param outRemovedPackages an array that will hold handles of the packages that were removed
     *   \param outShouldDestroy If provided, will be set to true if the package is now empty and should be destroyed.
     */
    void Prune(Array<Handle<AssetPackage>>& outRemovedPackages, bool* outShouldDestroy = nullptr);

    HYP_METHOD(Property = "IsDirty", Transient)
    HYP_FORCE_INLINE bool IsDirty() const
    {
        return (AtomicAdd(&m_stateFlags, 0) & SF_Dirty) != 0;
    }

    HYP_METHOD(Property = "IsSaved", Transient)
    HYP_FORCE_INLINE bool IsSaved() const
    {
        TSharedLock guard(m_mutex);
        return IsSaved_Internal();
    }

    HYP_FIELD()
    ScriptableDelegate<void, Handle<AssetObject>, bool /* isDirect */> OnAssetObjectAdded;

    HYP_FIELD()
    ScriptableDelegate<void, Handle<AssetObject>, bool /* isDirect */> OnAssetObjectRemoved;

    HYP_FIELD()
    ScriptableDelegate<void, Handle<AssetPackage>> OnSubpackageAdded;

    HYP_FIELD()
    ScriptableDelegate<void, Handle<AssetPackage>> OnSubpackageRemoved;

private:
    void Init() override;

    Handle<AssetObject> GetAssetObject(UTF8StringView assetName, bool attemptLoading);

    void MarkDirty();

    void WaitUntilLoaded();
    void SignalLoaded();

    HYP_FORCE_INLINE bool IsSaved_Internal() const
    {
        return m_packageDir.Length() != 0;
    }

    Result SaveManifest(ByteWriter& stream) const;

    void InitBlobStorage(BlobStorage& outStorage, bool readOnly = true);

    void LoadBlobStorage();

    /*! \brief Check for dirty asset objects and returns true if any are.
     *   Used before saving, to check if we should call MarkDirty() */
    bool HasDirtyAssetObjects() const;

    Name GetUniqueAssetName_Internal(Name baseName) const;
    Name GetUniqueSubpackageName_Internal(Name baseName) const;

    HYP_FIELD()
    Name m_name;

    HYP_FIELD()
    Name m_friendlyName;

    HYP_FIELD()
    EnumFlags<AssetPackageFlags> m_flags;

    HYP_FIELD(Transient)
    Array<AssetPath> m_dependencies;

    enum StateFlags : int32
    {
        SF_Dirty = 0x1,
        SF_Loading = 0x2
    };

    mutable volatile int32 m_stateFlags;

    WeakHandle<AssetRegistry> m_registry;
    WeakHandle<AssetPackage> m_parentPackage;
    AssetPackageSet m_subpackages;
    AssetObjectSet m_assetObjects;
    FilePath m_packageDir;

    SharedMutex m_mutex;

    mutable Mutex m_loadedMutex;
    ConditionVariable m_loadedCV;
    ThreadId m_loadingThreadId;

    BlobStorage* m_blobStorage;
};

HYP_CLASS()
class HYP_API AssetRegistry final : public ObjectBase
{
    HYP_OBJECT_BODY(AssetRegistry);

    friend class AssetPackage;

public:
    static Pool* GetAllocator() { return g_assetPool; }

    AssetRegistry();
    explicit AssetRegistry(const String& rootPath);

    AssetRegistry(const AssetRegistry& other) = delete;
    AssetRegistry& operator=(const AssetRegistry& other) = delete;

    AssetRegistry(AssetRegistry&& other) noexcept = delete;
    AssetRegistry& operator=(AssetRegistry&& other) noexcept = delete;

    ~AssetRegistry();

    HYP_METHOD()
    HYP_FORCE_INLINE String GetRootPath() const
    {
        TSharedLock guard(m_mutex);
        return m_rootPath;
    }

    HYP_METHOD()
    void SetRootPath(const String& rootPath);

    HYP_FORCE_INLINE const AssetPackageSet& GetPackages() const
    {
        return m_packages;
    }

    /*! \internal Serialization only */
    void SetPackages(const AssetPackageSet& packages);

    /*! \brief Adds a package to the registry. If a package with the same name already exists and `mergeIfExists` is false,
     *  this will fail and return error.
     *  If `mergeIfExists` is true, the contents of the given package will be merged into the existing package.
     *  \param package The package to add
     *  \param mergeIfExists If true, and a package with the same name already exists, the contents of the given package will be merged into the existing package.
     *  \return Result indicating success or failure of the operation. */
    Result AddPackage(const Handle<AssetPackage>& package, bool mergeIfExists = false);

    HYP_METHOD()
    void RemovePackage(AssetPackage* package);

    template <class Callback>
    void ForEachPackage(Callback&& callback) const
    {
        ForEach(m_packages, m_mutex, std::forward<Callback>(callback));
    }

    HYP_METHOD()
    Handle<AssetPackage> GetPackageFromPath(
        const UTF8StringView& path,
        bool createIfNotExist = true,
        bool requireLoaded = true);

    HYP_METHOD()
    Handle<AssetPackage> GetPackage(
        const Handle<AssetPackage>& parentPackage,
        const UTF8StringView& subpackageName,
        bool createIfNotExist = true,
        bool requireLoaded = true);

    HYP_METHOD()
    void LoadSubpackages(const Handle<AssetPackage>& parentPackage, bool recursive);

    TResult<Handle<AssetPackage>> LoadPackageFromManifest(
        const FilePath& manifestPath,
        bool loadSubpackages,
        bool forceLoad);

    HYP_METHOD()
    Name GetUniqueAssetName(const UTF8StringView& packagePath, Name baseName) const;

    Result RegisterAsset(
        const UTF8StringView& path,
        const Handle<AssetObject>& assetObject,
        AddAssetConflictMode conflictMode = AddAssetConflictMode::Default);

    /*! \brief Registers `target` if it is a subclass of AssetObject and registers all
     *  of its members that are subclasses of AssetObject as well, recursively.
     *  \param packagePath The base/root path in which to register the asset and its members.
     *  \param target The object to register.
     *  \param forceRelocation If true, will relocate assets that are already registered to the new package path.
     * \param getObjectSubpath Optional callback to determine the sub-path for each asset object being registered. */
    void RegisterAssetsRecursively(
        const UTF8StringView& packagePath,
        const BoxedValue& target,
        bool forceRelocation = false,
        ProcRef<String(const AssetObject&)> getObjectSubpath = nullptr);

    Handle<AssetObject> GetAssetFromPath(const UTF8StringView& path, bool attemptLoading = true) const;

    MemoryMappedFile* MapFile(const FilePath& path);

    void Initialize();

    /*! \brief Called by AssetManager to perform enqueued tasks that mutate the registry. */
    void Update(float delta);

    HYP_FIELD()
    ScriptableDelegate<void, Handle<AssetPackage>> OnPackageAdded;

    HYP_FIELD()
    ScriptableDelegate<void, Handle<AssetPackage>> OnPackageRemoved;

private:
    void LoadPackagesAsync(bool loadSubpackages = false);

    template <class Func, class FutureType = void>
    void PostTask(Func&& fn, Task<FutureType>* outFuture = nullptr);

    void PruneTransientPackages();

    Handle<AssetPackage> GetPackageFromPath_Internal(
        const UTF8StringView& path,
        bool createIfNotExist,
        bool requireLoaded);

    Handle<AssetObject> GetAssetFromPath_Internal(
        const UTF8StringView& path,
        String& outAssetName,
        bool attemptLoading);

    HYP_FIELD(Serialize = true)
    String m_rootPath;

    AssetPackageSet m_packages;
    SharedMutex m_mutex;

    // timer for when we should prune transient packages
    ClockTimer m_pruneTimer;
    threading::TaskBatch* m_pruneTaskBatch;

    Scheduler* m_scheduler;
};

} // namespace Hyperion
