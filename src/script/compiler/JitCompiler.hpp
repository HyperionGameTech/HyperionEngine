#ifndef JIT_COMPILER_HPP
#define JIT_COMPILER_HPP

#include <script/compiler/jit/ArchAmd64.hpp>

#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/ast/AstArgument.hpp>
#include <script/compiler/ast/AstMember.hpp>
#include <script/compiler/emit/BytecodeChunk.hpp>

#include <system/Debug.hpp>
#include <core/Core.hpp>

#include <memory>

#ifdef HYP_WINDOWS
#include <windows.h>
#else
#include <unistd.h>
#include <sys/mman.h>
#endif


namespace hyperion::compiler {

struct Page
{
    Page()
        : m_size(GetPageSize()), m_offset(0)
    {
    #if HYP_WINDOWS
        DWORD type = MEM_RESERVE | MEM_COMMIT;
        m_data = (uint8_t *)VirtualAlloc(NULL, m_size, type, PAGE_READWRITE);
    #else
        int prot = PROT_READ | PROT_WRITE;
        int flags = MAP_ANONYMOUS | MAP_PRIVATE;
        m_data = (uint8_t *)mmap(NULL, m_size, prot, flags, -1, 0);
    #endif
    }

    void Protect()
    {
    #if HYP_WINDOWS
        DWORD old;
        VirtualProtect(m_data, sizeof(*m_data), PAGE_EXECUTE_READ, &old);
    #else
        mprotect(m_data, sizeof(*m_data), PROT_READ | PROT_EXEC);
    #endif
    }

    ~Page()
    {
    #if HYP_WINDOWS
        VirtualFree(m_data, 0, MEM_RELEASE);
    #else
        munmap(m_data, m_size);
    #endif
    }

    void SetData(const std::vector<uint8_t> &ops)
    {
        Memory::Copy(m_data + m_offset, ops.data(), ops.size());
        m_offset += ops.size();
    }
    void SetData(uint8_t imm)
    {
        m_data[m_offset++] = imm;
    }

    /* Get the size of a memory page */
    uint32_t GetPageSize()
    {
    #if HYP_WINDOWS
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        return info.dwPageSize;
    #else
        return getpagesize();
    #endif
    }

    uint8_t *GetData() { return m_data; }
    uint32_t GetPosition() { return m_offset; }

    uint32_t m_size;
    uint32_t m_offset;
    uint32_t m_page_size = 0;
    uint8_t *m_data = nullptr;
};


class JitCompiler : public AstVisitor {
public:
    struct CondInfo {
        AstStatement *cond;
        AstStatement *then_part;
        AstStatement *else_part;
    };

    struct ExprInfo {
        AstExpression *left;
        AstExpression *right;
    };

    std::unique_ptr<Page> m_page;

    enum class Arch
    {
        NONE,
        AMD64,
    };

    void Emit(std::vector<uint8_t> &ops) { m_page->SetData(ops); }

    int Run()
    {
        int (*func)(void) = (int(*)(void))(m_page->GetData());
        return func();
    }

    static std::unique_ptr<Buildable> BuildArgumentsStart(
        AstVisitor *visitor,
        Module *mod,
        const std::vector<std::shared_ptr<AstArgument>> &args
    );

    static std::unique_ptr<Buildable> BuildArgumentsEnd(
        AstVisitor *visitor,
        Module *mod,
        size_t nargs
    );

    static std::unique_ptr<Buildable> BuildCall(
        AstVisitor *visitor,
        Module *mod,
        const std::shared_ptr<AstExpression> &target,
        uint8_t nargs
    );

    static std::unique_ptr<Buildable> BuildMethodCall(
        AstVisitor *visitor,
        Module *mod,
        const std::shared_ptr<AstMember> &target,
        const std::vector<std::shared_ptr<AstArgument>> &args
    );

    static std::unique_ptr<Buildable> LoadMemberFromHash(AstVisitor *visitor, Module *mod, uint32_t hash);

    static std::unique_ptr<Buildable> StoreMemberFromHash(AstVisitor *visitor, Module *mod, uint32_t hash);

    static std::unique_ptr<Buildable> LoadMemberAtIndex(AstVisitor *visitor, Module *mod, int dm_index);

    static std::unique_ptr<Buildable> StoreMemberAtIndex(AstVisitor *visitor, Module *mod, int dm_index);

    /** Compiler a standard if-then-else statement into the program.
        If the `else` expression is nullptr it will be omitted.
    */
    static std::unique_ptr<Buildable> CreateConditional(
        AstVisitor *visitor,
        Module *mod,
        AstStatement *cond,
        AstStatement *then_part,
        AstStatement *else_part
    );

    /** Standard evaluation order. Load left into register 0,
        then load right into register 1.
        Rinse and repeat.
    */
    static std::unique_ptr<Buildable> LoadLeftThenRight(AstVisitor *visitor, Module *mod, ExprInfo info);
    /** Handles the right side before the left side. Used in the case that the
        right hand side is an expression, but the left hand side is just a value.
        If the left hand side is a function call, the right hand side will have to
        be temporarily stored on the stack.
    */
    static std::unique_ptr<Buildable> LoadRightThenLeft(AstVisitor *visitor, Module *mod, ExprInfo info);
    /** Loads the left hand side and stores it on the stack.
        Then, the right hand side is loaded into a register,
        and the result is computed.
    */
    static std::unique_ptr<Buildable> LoadLeftAndStore(AstVisitor *visitor, Module *mod, ExprInfo info);
    /** Build a binary operation such as ADD, SUB, MUL, etc. */
    static std::unique_ptr<Buildable> BuildBinOp(uint8_t opcode, AstVisitor *visitor, Module *mod, JitCompiler::ExprInfo info);
    /** Pops from the stack N times. If N is greater than 1,
        the POP_N instruction is generated. Otherwise, the POP
        instruction is generated.
    */
    static std::unique_ptr<Buildable> PopStack(AstVisitor *visitor, int amt);

public:
    JitCompiler(AstIterator *ast_iterator, CompilationUnit *compilation_unit);
    JitCompiler(const JitCompiler &other);

    std::unique_ptr<BytecodeChunk> Compile();
};

} // namespace hyperion::compiler

#endif
