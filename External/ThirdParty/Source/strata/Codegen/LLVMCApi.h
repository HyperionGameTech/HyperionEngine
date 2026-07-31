#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct LLVMOpaqueContext;
struct LLVMOpaqueModule;
struct LLVMOpaqueType;
struct LLVMOpaqueValue;
struct LLVMOpaqueBasicBlock;
struct LLVMOpaqueBuilder;
struct LLVMOpaqueTarget;
struct LLVMOpaqueTargetMachine;
struct LLVMOpaqueMemoryBuffer;
struct LLVMOpaqueTargetData;
struct LLVMOpaqueExecutionEngine;

typedef struct LLVMOpaqueContext* LLVMContextRef;
typedef struct LLVMOpaqueModule* LLVMModuleRef;
typedef struct LLVMOpaqueType* LLVMTypeRef;
typedef struct LLVMOpaqueValue* LLVMValueRef;
typedef struct LLVMOpaqueBasicBlock* LLVMBasicBlockRef;
typedef struct LLVMOpaqueBuilder* LLVMBuilderRef;
typedef struct LLVMOpaqueTarget* LLVMTargetRef;
typedef struct LLVMOpaqueTargetMachine* LLVMTargetMachineRef;
typedef struct LLVMOpaqueMemoryBuffer* LLVMMemoryBufferRef;
typedef struct LLVMOpaqueTargetData* LLVMTargetDataRef;
typedef struct LLVMOpaqueExecutionEngine* LLVMExecutionEngineRef;

typedef int LLVMBool;

void LLVMGetVersion(unsigned* major, unsigned* minor, unsigned* patch);

LLVMContextRef LLVMContextCreate(void);
void LLVMContextDispose(LLVMContextRef c);

LLVMModuleRef LLVMModuleCreateWithNameInContext(const char* moduleId, LLVMContextRef c);
void LLVMDisposeModule(LLVMModuleRef m);
void LLVMDumpModule(LLVMModuleRef m);
char* LLVMPrintModuleToString(LLVMModuleRef m);
void LLVMDisposeMessage(char* message);

LLVMTypeRef LLVMInt1TypeInContext(LLVMContextRef c);
LLVMTypeRef LLVMInt32TypeInContext(LLVMContextRef c);
LLVMTypeRef LLVMInt64TypeInContext(LLVMContextRef c);
LLVMTypeRef LLVMHalfTypeInContext(LLVMContextRef c);
LLVMTypeRef LLVMFloatTypeInContext(LLVMContextRef c);
LLVMTypeRef LLVMDoubleTypeInContext(LLVMContextRef c);
LLVMTypeRef LLVMVoidTypeInContext(LLVMContextRef c);
LLVMTypeRef LLVMVectorType(LLVMTypeRef elementType, unsigned elementCount);

LLVMTypeRef LLVMStructCreateNamed(LLVMContextRef c, const char* name);
LLVMBool LLVMStructSetBody(LLVMTypeRef structTy, LLVMTypeRef* elementTypes, unsigned elementCount, LLVMBool packed);

LLVMTypeRef LLVMFunctionType(LLVMTypeRef returnType, LLVMTypeRef* paramTypes, unsigned paramCount, LLVMBool isVarArg);

LLVMValueRef LLVMConstInt(LLVMTypeRef intTy, unsigned long long n, LLVMBool signExtend);
LLVMValueRef LLVMConstReal(LLVMTypeRef ty, double v);
LLVMValueRef LLVMGetParam(LLVMValueRef func, unsigned index);

LLVMTypeRef LLVMPointerTypeInContext(LLVMContextRef c, unsigned addressSpace);
LLVMValueRef LLVMConstNull(LLVMTypeRef ty);
LLVMValueRef LLVMAddGlobal(LLVMModuleRef m, LLVMTypeRef ty, const char* name);
void LLVMSetInitializer(LLVMValueRef globalVar, LLVMValueRef constantVal);

