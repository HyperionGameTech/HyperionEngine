/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_SPARSE_PAGED_ARRAY_HPP
#define HYPERION_SPARSE_PAGED_ARRAY_HPP

#include <core/containers/Array.hpp>
#include <core/containers/Bitset.hpp>

#include <core/Defines.hpp>

#include <Types.hpp>

namespace hyperion {
namespace containers {

template <class T, uint32 PageSize>
class SparsePagedArray : public ContainerBase<SparsePagedArray<T, PageSize>, SizeType>
{
    struct Page
    {
        ValueStorageArray<T, PageSize, alignof(T)> storage;
        Bitset initialized_bits;

        ~Page()
        {
            for (Bitset::BitIndex bit : initialized_bits)
            {
                storage.GetPointer()[bit].~T();
            }
        }
    };

public:
    static constexpr bool is_contiguous = false;

    struct Iterator
    {
        SparsePagedArray* array;
        Page** pg;
        SizeType element_index;

        Iterator(SparsePagedArray* array, SizeType idx)
            : array(array)
        {
            const SizeType page_index = PageIndex(idx);

            pg = array->m_pages.Begin() + page_index;
            element_index = ElementIndex(idx);

            while (pg != array->m_pages.End())
            {
                bool found = false;

                while (element_index < PageSize)
                {
                    if ((*pg)->initialized_bits.Test(element_index))
                    {
                        found = true;
                        break;
                    }
                    else
                    {
                        ++element_index;
                    }
                }

                if (found)
                {
                    break;
                }

                ++pg;

                element_index = 0;
            }
        }

        T& operator*() const
        {
        }
    };

    SparsePagedArray()
    {
        for (Page* page : m_pages)
        {
            if (page)
            {
                delete page;
                page = nullptr;
            }
        }
    }

    SparsePagedArray(const SparsePagedArray& other) = delete;
    SparsePagedArray& operator=(const SparsePagedArray& other) = delete;

    SparsePagedArray(SparsePagedArray&& other) noexcept
        : m_pages(std::move(other.m_pages))
    {
    }

    SparsePagedArray& operator=(SparsePagedArray&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        HYP_NOT_IMPLEMENTED();

        return *this;
    }

    ~SparsePagedArray()
    {
    }

    T& operator[](SizeType index)
    {
        const SizeType page_index = PageIndex(index);
        const SizeType element_index = ElementIndex(index);

        AssertDebug(page_index < m_pages.Size());
        AssertDebug(m_pages[page_index] != nullptr);

        return m_pages[page_index]->GetPointer()[element_index];
    }

    const T& operator[](SizeType index) const
    {
        return const_cast<SparsePagedArray&>(*this)[index];
    }

    HYP_DEF_STL_BEGIN_END(Iterator(this, 0), Iterator(this, m_pages.Size() * PageSize));

private:
    HYP_FORCE_INLINE static SizeType PageIndex(SizeType index)
    {
        return index / PageSize;
    }

    HYP_FORCE_INLINE static SizeType ElementIndex(SizeType index)
    {
        return index % PageSize;
    }

    Page* GetOrAllocatePage(SizeType page_index)
    {
        if (m_pages.Size() <= page_index)
        {
            m_pages.Resize(page_index + 1);
        }

        if (!m_pages[page_index])
        {
            m_pages[page_index] = new Page;
        }

        return m_pages[page_index];
    }

    Array<Page*, InlineAllocator<32>> m_pages;
};

} // namespace containers

template <class T, uint32 PageSize = 16>
using SparsePagedArray = containers::SparsePagedArray<T, PageSize>;

} // namespace hyperion

#endif
