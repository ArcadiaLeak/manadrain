module glslgram.storage_qualifier;

import bison;

private enum string[] RHSALT = [
  "CONST", "INOUT", "IN", "OUT", "CENTROID", "UNIFORM", "TILEIMAGEEXT",
  "SHARED", "BUFFER", "ATTRIBUTE", "VARYING", "PATCH", "SAMPLE",
  "HITATTRNV", "HITOBJECTATTRNV", "HITOBJECTATTREXT", "HITATTREXT",
  "PAYLOADNV", "PAYLOADEXT", "PAYLOADINNV", "PAYLOADINEXT", "CALLDATANV",
  "CALLDATAEXT", "CALLDATAINNV", "CALLDATAINEXT", "COHERENT",
  "DEVICECOHERENT", "QUEUEFAMILYCOHERENT", "WORKGROUPCOHERENT",
  "SUBGROUPCOHERENT", "NONPRIVATE", "SHADERCALLCOHERENT", "VOLATILE",
  "RESTRICT", "READONLY", "WRITEONLY", "NONTEMPORAL", "SUBROUTINE"
];

auto storage_qualifier() {
  declare_sym(symbol_get("storage_qualifier"), symbol_class_.nterm_sym);

  static foreach (tag; RHSALT) {
    grammar_current_rule_begin(symbol_get("storage_qualifier"));
    grammar_current_rule_symbol_append(symbol_get(tag));
    grammar_current_rule_end();
  }

  grammar_current_rule_begin(symbol_get("storage_qualifier"));
  grammar_current_rule_symbol_append(symbol_get("SUBROUTINE"));
  grammar_current_rule_symbol_append(symbol_get("LEFT_PAREN"));
  grammar_current_rule_symbol_append(symbol_get("type_name_list"));
  grammar_current_rule_symbol_append(symbol_get("RIGHT_PAREN"));
  grammar_current_rule_end();

  grammar_current_rule_begin(symbol_get("storage_qualifier"));
  grammar_current_rule_symbol_append(symbol_get("TASKPAYLOADWORKGROUPEXT"));
  grammar_current_rule_end();
}