LLVMValueRef LLVMAddFunction(LLVMModuleRef m, const char* name, LLVMTypeRef functionTy);
LLVMBasicBlockRef LLVMAppendBasicBlockInContext(LLVMContextRef c, LLVMValueRef func, const char* name);

LLVMBuilderRef LLVMCreateBuilderInContext(LLVMContextRef c);
void LLVMDisposeBuilder(LLVMBuilderRef b);
void LLVMPositionBuilderAtEnd(LLVMBuilderRef b, LLVMBasicBlockRef block);

LLVMValueRef LLVMBuildRet(LLVMBuilderRef b, LLVMValueRef v);
LLVMValueRef LLVMBuildRetVoid(LLVMBuilderRef b);
LLVMValueRef LLVMBuildBr(LLVMBuilderRef b, LLVMBasicBlockRef dest);
LLVMValueRef LLVMBuildCondBr(LLVMBuilderRef b, LLVMValueRef If, LLVMBasicBlockRef T, LLVMBasicBlockRef E);
LLVMValueRef LLVMBuildAdd(LLVMBuilderRef b, LLVMValueRef l, LLVMValueRef r, const char* name);
LLVMValueRef LLVMBuildSub(LLVMBuilderRef b, LLVMValueRef l, LLVMValueRef r, const char* name);
LLVMValueRef LLVMBuildMul(LLVMBuilderRef b, LLVMValueRef l, LLVMValueRef r, const char* name);
LLVMValueRef LLVMBuildFAdd(LLVMBuilderRef b, LLVMValueRef l, LLVMValueRef r, const char* name);
LLVMValueRef LLVMBuildFSub(LLVMBuilderRef b, LLVMValueRef l, LLVMValueRef r, const char* name);
LLVMValueRef LLVMBuildFMul(LLVMBuilderRef b, LLVMValueRef l, LLVMValueRef r, const char* name);
LLVMValueRef LLVMBuildAlloca(LLVMBuilderRef b, LLVMTypeRef ty, const char* name);
LLVMValueRef LLVMBuildStore(LLVMBuilderRef b, LLVMValueRef val, LLVMValueRef ptr);
LLVMValueRef LLVMBuildLoad2(LLVMBuilderRef b, LLVMTypeRef ty, LLVMValueRef ptr, const char* name);
LLVMValueRef LLVMBuildCall2(LLVMBuilderRef b, LLVMTypeRef fnTy, LLVMValueRef fn, LLVMValueRef* args, unsigned numArgs, const char* name);

typedef enum {
    LLVMIntEQ = 32, LLVMIntNE, LLVMIntUGT, LLVMIntUGE, LLVMIntULT, LLVMIntULE,
    LLVMIntSGT, LLVMIntSGE, LLVMIntSLT, LLVMIntSLE
} LLVMIntPredicate;

typedef enum {
    LLVMRealPredicateFalse = 0, LLVMRealOEQ = 1, LLVMRealOGT = 2, LLVMRealOGE = 3,
    LLVMRealOLT = 4, LLVMRealOLE = 5, LLVMRealONE = 6, LLVMRealORD = 7,
    LLVMRealUNO = 8, LLVMRealUEQ = 9, LLVMRealUGT = 10, LLVMRealUGE = 11,
    LLVMRealULT = 12, LLVMRealULE = 13, LLVMRealUNE = 14, LLVMRealPredicateTrue = 15
} LLVMRealPredicate;

