//
// Created by emd22 on 22/08/22.
//

#ifndef HYPERION_JITBUFFER_H
#define HYPERION_JITBUFFER_H

#include <vector>
#include <cstdint>
#include <cstring>

#include <util/Defines.hpp>

#if HYP_WINDOWS
#include <windows.h>
#elif HYP_UNIX
#include <sys/mman.h>
#include <sys/errno.h>
#endif

namespace hyperion::compiler::jit {

class InstructionBuffer : public std::vector<uint8_t> {
public:
    using vector<uint8_t>::vector;

    InstructionBuffer(std::initializer_list<InstructionBuffer> il) {
        for (auto &ib : il) {
            this->insert_back(ib);
        }
    }

    InstructionBuffer operator += (InstructionBuffer const &other)
    {
        this->insert_back(other);
        return *this;
    }
    void insert_back(InstructionBuffer const &other)
    {
        this->insert(this->end(), other.begin(), other.end());
    }
};

class MemoryPage {
    static const int DefaultMemoryPageSize = 4096;

public:
    MemoryPage()
        : MemoryPage(DefaultMemoryPageSize)
    {
    }
    MemoryPage(uint32_t size)
        : m_size(size)
    {
        #if HYP_WINDOWS
        m_data = VirtualAlloc(0, size, MEM_COMMIT, PAGE_READWRITE);
        #elif HYP_UNIX
        m_data = mmap(
            nullptr, size,
            (PROT_READ | PROT_WRITE),
            (MAP_PRIVATE | MAP_ANON),
            -1, 0
        );
        #else
        #error "Unsupported machine type for creating JIT memory page!"
        #endif
    }

    void Insert(const InstructionBuffer &instr) {
        bool was_protected = m_protected;
        if (m_protected) {
            Protect(false);
        }

        memcpy(m_data, instr.data(), instr.size());

        if (was_protected) {
            Protect(true);
        }
    }

    void Protect(bool protect=true)
    {
        #if HYP_WINDOWS
        DWORD oldp = 0;
        VirtualProtect(m_data, m_size, protect ? PAGE_EXECUTE_READ : PAGE_READWRITE, &oldp);
        #elif HYP_UNIX
        mprotect(m_data, m_size, PROT_READ | (protect ? PROT_EXEC : PROT_WRITE));
        #endif
        m_protected = protect;
    }

    ~MemoryPage()
    {
        #if HYP_WINDOWS
        VirtualFree(m_data, 0, MEM_RELEASE);
        #elif HYP_UNIX
        munmap(m_data, m_size);
        #endif
    }

    void *GetData()     { return m_data; }
    uint32_t GetSize()  { return m_size; }

private:
    void *m_data = nullptr;
    bool m_protected = false;
    uint32_t m_index = 0;
    uint32_t m_size = 0;
};

}

#endif //HYPERION_JITBUFFER_H
