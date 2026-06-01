#include <array>

#include <binaryen-c.h>

// "hello world" type example: create a function that adds two i32s and returns
// the result

int main() {
  BinaryenModuleRef module = BinaryenModuleCreate();
  BinaryenSetMemory(module, 1, -1, "memory", nullptr, nullptr, nullptr, nullptr,
                    nullptr, 0, 0, 0, "memory");
  BinaryenAddGlobal(module, "bump_ptr", BinaryenTypeInt32(), 1,
                    BinaryenConst(module, BinaryenLiteralInt32(0)));

  {
    std::array params{std::to_array({BinaryenTypeInt32()})};
    std::array locals{std::to_array(
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
         BinaryenIf(
             module,
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
             BinaryenReturn(module,
                            BinaryenConst(module, BinaryenLiteralInt32(0))),
             nullptr)})};
    std::array body{std::to_array(
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

    BinaryenFunctionRef func = BinaryenAddFunction(
        module, "bump_alloc", BinaryenTypeCreate(params.data(), params.size()),
        BinaryenTypeInt32(), locals.data(), locals.size(),
        BinaryenBlock(module, nullptr, body.data(), body.size(),
                      BinaryenTypeAuto()));
    BinaryenAddFunctionExport(module, "bump_alloc", "bump_alloc");

    BinaryenFunctionSetLocalName(func, 0, "size");

    BinaryenFunctionSetLocalName(func, 1, "new_ptr");
    BinaryenFunctionSetLocalName(func, 2, "final_page");
    BinaryenFunctionSetLocalName(func, 3, "cur_pages");
    BinaryenFunctionSetLocalName(func, 4, "req_pages");
    BinaryenFunctionSetLocalName(func, 5, "target");
    BinaryenFunctionSetLocalName(func, 6, "old_ptr");
  }

  BinaryenModulePrint(module);
  BinaryenModuleDispose(module);
  return 0;
}