LLVMValueRef LLVMBuildSDiv(LLVMBuilderRef b, LLVMValueRef l, LLVMValueRef r, const char* name);
LLVMValueRef LLVMBuildUDiv(LLVMBuilderRef b, LLVMValueRef l, LLVMValueRef r, const char* name);
LLVMValueRef LLVMBuildFDiv(LLVMBuilderRef b, LLVMValueRef l, LLVMValueRef r, const char* name);
LLVMValueRef LLVMBuildSRem(LLVMBuilderRef b, LLVMValueRef l, LLVMValueRef r, const char* name);
LLVMValueRef LLVMBuildURem(LLVMBuilderRef b, LLVMValueRef l, LLVMValueRef r, const char* name);
LLVMValueRef LLVMBuildFRem(LLVMBuilderRef b, LLVMValueRef l, LLVMValueRef r, const char* name);
LLVMValueRef LLVMBuildShl(LLVMBuilderRef b, LLVMValueRef l, LLVMValueRef r, const char* name);
LLVMValueRef LLVMBuildLShr(LLVMBuilderRef b, LLVMValueRef l, LLVMValueRef r, const char* name);
LLVMValueRef LLVMBuildAShr(LLVMBuilderRef b, LLVMValueRef l, LLVMValueRef r, const char* name);
LLVMValueRef LLVMBuildAnd(LLVMBuilderRef b, LLVMValueRef l, LLVMValueRef r, const char* name);
LLVMValueRef LLVMBuildOr(LLVMBuilderRef b, LLVMValueRef l, LLVMValueRef r, const char* name);
LLVMValueRef LLVMBuildXor(LLVMBuilderRef b, LLVMValueRef l, LLVMValueRef r, const char* name);
LLVMValueRef LLVMBuildNeg(LLVMBuilderRef b, LLVMValueRef v, const char* name);
LLVMValueRef LLVMBuildNot(LLVMBuilderRef b, LLVMValueRef v, const char* name);
LLVMValueRef LLVMBuildFNeg(LLVMBuilderRef b, LLVMValueRef v, const char* name);
LLVMValueRef LLVMBuildICmp(LLVMBuilderRef b, LLVMIntPredicate pred, LLVMValueRef l, LLVMValueRef r, const char* name);
LLVMValueRef LLVMBuildFCmp(LLVMBuilderRef b, LLVMRealPredicate pred, LLVMValueRef l, LLVMValueRef r, const char* name);
LLVMValueRef LLVMBuildSIToFP(LLVMBuilderRef b, LLVMValueRef v, LLVMTypeRef destTy, const char* name);
LLVMValueRef LLVMBuildUIToFP(LLVMBuilderRef b, LLVMValueRef v, LLVMTypeRef destTy, const char* name);
LLVMValueRef LLVMBuildFPToSI(LLVMBuilderRef b, LLVMValueRef v, LLVMTypeRef destTy, const char* name);
LLVMValueRef LLVMBuildFPToUI(LLVMBuilderRef b, LLVMValueRef v, LLVMTypeRef destTy, const char* name);
LLVMValueRef LLVMBuildIntCast2(LLVMBuilderRef b, LLVMValueRef v, LLVMTypeRef destTy, LLVMBool isSigned, const char* name);

LLVMValueRef LLVMGetUndef(LLVMTypeRef ty);
LLVMValueRef LLVMBuildInsertValue(LLVMBuilderRef b, LLVMValueRef agg, LLVMValueRef val, unsigned index, const char* name);
LLVMValueRef LLVMBuildExtractValue(LLVMBuilderRef b, LLVMValueRef agg, unsigned index, const char* name);
LLVMValueRef LLVMBuildGEP2(LLVMBuilderRef b, LLVMTypeRef ty, LLVMValueRef pointer, LLVMValueRef* indices, unsigned numIndices, const char* name);
LLVMValueRef LLVMBuildPhi(LLVMBuilderRef b, LLVMTypeRef ty, const char* name);
void LLVMAddIncoming(LLVMValueRef phiNode, LLVMValueRef* incomingValues, LLVMBasicBlockRef* incomingBlocks, unsigned count);
LLVMBasicBlockRef LLVMGetInsertBlock(LLVMBuilderRef b);
void LLVMPositionBuilderBefore(LLVMBuilderRef b, LLVMValueRef inst);
LLVMValueRef LLVMGetFirstInstruction(LLVMBasicBlockRef bb);
LLVMValueRef LLVMGetBasicBlockTerminator(LLVMBasicBlockRef bb);

