module glslang.machine_independent.symbol_table;

class TVariable {}

class TSymbolTableLevel {
  import std.typecons;
  
  int anonId;
  bool thisLevel;
  Tuple!(string, string)[] retargetedSymbols;

  TSymbolTableLevel clone() const {
    TSymbolTableLevel symTableLevel = new TSymbolTableLevel;
    symTableLevel.anonId = anonId;
    symTableLevel.thisLevel = thisLevel;
    symTableLevel.retargetedSymbols.length = retargetedSymbols.length;
    symTableLevel.retargetedSymbols[] = retargetedSymbols[];

    //

    return symTableLevel;
  }
}

class TSymbolTable {
  enum uint LevelFlagBitOffset = 56;

  TSymbolTableLevel[] table;
  long uniqueId;
  bool noBuiltInRedeclarations;
  bool separateNameSpaces;
  uint adoptedLevels;

  enum ulong uniqueIdMask = (1L << LevelFlagBitOffset) - 1;
  enum uint MaxLevelInUniqueID = 127;

  void push() {
    table ~= new TSymbolTableLevel;
    updateUniqueIdLevelFlag;
  }

  void updateUniqueIdLevelFlag() {
    ulong level = cast(uint) currentLevel > MaxLevelInUniqueID
      ? MaxLevelInUniqueID : currentLevel;
    uniqueId &= uniqueIdMask;
    uniqueId |= (level << LevelFlagBitOffset);
  }

  bool isEmpty() => table.length == 0;
  int currentLevel() const => cast(int) table.length - 1;

  void copyTable(const TSymbolTable copyOf) {
    assert(adoptedLevels == copyOf.adoptedLevels);

    uniqueId = copyOf.uniqueId;
    noBuiltInRedeclarations = copyOf.noBuiltInRedeclarations;
    separateNameSpaces = copyOf.separateNameSpaces;
    foreach (i; copyOf.adoptedLevels..copyOf.table.length)
      table ~= copyOf.table[i].clone;
  }
}
