// Define a pass plugin that runs a collection of HIP passes.

#include "HipDefrost.h"
#include "HipDynMem.h"
#include "HipStripCompilerUsed.h"
#include "HipPrintf.h"
#include "HipGlobalVariables.h"
#include "HipTextureLowering.h"

#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Transforms/IPO/Inliner.h"
#include "llvm/Transforms/Scalar/DCE.h"
#include "llvm/Transforms/IPO/GlobalDCE.h"
#include "llvm/Transforms/Scalar/SROA.h"

using namespace llvm;

// A pass that removes noinline and optnone attributes from functions.
class RemoveNoInlineOptNoneAttrsPass
    : public PassInfoMixin<RemoveNoInlineOptNoneAttrsPass> {
public:
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM) {
    for (auto &F : M) {
      F.removeFnAttr(Attribute::NoInline);
      F.removeFnAttr(Attribute::OptimizeNone);
    }
    return PreservedAnalyses::none();
  }
  static bool isRequired() { return true; }
};

static void addFullLinkTimePasses(ModulePassManager &MPM) {
  // Run a collection of passes run at device link time.
  MPM.addPass(HipDynMemExternReplaceNewPass());

  // Prepare device code for texture function lowering which does not yet work
  // on non-inlined code and local variables of hipTextureObject_t type.
  MPM.addPass(RemoveNoInlineOptNoneAttrsPass());
  // Increase getInlineParams argument for more aggressive inlining.
  MPM.addPass(ModuleInlinerWrapperPass(getInlineParams(1000)));
  MPM.addPass(createModuleToFunctionPassAdaptor(SROA()));

  MPM.addPass(HipTextureLoweringPass());

  MPM.addPass(createModuleToFunctionPassAdaptor(HipPrintfToOpenCLPrintfPass()));
  MPM.addPass(createModuleToFunctionPassAdaptor(HipDefrostPass()));
  // This pass must appear after HipDynMemExternReplaceNewPass.
  MPM.addPass(HipGlobalVariablesPass());

  // Remove dead code left over by HIP lowering passes and kept alive by
  // llvm.compiler.used intrinsic variable.
  MPM.addPass(HipStripCompilerUsedPass());
  MPM.addPass(createModuleToFunctionPassAdaptor(DCEPass()));
  MPM.addPass(GlobalDCEPass());
}

extern "C" ::llvm::PassPluginLibraryInfo LLVM_ATTRIBUTE_WEAK
llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "hip-passes",
          LLVM_VERSION_STRING, [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, ModulePassManager &MPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "hip-link-time-passes") {
                    addFullLinkTimePasses(MPM);
                    return true;
                  }
                  return false;
                });
          }};
}
