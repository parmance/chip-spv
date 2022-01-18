// LLVM IR pass to convert calls to the CUDA/HIP printf() to OpenCL/SPIR-V
// compatible ones.
//
// (c) 2021 Pekka Jääskeläinen / Parmance for Argonne National Laboratory
//
// SPIRV-LLVM translator generates a wrong pointer address space to printf
// format string if it's not the correct (constant) one in the input. This
// pass moves the format string to constant address space before we pass the IR
// to SPIRV emission.

#include "HipPrintf.h"

#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Pass.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"

using namespace llvm;

class HipPrintfToOpenCLPrintfLegacyPass : public ModulePass {
public:
  static char ID;
  HipPrintfToOpenCLPrintfLegacyPass() : ModulePass(ID) {}

  bool runOnModule(Module &M) override {
    return false;
  }

  StringRef getPassName() const override {
    return "Convert printf calls to OpenCL compatible ones.";
  }
};

char HipPrintfToOpenCLPrintfLegacyPass::ID = 0;
static RegisterPass<HipPrintfToOpenCLPrintfLegacyPass>
    X("hip-printf", "Convert printf calls to OpenCL compatible ones.");

// Converts the address space of the format string to OpenCL compatible one
// by creating a copy of it in the module global scope.
//
// Also counts the number of format args for replacing the return
// value of the printf() call with it for CUDA emulation.
Value* convertFormatString(Value *HipFmtStrArg, Instruction *Before,
                           unsigned &NumberOfFormatSpecs) {

  Module *M = Before->getParent()->getParent()->getParent();

  Type *Int8Ty = IntegerType::get(M->getContext(), 8);
  ConstantExpr *CE = cast<ConstantExpr>(HipFmtStrArg);

  Value *FmtStrOpr = CE->getOperand(0);

  if (auto GEP = dyn_cast<GetElementPtrInst>(FmtStrOpr)) {
    FmtStrOpr = GEP->getPointerOperand();
  }
  if (auto ASCast = dyn_cast<AddrSpaceCastInst>(FmtStrOpr)) {
    FmtStrOpr = ASCast->getPointerOperand();
  } else if (auto CE = dyn_cast<ConstantExpr>(FmtStrOpr)) {
    if (CE->getOpcode() == llvm::Instruction::AddrSpaceCast) {
      FmtStrOpr = CE->getOperand(0);
    } else {
      assert ("Cannot handle the printf format string!" && false);
    }
  }

  GlobalVariable *OrigFmtStr = cast<GlobalVariable>(FmtStrOpr);

  ConstantDataSequential *FmtStrData =
    cast<ConstantDataSequential>(OrigFmtStr->getInitializer());

  NumberOfFormatSpecs =
    FmtStrData->getAsString().count("%") -
    FmtStrData->getAsString().count("%%");

  GlobalVariable *NewFmtStr = new GlobalVariable(
      *M, OrigFmtStr->getValueType(), true, OrigFmtStr->getLinkage(),
      FmtStrData, OrigFmtStr->getName() + ".cl",
      (GlobalVariable *)nullptr, OrigFmtStr->getThreadLocalMode(),
      SPIRV_OPENCL_PRINTF_FMT_ARG_AS);
  NewFmtStr->copyAttributesFrom(OrigFmtStr);

  IntegerType *Int64Ty = Type::getInt64Ty(M->getContext());
  ConstantInt *Zero = ConstantInt::get(Int64Ty, 0);
  std::array<Constant*, 2> Indices = {Zero, Zero};

  PointerType *PtrTy =
    cast<PointerType>(NewFmtStr->getType()->getScalarType());

  return llvm::ConstantExpr::getGetElementPtr(
    PtrTy->getElementType(), NewFmtStr, Indices);
}


PreservedAnalyses HipPrintfToOpenCLPrintfPass::run(
  Function &F,
  FunctionAnalysisManager &AM) {

  Module *M = F.getParent();
  GlobalValue *Printf = M->getNamedValue("printf");
  GlobalValue *HipPrintf = M->getNamedValue("_hip_printf");

  if (Printf == nullptr) {
    // No printf decl in module, no printf calls in the function.
    return PreservedAnalyses::all();
  }

  auto *Int8Ty = IntegerType::get(F.getContext(), 8);
  auto *Int32Ty = IntegerType::get(F.getContext(), 32);

  PointerType *OCLPrintfFmtArgT =
    PointerType::get(Int8Ty, SPIRV_OPENCL_PRINTF_FMT_ARG_AS);
  FunctionType *OpenCLPrintfTy =
      FunctionType::get(Int32Ty, {OCLPrintfFmtArgT}, true);

  FunctionCallee OpenCLPrintf;

  if (HipPrintf == nullptr) {
    // Create the OpenCL printf which will be used instead. Rename the
    // old one away to _hip_printf.
    Printf->setName("_hip_printf");
    HipPrintf = Printf;
    OpenCLPrintf =
      M->getOrInsertFunction(
        "printf", OpenCLPrintfTy, cast<Function>(HipPrintf)->getAttributes());
  } else {
    OpenCLPrintf = FunctionCallee(OpenCLPrintfTy, Printf);
  }

  SmallPtrSet<Instruction *, 8> EraseList;
  for (auto &BB : F) {
    for (auto &I : BB) {
      if (!isa<CallInst>(I) ||
          (cast<CallInst>(I).getCalledFunction()->getName() != "_hip_printf"))
        continue;
      CallInst &OrigCall = cast<CallInst>(I);
      std::vector<Value *> Args;
      unsigned FmtSpecCount;
      for (auto &OrigArg : OrigCall.args()) {
        if (Args.size() == 0) {
          Args.push_back(
            convertFormatString(OrigArg, &OrigCall, FmtSpecCount));
          continue;
        }
        Args.push_back(OrigArg);
      }
      CallInst *NewCall =
        CallInst::Create(OpenCLPrintf, Args, "", &OrigCall);

      // CHECK: Does this invalidate I?
      OrigCall.replaceAllUsesWith(NewCall);


      // Instead of returning the success/failure from the OpenCL printf(),
      // assume that the parsing succeeds and return the number of format
      // strings. A slight improvement would be to return 0 in case of a
      // failure, but it still would not necessary conform to CUDA nor HIP
      // since it should return the number of valid format replacements?
      IntegerType *Int32Ty = Type::getInt32Ty(M->getContext());
      ConstantInt *RV = ConstantInt::get(Int32Ty, FmtSpecCount);
      NewCall->replaceAllUsesWith(RV);

      EraseList.insert(&I);
    }
  }
  for (auto I : EraseList)
    I->eraseFromParent();
  return EraseList.size() > 0 ?
    PreservedAnalyses::none() : PreservedAnalyses::all();
}

namespace {

extern "C" ::llvm::PassPluginLibraryInfo LLVM_ATTRIBUTE_WEAK
llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "hip-printf",
          LLVM_VERSION_STRING, [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "hip-printf") {
                    FPM.addPass(HipPrintfToOpenCLPrintfPass());
                    return true;
                  }
                  return false;
                });
          }};
}
}
