module freetype.ftmodapi;

import freetype;
import freetype.fttypes;

enum FT_DEBUG_HOOK_TRUETYPE = 0;

struct FT_Module_Class {
  FT_ULong module_flags;
  FT_Long module_size;
  const(FT_String*) module_name;
  FT_Fixed module_version;
  FT_Fixed module_requires;

  void* module_interface;

  FT_Module_Constructor module_init;
  FT_Module_Destructor module_done;
  FT_Module_Requester get_interface;
}

alias FT_Module_Interface = FT_Pointer;
alias FT_DebugHook_Func = FT_Error function(void* arg);
alias FT_Module_Constructor = FT_Error function(FT_Module modul);
alias FT_Module_Destructor = FT_Error function(FT_Module modul);
alias FT_Module_Requester = FT_Module_Interface function(
  FT_Module modul,
  const(char*) name
);
