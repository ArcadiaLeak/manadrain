#include <algorithm>
#include <array>

#include <binaryen-c.h>

// "hello world" type example: create a function that adds two i32s and returns
// the result

int main() {
  BinaryenModuleRef module = BinaryenModuleCreate();

  std::array segment_names{std::to_array<const char *>({nullptr})};
  std::array segment_datas{std::to_array({"hello world\n"})};
  std::array segment_passives{std::to_array({false})};
  std::array segment_offsets{
      std::to_array({BinaryenConst(module, BinaryenLiteralInt32(0))})};
  std::array segment_sizes{std::to_array<BinaryenIndex>({12})};
  BinaryenSetMemory(module, 1, -1, "memory", segment_names.data(),
                    segment_datas.data(), segment_passives.data(),
                    segment_offsets.data(), segment_sizes.data(),
                    segment_datas.size(), 0, 0, "memory");

  BinaryenAddGlobal(
      module, "bump_ptr", BinaryenTypeInt32(), 1,
      BinaryenConst(module,
                    BinaryenLiteralInt32(std::bit_ceil(std::ranges::fold_left(
                        segment_sizes, 0, std::plus<>{})))));
  BinaryenAddGlobal(module, "current_frame", BinaryenTypeInt32(), 1,
                    BinaryenConst(module, BinaryenLiteralInt32(0)));

  {
    std::array fd_write_params{
        std::to_array({BinaryenTypeInt32(), BinaryenTypeInt32(),
                       BinaryenTypeInt32(), BinaryenTypeInt32()})};
    BinaryenAddFunctionImport(
        module, "fd_write", "wasi_snapshot_preview1", "fd_write",
        BinaryenTypeCreate(fd_write_params.data(), fd_write_params.size()),
        BinaryenTypeInt32());
    BinaryenAddFunctionImport(module, "proc_exit", "wasi_snapshot_preview1",
                              "proc_exit", BinaryenTypeInt32(),
                              BinaryenTypeNone());
  }

  std::array bump_params{std::to_array({BinaryenTypeInt32()})};
  std::array bump_locals{std::to_array(
      {BinaryenTypeInt32(), BinaryenTypeInt32(), BinaryenTypeInt32(),
       BinaryenTypeInt32(), BinaryenTypeInt32(), BinaryenTypeInt32()})};
  std::array grower_loop{std::to_array(
      {BinaryenIf(
           module,
           BinaryenUnary(module, BinaryenEqZInt32(),
                         BinaryenLocalGet(module, 5, BinaryenTypeInt32())),
           BinaryenReturn(module,
                          BinaryenConst(module, BinaryenLiteralInt32(0))),
           nullptr),
       BinaryenBreak(
           module, "grow_doubly",
           BinaryenBinary(
               module, BinaryenLtUInt32(),
               BinaryenLocalTee(
                   module, 5,
                   BinaryenBinary(
                       module, BinaryenShlInt32(),
                       BinaryenLocalGet(module, 5, BinaryenTypeInt32()),
                       BinaryenConst(module, BinaryenLiteralInt32(1))),
                   BinaryenTypeInt32()),
               BinaryenLocalGet(module, 4, BinaryenTypeInt32())),
           nullptr)})};
  std::array grower_branch{std::to_array(
      {BinaryenLocalSet(
           module, 4,
           BinaryenBinary(module, BinaryenAddInt32(),
                          BinaryenLocalGet(module, 2, BinaryenTypeInt32()),
                          BinaryenConst(module, BinaryenLiteralInt32(1)))),
       BinaryenLocalSet(module, 5,
                        BinaryenLocalGet(module, 3, BinaryenTypeInt32())),
       BinaryenLoop(module, "grow_doubly",
                    BinaryenBlock(module, nullptr, grower_loop.data(),
                                  grower_loop.size(), BinaryenTypeAuto())),
       BinaryenIf(module,
                  BinaryenBinary(
                      module, BinaryenEqInt32(),
                      BinaryenMemoryGrow(
                          module,
                          BinaryenBinary(
                              module, BinaryenSubInt32(),
                              BinaryenLocalGet(module, 5, BinaryenTypeInt32()),
                              BinaryenLocalGet(module, 3, BinaryenTypeInt32())),
                          "memory", 0),
                      BinaryenConst(module, BinaryenLiteralInt32(-1))),
                  BinaryenReturn(
                      module, BinaryenConst(module, BinaryenLiteralInt32(0))),
                  nullptr)})};
  std::array bump_body{std::to_array(
      {BinaryenIf(
           module,
           BinaryenUnary(module, BinaryenEqZInt32(),
                         BinaryenLocalGet(module, 0, BinaryenTypeInt32())),
           BinaryenReturn(module, BinaryenGlobalGet(module, "bump_ptr",
                                                    BinaryenTypeInt32())),
           nullptr),
       BinaryenIf(
           module,
           BinaryenBinary(
               module, BinaryenLtUInt32(),
               BinaryenLocalTee(
                   module, 1,
                   BinaryenBinary(
                       module, BinaryenAddInt32(),
                       BinaryenGlobalGet(module, "bump_ptr",
                                         BinaryenTypeInt32()),
                       BinaryenLocalGet(module, 0, BinaryenTypeInt32())),
                   BinaryenTypeInt32()),
               BinaryenGlobalGet(module, "bump_ptr", BinaryenTypeInt32())),
           BinaryenReturn(module,
                          BinaryenConst(module, BinaryenLiteralInt32(0))),
           nullptr),
       BinaryenIf(
           module,
           BinaryenBinary(
               module, BinaryenGeUInt32(),
               BinaryenLocalTee(
                   module, 2,
                   BinaryenBinary(
                       module, BinaryenShrUInt32(),
                       BinaryenBinary(
                           module, BinaryenSubInt32(),
                           BinaryenLocalGet(module, 1, BinaryenTypeInt32()),
                           BinaryenConst(module, BinaryenLiteralInt32(1))),
                       BinaryenConst(module, BinaryenLiteralInt32(16))),
                   BinaryenTypeInt32()),
               BinaryenLocalTee(module, 3,
                                BinaryenMemorySize(module, "memory", 0),
                                BinaryenTypeInt32())),
           BinaryenBlock(module, nullptr, grower_branch.data(),
                         grower_branch.size(), BinaryenTypeAuto()),
           nullptr),
       BinaryenLocalSet(
           module, 6,
           BinaryenGlobalGet(module, "bump_ptr", BinaryenTypeInt32())),
       BinaryenGlobalSet(module, "bump_ptr",
                         BinaryenLocalGet(module, 1, BinaryenTypeInt32())),
       BinaryenReturn(module,
                      BinaryenLocalGet(module, 6, BinaryenTypeInt32()))})};

  BinaryenFunctionRef bump_func = BinaryenAddFunction(
      module, "bump_alloc",
      BinaryenTypeCreate(bump_params.data(), bump_params.size()),
      BinaryenTypeInt32(), bump_locals.data(), bump_locals.size(),
      BinaryenBlock(module, nullptr, bump_body.data(), bump_body.size(),
                    BinaryenTypeAuto()));
  BinaryenFunctionSetLocalName(bump_func, 0, "size");
  BinaryenFunctionSetLocalName(bump_func, 1, "new_ptr");
  BinaryenFunctionSetLocalName(bump_func, 2, "final_page");
  BinaryenFunctionSetLocalName(bump_func, 3, "cur_pages");
  BinaryenFunctionSetLocalName(bump_func, 4, "req_pages");
  BinaryenFunctionSetLocalName(bump_func, 5, "target");
  BinaryenFunctionSetLocalName(bump_func, 6, "old_ptr");

  std::array alloc_operands{
      std::to_array({BinaryenConst(module, BinaryenLiteralInt32(16))})};
  std::array write_operands{std::to_array(
      {BinaryenConst(module, BinaryenLiteralInt32(1)),
       BinaryenLocalGet(module, 0, BinaryenTypeInt32()),
       BinaryenConst(module, BinaryenLiteralInt32(1)),
       BinaryenBinary(module, BinaryenAddInt32(),
                      BinaryenLocalGet(module, 0, BinaryenTypeInt32()),
                      BinaryenConst(module, BinaryenLiteralInt32(8)))})};
  std::array exit_operands{
      std::to_array({BinaryenConst(module, BinaryenLiteralInt32(0))})};
  std::array main_body{std::to_array(
      {BinaryenStore(module, 4, 0, 0,
                     BinaryenLocalTee(module, 0,
                                      BinaryenCall(module, "bump_alloc",
                                                   alloc_operands.data(),
                                                   alloc_operands.size(),
                                                   BinaryenTypeInt32()),
                                      BinaryenTypeInt32()),
                     BinaryenConst(module, BinaryenLiteralInt32(0)),
                     BinaryenTypeInt32(), "memory"),
       BinaryenStore(
           module, 4, 0, 0,
           BinaryenBinary(module, BinaryenAddInt32(),
                          BinaryenLocalGet(module, 0, BinaryenTypeInt32()),
                          BinaryenConst(module, BinaryenLiteralInt32(4))),
           BinaryenConst(module, BinaryenLiteralInt32(12)), BinaryenTypeInt32(),
           "memory"),
       BinaryenDrop(module,
                    BinaryenCall(module, "fd_write", write_operands.data(),
                                 write_operands.size(), BinaryenTypeInt32())),
       BinaryenCall(module, "proc_exit", exit_operands.data(),
                    exit_operands.size(), BinaryenTypeNone())})};
  std::array main_locals{std::to_array({BinaryenTypeInt32()})};
  BinaryenFunctionRef main_func = BinaryenAddFunction(
      module, "main", BinaryenTypeNone(), BinaryenTypeNone(),
      main_locals.data(), main_locals.size(),
      BinaryenBlock(module, nullptr, main_body.data(), main_body.size(),
                    BinaryenTypeAuto()));
  BinaryenFunctionSetLocalName(main_func, 0, "ioptr");
  BinaryenAddFunctionExport(module, "main", "_start");

  BinaryenModulePrint(module);
  BinaryenModuleDispose(module);
  return 0;
}
