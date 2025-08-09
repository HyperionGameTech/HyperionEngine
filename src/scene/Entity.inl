template <class Component, class EntityManagerPtr>
Component& Entity::GetComponent() const
{
    HYP_SCOPE;
    AssertReady();

    EntityManagerPtr entityManager = reinterpret_cast<EntityManagerPtr>(GetEntityManager());
    AssertDebug(entityManager != nullptr);

    return entityManager->template GetComponent<Component>(this);
}

template <class Component, class EntityManagerPtr>
Component* Entity::TryGetComponent() const
{
    HYP_SCOPE;
    AssertReady();

    EntityManagerPtr entityManager = reinterpret_cast<EntityManagerPtr>(GetEntityManager());
    AssertDebug(entityManager != nullptr);

    return entityManager->template TryGetComponent<Component>(this);
}

template <class Component, class EntityManagerPtr>
bool Entity::HasComponent() const
{
    HYP_SCOPE;
    AssertReady();

    EntityManagerPtr entityManager = reinterpret_cast<EntityManagerPtr>(GetEntityManager());
    AssertDebug(entityManager != nullptr);

    return entityManager->template TryGetComponent<Component>(this) != nullptr;
}

template <class Component, class T, class EntityManagerPtr>
Component& Entity::AddComponent(T&& component)
{
    HYP_SCOPE;
    AssertReady();

    EntityManagerPtr entityManager = reinterpret_cast<EntityManagerPtr>(GetEntityManager());
    AssertDebug(entityManager != nullptr);

    return entityManager->template AddComponent<Component>(this, std::forward<T>(component));
}

template <class Component, class EntityManagerPtr>
bool Entity::RemoveComponent()
{
    HYP_SCOPE;
    AssertReady();

    EntityManagerPtr entityManager = reinterpret_cast<EntityManagerPtr>(GetEntityManager());
    AssertDebug(entityManager != nullptr);

    return entityManager->template RemoveComponent<Component>(this);
}

template <EntityTag Tag, class EntityManagerPtr>
void Entity::AddTag()
{
    HYP_SCOPE;
    AssertReady();

    EntityManagerPtr entityManager = reinterpret_cast<EntityManagerPtr>(GetEntityManager());
    AssertDebug(entityManager != nullptr);

    entityManager->template AddTag<Tag>(this);
}

template <EntityTag Tag, class EntityManagerPtr>
bool Entity::RemoveTag()
{
    HYP_SCOPE;
    AssertReady();

    EntityManagerPtr entityManager = reinterpret_cast<EntityManagerPtr>(GetEntityManager());
    AssertDebug(entityManager != nullptr);

    return entityManager->template RemoveTag<Tag>(this);
}

template <EntityTag Tag, class EntityManagerPtr>
bool Entity::HasTag() const
{
    HYP_SCOPE;
    AssertReady();

    EntityManagerPtr entityManager = reinterpret_cast<EntityManagerPtr>(GetEntityManager());
    AssertDebug(entityManager != nullptr);

    return entityManager->template HasTag<Tag>(this);
}