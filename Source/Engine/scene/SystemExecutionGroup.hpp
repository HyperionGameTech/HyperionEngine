/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/containers/TypeMap.hpp>
#include <Core/containers/FlatMap.hpp>
#include <Core/containers/Array.hpp>

#include <Core/memory/UniquePtr.hpp>

#include <Core/reflection/Handle.hpp>
#include <Core/reflection/ObjId.hpp>

#include <Core/profiling/PerformanceClock.hpp>

#include <scene/System.hpp>

namespace Hyperion {

namespace threading {
class TaskBatch;
} // namespace threading

using threading::TaskBatch;

/*! \brief A group of Systems that are able to be processed concurrently, as they do not share any dependencies.
 */
class HYP_API SystemExecutionGroup
{
public:
    explicit SystemExecutionGroup(bool requiresSimThread = false, bool allowUpdate = true);

    SystemExecutionGroup(const SystemExecutionGroup&) = delete;
    SystemExecutionGroup& operator=(const SystemExecutionGroup&) = delete;

    SystemExecutionGroup(SystemExecutionGroup&&) noexcept = default;
    SystemExecutionGroup& operator=(SystemExecutionGroup&&) noexcept = default;

    ~SystemExecutionGroup();

    HYP_FORCE_INLINE bool RequiresSimThread() const
    {
        return m_requiresSimThread;
    }

    HYP_FORCE_INLINE bool AllowUpdate() const
    {
        return m_allowUpdate;
    }

    HYP_FORCE_INLINE TypeMap<SystemBase*>& GetSystems()
    {
        return m_systems;
    }

    HYP_FORCE_INLINE const TypeMap<SystemBase*>& GetSystems() const
    {
        return m_systems;
    }

    HYP_FORCE_INLINE TaskBatch* GetTaskBatch() const
    {
        return m_taskBatch.Get();
    }

#if HYP_DEBUG_MODE
    HYP_FORCE_INLINE const PerformanceClock& GetPerformanceClock() const
    {
        return m_performanceClock;
    }

    HYP_FORCE_INLINE const FlatMap<SystemBase*, PerformanceClock>& GetPerformanceClocks() const
    {
        return m_performanceClocks;
    }
#endif

    /*! \brief Checks if the SystemExecutionGroup is valid for the given System.
     *
     *  \param[in] systemPtr The System to check.
     *
     *  \return True if the SystemExecutionGroup is valid for the given System, false otherwise.
     */
    bool IsValidForSystem(const SystemBase* systemPtr) const;

    /*! \brief Checks if the SystemExecutionGroup has a System of the given type.
     *
     *  \tparam SystemType The type of the System to check for.
     *
     *  \return True if the SystemExecutionGroup has a System of the given type, false otherwise.
     */
    template <class SystemType>
    HYP_FORCE_INLINE bool HasSystem() const
    {
        static const TypeId s_typeId = TypeId::ForType<SystemType>();

        return m_systems.Find(s_typeId) != m_systems.End();
    }

    HYP_FORCE_INLINE bool HasSystem(const SystemBase* system) const
    {
        return m_systems.FindIf([system](const auto& item)
                   {
                       return item.second == system;
                   })
            != m_systems.End();
    }

    /*! \brief Adds a System to the SystemExecutionGroup.
     *
     *  \param[in] system The System to add.
     */
    SystemBase* AddSystem(SystemBase* system);

    template <class SystemType>
    SystemType* GetSystem() const
    {
        static const TypeId s_typeId = TypeId::ForType<SystemType>();

        const auto it = m_systems.Find(s_typeId);

        if (it == m_systems.End())
        {
            return nullptr;
        }

        return DynamicCast<SystemType>(it->second);
    }

    /*! \brief Removes a System from the SystemExecutionGroup.
     *
     *  \tparam SystemType The type of the System to remove.
     *
     *  \return True if the System was removed, false otherwise.
     */
    template <class SystemType>
    bool RemoveSystem()
    {
        static const TypeId s_typeId = TypeId::ForType<SystemType>();

        return m_systems.Erase(s_typeId);
    }

    bool RemoveSystem(const SystemBase* system)
    {
        if (!system)
        {
            return false;
        }

        auto it = m_systems.FindIf([system](const auto& item)
            {
                return item.second == system;
            });

        if (it != m_systems.End())
        {
            m_systems.Erase(it);

            return true;
        }

        return false;
    }

    /*! \brief Start processing all Systems in the SystemExecutionGroup.
     *
     *  \param[in] delta The delta time value
     */
    void StartProcessing(float delta, Span<Handle<Scene>> scenes);

    /*! \brief Waits on all processing tasks to complete */
    void FinishProcessing(bool executeBlocking = false);

private:
    bool m_requiresSimThread;
    bool m_allowUpdate;

    TypeMap<SystemBase*> m_systems;
    UniquePtr<TaskBatch> m_taskBatch;

#if HYP_DEBUG_MODE
    PerformanceClock m_performanceClock;
    FlatMap<SystemBase*, PerformanceClock> m_performanceClocks;
#endif
};

} // namespace Hyperion