LLVMBool LLVMVerifyModule(LLVMModuleRef m, int verifierAction, char** outMessage);

void LLVMInitializeX86TargetInfo(void);
void LLVMInitializeX86Target(void);
void LLVMInitializeX86TargetMC(void);
void LLVMInitializeX86AsmPrinter(void);
void LLVMInitializeAArch64TargetInfo(void);
void LLVMInitializeAArch64Target(void);
void LLVMInitializeAArch64TargetMC(void);
void LLVMInitializeAArch64AsmPrinter(void);

typedef enum {
    LLVMCodeGenLevelNone = 0, LLVMCodeGenLevelLess = 1,
    LLVMCodeGenLevelDefault = 2, LLVMCodeGenLevelAggressive = 3
} LLVMCodeGenOptLevel;

typedef enum {
    LLVMRelocDefault = 0, LLVMRelocStatic = 1,
    LLVMRelocPIC = 2, LLVMRelocDynamicNoPic = 3
} LLVMRelocMode;

typedef enum {
    LLVMCodeModelDefault = 0, LLVMCodeModelJITDefault = 1, LLVMCodeModelTiny = 2,
    LLVMCodeModelSmall = 3, LLVMCodeModelKernel = 4,
    LLVMCodeModelMedium = 5, LLVMCodeModelLarge = 6
} LLVMCodeModel;

typedef enum {
    LLVMAssemblyFile = 0, LLVMObjectFile = 1
} LLVMCodeGenFileType;

LLVMBool LLVMGetTargetFromTriple(const char* triple, LLVMTargetRef* target, char** errorMessage);
char* LLVMGetDefaultTargetTriple(void);
char* LLVMGetHostCPUName(void);
LLVMTargetMachineRef LLVMCreateTargetMachine(LLVMTargetRef t, const char* triple, const char* cpu,
                                             const char* features, LLVMCodeGenOptLevel level,
                                             LLVMRelocMode reloc, LLVMCodeModel codeModel);
void LLVMDisposeTargetMachine(LLVMTargetMachineRef t);
LLVMTargetDataRef LLVMCreateTargetDataLayout(LLVMTargetMachineRef t);
char* LLVMCopyStringRepOfTargetData(LLVMTargetDataRef td);
void LLVMDisposeTargetData(LLVMTargetDataRef td);
void LLVMSetDataLayout(LLVMModuleRef m, const char* dataLayout);
void LLVMSetTarget(LLVMModuleRef m, const char* triple);
LLVMBool LLVMTargetMachineEmitToFile(LLVMTargetMachineRef t, LLVMModuleRef m, const char* filename,
                                     LLVMCodeGenFileType codegen, char** errorMessage);
LLVMBool LLVMTargetMachineEmitToMemoryBuffer(LLVMTargetMachineRef t, LLVMModuleRef m, LLVMCodeGenFileType codegen,
                                             char** errorMessage, LLVMMemoryBufferRef* outMemBuf);
void LLVMDisposeMemoryBuffer(LLVMMemoryBufferRef memBuf);

LLVMBool LLVMCreateExecutionEngineForModule(LLVMExecutionEngineRef* outEe, LLVMModuleRef m, char** outError);
void LLVMDisposeExecutionEngine(LLVMExecutionEngineRef ee);
uint64_t LLVMGetFunctionAddress(LLVMExecutionEngineRef ee, const char* name);
uint64_t LLVMGetGlobalValueAddress(LLVMExecutionEngineRef ee, const char* name);
LLVMValueRef LLVMGetNamedFunction(LLVMModuleRef m, const char* name);
void LLVMAddGlobalMapping(LLVMExecutionEngineRef ee, LLVMValueRef global, void* addr);

#define kReturnStatusAction 1

#ifdef __cplusplus
}
#endif
