template <class Component, class EntityManagerPtr>
HYP_FORCE_INLINE Component& Entity::GetComponent() const
{
    EntityManagerPtr entityManager = reinterpret_cast<EntityManagerPtr>(GetEntityManager());
    AssertDebug(entityManager != nullptr);

    return entityManager->template GetComponent<Component>(this);
}

template <class Component, class EntityManagerPtr>
HYP_FORCE_INLINE Component* Entity::TryGetComponent() const
{
    EntityManagerPtr entityManager = reinterpret_cast<EntityManagerPtr>(GetEntityManager());
    AssertDebug(entityManager != nullptr);

    return entityManager->template TryGetComponent<Component>(this);
}

template <class Component, class EntityManagerPtr>
HYP_FORCE_INLINE bool Entity::HasComponent() const
{
    EntityManagerPtr entityManager = reinterpret_cast<EntityManagerPtr>(GetEntityManager());
    AssertDebug(entityManager != nullptr);

    return entityManager->template TryGetComponent<Component>(this) != nullptr;
}

template <class Component, class T, class EntityManagerPtr>
HYP_FORCE_INLINE Component& Entity::AddComponent(T&& component)
{
    EntityManagerPtr entityManager = reinterpret_cast<EntityManagerPtr>(GetEntityManager());
    AssertDebug(entityManager != nullptr);

    return entityManager->template AddComponent<Component>(this, std::forward<T>(component));
}

template <class Component, class EntityManagerPtr>
HYP_FORCE_INLINE bool Entity::RemoveComponent()
{
    EntityManagerPtr entityManager = reinterpret_cast<EntityManagerPtr>(GetEntityManager());
    AssertDebug(entityManager != nullptr);

    return entityManager->template RemoveComponent<Component>(this);
}

template <EntityTag Tag, class EntityManagerPtr>
HYP_FORCE_INLINE void Entity::AddTag()
{
    EntityManagerPtr entityManager = reinterpret_cast<EntityManagerPtr>(GetEntityManager());

    if (!entityManager)
    {
        m_entityInitInfo.initialTags.PushBack(Tag);

        return;
    }

    entityManager->template AddTag<Tag>(this);
}

template <EntityTag Tag, class EntityManagerPtr>
HYP_FORCE_INLINE bool Entity::RemoveTag()
{
    EntityManagerPtr entityManager = reinterpret_cast<EntityManagerPtr>(GetEntityManager());

    if (!entityManager)
    {
        auto it = m_entityInitInfo.initialTags.Find(Tag);

        if (it != m_entityInitInfo.initialTags.End())
        {
            m_entityInitInfo.initialTags.Erase(it);

            return true;
        }

        return false;
    }

    return entityManager->template RemoveTag<Tag>(this);
}

template <EntityTag Tag, class EntityManagerPtr>
HYP_FORCE_INLINE bool Entity::HasTag() const
{
    EntityManagerPtr entityManager = reinterpret_cast<EntityManagerPtr>(GetEntityManager());

    if (!entityManager)
    {
        auto it = m_entityInitInfo.initialTags.Find(Tag);

        return it != m_entityInitInfo.initialTags.End();
    }

    return entityManager->template HasTag<Tag>(this);
}