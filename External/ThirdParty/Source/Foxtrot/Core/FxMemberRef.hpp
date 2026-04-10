#pragma once

#include <utility>

template <typename TPtrType>
struct FxMemberRef
{
    FxMemberRef() : pPtr(nullptr) {};

    FxMemberRef(nullptr_t np) : pPtr(nullptr) {}

    FxMemberRef(const FxMemberRef& other) = delete;
    FxMemberRef(FxMemberRef&& other) = delete;

    template <typename... TArgTypes>
    inline void InitRef(TArgTypes... args)
    {
        pPtr = ::new TPtrType(std::forward<TArgTypes>(args)...);
    }

    inline void Destroy()
    {
        if (pPtr == nullptr) {
            return;
        }

        ::delete pPtr;

        pPtr = nullptr;
    }

    void operator=(FxMemberRef& other) = delete;

    TPtrType* operator->() { return *pPtr; }
    TPtrType& operator*() { return *pPtr; }

    ~FxMemberRef() { Destroy(); }

public:
    TPtrType* pPtr;
};
