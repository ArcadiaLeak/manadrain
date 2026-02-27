module glslang.machine_independent.symbol_table;
import glslang;

class TSymbol {
  const(TAnonMember) getAsAnonMember() const => null;
}

class TVariable : TSymbol {
  TType type;
  bool userType;

  this(const TVariable copyOf) {
    type.deepCopy = copyOf.type;
    userType = copyOf.userType;
  }

  TVariable clone() const => new TVariable(this);
}

class TAnonMember : TSymbol {
  int anonId;
  TVariable anonContainer;

  override const(TAnonMember) getAsAnonMember() const => this;

  int getAnonId() const => anonId;
  const(TVariable) getAnonContainer() const => anonContainer;
}

class TSymbolTableLevel {
  import std.typecons;
  
  int anonId;
  bool thisLevel;
  Tuple!(string, string)[] retargetedSymbols;
  TSymbol[string] level;

  TSymbolTableLevel clone() const {
    TSymbolTableLevel symTableLevel = new TSymbolTableLevel;
    symTableLevel.anonId = anonId;
    symTableLevel.thisLevel = thisLevel;
    symTableLevel.retargetedSymbols.length = retargetedSymbols.length;
    symTableLevel.retargetedSymbols[] = retargetedSymbols[];

    bool[] containerCopied = new bool[anonId];
    foreach (name, sym; level) {
      const TAnonMember anon = sym.getAsAnonMember;
      if (anon) {
        if (!containerCopied[anon.getAnonId]) {
          const TVariable container = anon.getAnonContainer;
        }
      }
    }

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
  long currentLevel() const => table.length - 1;

  void copyTable(const TSymbolTable copyOf) {
    assert(adoptedLevels == copyOf.adoptedLevels);

    uniqueId = copyOf.uniqueId;
    noBuiltInRedeclarations = copyOf.noBuiltInRedeclarations;
    separateNameSpaces = copyOf.separateNameSpaces;
    foreach (i; copyOf.adoptedLevels..copyOf.table.length)
      table ~= copyOf.table[i].clone;
  }
}
