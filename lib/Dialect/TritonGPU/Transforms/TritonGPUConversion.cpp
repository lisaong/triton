#include "triton/Dialect/TritonGPU/Transforms/TritonGPUConversion.h"
#include "triton/Dialect/Triton/IR/Dialect.h"
#include "triton/Dialect/TritonGPU/IR/Dialect.h"
#include <algorithm>
#include <llvm-6.0/llvm/Support/ErrorHandling.h>

using namespace mlir;

//
// TypeConverter
//
TritonGPUTypeConverter::TritonGPUTypeConverter(MLIRContext *context, 
                                               int numThreads)
    : context(context), numThreads(numThreads) {
  // TODO: how does MLIR pick the right conversion?
  addConversion([](Type type) { return type; });
  addConversion([this](RankedTensorType tensorType) -> RankedTensorType {
    MLIRContext *context = this->context;
    int numThreads = this->numThreads;

    llvm::ArrayRef<int64_t> shape = tensorType.getShape();
    Type elementType = tensorType.getElementType();
    int64_t rank = tensorType.getRank();
    int64_t numElements = tensorType.getNumElements();

    // TODO: we should raise exception here.
    assert(numElements > numThreads);
    assert(numElements % numThreads == 0);

    // or assert no encoding?

    // Now we assume:
    //   contiguous = 1, order = 0, 1, 2, ..., 
    llvm::SmallVector<unsigned> threadTileSize(rank, 1); // naive layout
    llvm::SmallVector<unsigned> blockTileSize(rank);
    llvm::SmallVector<unsigned> order(rank);
    int remainingThreads = numThreads;
    for (int64_t dim = 0; dim < rank; ++dim) {
      blockTileSize[dim] = std::clamp(remainingThreads, 1, int(shape[dim]));
      order[dim] = dim;

      remainingThreads /= blockTileSize[dim];
      // TODO: will we need repetition?
    }
    Attribute encoding = triton::gpu::TritonGPUDistributedEncodingAttr::get(
        context, threadTileSize, blockTileSize, order);
    return RankedTensorType::get(shape, elementType, encoding);
  });

  // materailizations
  addArgumentMaterialization([&](OpBuilder &builder, RankedTensorType tensorType,
                                 ValueRange inputs, Location loc) {
    // llvm::errs() << "Trying to materialize target... " << inputs[0] << "\n";
    llvm_unreachable("Not implemented");
    return llvm::None;
  });
  addSourceMaterialization([&](OpBuilder &builder, RankedTensorType tensorType,
                                 ValueRange inputs, Location loc) {
    llvm_unreachable("Not implemented");
    return llvm::None;
  });
  addTargetMaterialization([&](OpBuilder &builder, RankedTensorType tensorType,
                                ValueRange inputs, Location loc) {
    llvm_unreachable("Not implemented");
    // llvm::errs() << "Trying to materialize target... " << inputs[0] << "\n";
    return llvm::None;
  });
}

//
// TritonGPUConversion
//
TritonGPUConversionTarget::TritonGPUConversionTarget(
  MLIRContext &context, TritonGPUTypeConverter &typeConverter)
    : ConversionTarget(context), typeConverter(typeConverter) {
  // Some ops from SCF are illegal
  addIllegalOp<scf::ExecuteRegionOp, scf::ParallelOp, 
               scf::ReduceOp, scf::ReduceReturnOp>();
  
  addDynamicallyLegalDialect<arith::ArithmeticDialect,
                             triton::TritonDialect,
                             triton::gpu::TritonGPUDialect,
                             StandardOpsDialect,
                             scf::SCFDialect>([&](Operation *op) {
    if (typeConverter.isLegal(op))
      return true;
    return false;
  });

  // // We have requirements for the data layouts
  // addDynamicallyLegalOp<triton::DotOp>([](triton::DotOp dotOp) -> bool {
  //   Attribute aEncoding = dotOp.a().getType().cast<RankedTensorType>().getEncoding();
  //   Attribute bEncoding = dotOp.b().getType().cast<RankedTensorType>().getEncoding();
  //   if (aEncoding && aEncoding.isa<triton::gpu::TritonGPUSharedEncodingAttr>() &&
  //       bEncoding && bEncoding.isa<triton::gpu::TritonGPUSharedEncodingAttr>())
  //     return true;
  //   return false;
  // });

}