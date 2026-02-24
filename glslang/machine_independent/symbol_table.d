module glslang.machine_independent.symbol_table;

class TVariable {}

class TSymbolTableLevel {}

class TSymbolTable {
  enum uint LevelFlagBitOffset = 56;

  int currentLevel() @safe const => cast(int) table.length - 1;
  TSymbolTableLevel[] table;
  long uniqueId;

  enum ulong uniqueIdMask = (1L << LevelFlagBitOffset) - 1;
  enum uint MaxLevelInUniqueID = 127;

  void push() @safe {
    table ~= new TSymbolTableLevel();
    updateUniqueIdLevelFlag();
  }

  void updateUniqueIdLevelFlag() @safe {
    ulong level = cast(uint) currentLevel() > MaxLevelInUniqueID
      ? MaxLevelInUniqueID : currentLevel();
    uniqueId &= uniqueIdMask;
    uniqueId |= (level << LevelFlagBitOffset);
  }
}
