module quickjs;

enum JSGCPhase {
  NONE,
  DECREF,
  REMOVE_CYCLES
}

enum JSWknownAtom {
  @("null") NULL = 1, @("false") FALSE, @("true") TRUE, @("if") IF,
  @("else") ELSE, @("return") RETURN, @("var") VAR, @("this") THIS,
  @("delete") DELETE, @("void") VOID, @("typeof") TYPEOF, @("new") NEW,
  @("in") IN, @("instanceof") INSTANCEOF, @("do") DO, @("while") WHILE,
  @("for") FOR, @("break") BREAK, @("continue") CONTINUE, @("switch") SWITCH,
  @("case") CASE, @("default") DEFAULT, @("throw") THROW, @("try") TRY,
  @("catch") CATCH, @("finally") FINALLY, @("function") FUNCTION,
  @("debugger") DEBUGGER, @("with") WITH, @("class") CLASS, @("const") CONST,
  @("enum") ENUM, @("export") EXPORT, @("extends") EXTENDS, @("import") IMPORT,
  @("super") SUPER, @("implements") IMPLEMENTS, @("interface") INTERFACE,
  @("let") LET, @("package") PACKAGE, @("private") PRIVATE,
  @("protected") PROTECTED, @("public") PUBLIC, @("static") STATIC,
  @("yield") YIELD, @("await") AWAIT,
  @("") EMPTY_STRING, @("keys") KEYS, @("size") SIZE, @("length") LENGTH,
  @("fileName") FILENAME, @("lineNumber") LINE_NUMBER,
  @("columnNumber") COLUMN_NUMBER, @("message") MESSAGE, @("cause") CAUSE,
  @("errors") ERRORS, @("stack") STACK, @("name") NAME, @("toString") TO_STRING,
  @("toLocaleString") TO_LOCALE_STRING, @("valueOf") VALUE_OF, @("eval") EVAL,
  @("prototype") PROTOTYPE, @("constructor") CONSTRUCTOR,
  @("configurable") CONFIGURABLE, @("writable") WRITABLE,
  @("enumerable") ENUMERABLE, @("value") VALUE, @("get") GET, @("set") SET,
  @("of") OF, @("__proto__") PROTO, @("undefined") UNDEFINED,
  @("number") NUMBER, @("boolean") BOOLEAN, @("string") STRING,
  @("object") OBJECT, @("symbol") SYMBOL, @("integer") INTEGER,
  @("unknown") UNKNOWN, @("arguments") ARGUMENTS, @("callee") CALLEE,
  @("caller") CALLER, @("<eval>") EVAL_, @("<ret>") RET, @("<var>") VAR_,
  @("<arg_var>") ARGVAR, @("<with>") WITH_, @("lastIndex") LAST_INDEX,
  @("target") TARGET, @("index") INDEX, @("input") INPUT,
  @("defineProperties") DEFINE_PROPERTIES, @("apply") APPLY, @("join") JOIN,
  @("concat") CONCAT, @("split") SPLIT, @("construct") CONSTRUCT,
  @("getPrototypeOf") GET_PROTOTYPE_OF, @("setPrototypeOf") SET_PROTOTYPE_OF,
  @("isExtensible") IS_EXTENSIBLE, @("preventExtensions") PREVENT_EXTENSIONS,
  @("has") HAS, @("deleteProperty") DELETE_PROPERTY,
  @("defineProperty") DEFINE_PROPERTY,
  @("getOwnPropertyDescriptor") GET_OWN_PROPERTY_DESCRIPTOR,
  @("ownKeys") OWN_KEYS, @("add") ADD, @("done") DONE, @("next") NEXT,
  @("values") VALUES, @("source") SOURCE, @("flags") FLAGS, @("global") GLOBAL,
  @("unicode") UNICODE, @("raw") RAW, @("new.target") NEW_TARGET,
  @("this.active_func") THIS_ACTIVE_FUNC, @("<home_object>") HOME_OBJECT,
  @("<computed_field>") COMPUTED_FIELD,
  @("<static_computed_field>") STATIC_COMPUTED_FIELD,
  @("<class_fields_init>") CLASS_FIELDS_INIT, @("<brand>") BRAND,
  @("#constructor") HASH_CONSTRUCTOR, @("as") AS, @("from") FROM,
  @("meta") META, @("*default*") DEFAULT_, @("*") STAR, @("Module") MODULE,
  @("then") THEN, @("resolve") RESOLVE, @("reject") REJECT, @("promise") PROMISE,
  @("proxy") PROXY, @("revoke") REVOKE, @("async") ASYNC, @("exec") EXEC,
  @("groups") GROUPS, @("indices") INDICES, @("status") STATUS, @("reason") REASON,
  @("globalThis") GLOBAL_THIS, @("bigint") BIGINT, @("-0") MINUS_ZERO,
  @("Infinity") INFINITY, @("-Infinity") MINUS_INFINITY, @("NaN") NAN,
  @("hasIndices") HAS_INDICES, @("ignoreCase") IGNORE_CASE, @("multiline") MULTILINE,
  @("dotAll") DOTALL, @("sticky") STICKY, @("unicodeSets") UNICODE_SETS,
  @("not-equal") NOT_EQUAL, @("timed-out") TIMED_OUT, @("ok") OK,
  @("toJSON") TO_JSON, @("maxByteLength") MAX_BYTE_LENGTH, @("Object") OBJECT_,
  @("Array") ARRAY, @("Error") ERROR, @("Number") NUMBER_, @("String") STRING_,
  @("Boolean") BOOLEAN_, @("Symbol") SYMBOL_, @("Arguments") ARGUMENTS_,
  @("Math") MATH, @("JSON") JSON, @("Date") DATE, @("Function") FUNCTION_,
  @("GeneratorFunction") GENERATOR_FUNCTION, @("ForInIterator") FOR_IN_ITERATOR,
  @("RegExp") REGEXP, @("ArrayBuffer") ARRAY_BUFFER,
  @("SharedArrayBuffer") SHARED_ARRAY_BUFFER, @("Uint8ClampedArray") UINT8CLAMPEDARRAY,
  @("Int8Array") INT8ARRAY, @("Uint8Array") UINT8ARRAY, @("Int16Array") INT16ARRAY,
  @("Uint16Array") UINT16ARRAY, @("Int32Array") INT32ARRAY,
  @("Uint32Array") UINT32ARRAY, @("BigInt64Array") BIGINT64ARRAY,
  @("BigUint64Array") BIGUINT64ARRAY, @("Float16Array") FLOAT16ARRAY,
  @("Float32Array") FLOAT32ARRAY, @("Float64Array") FLOAT64ARRAY,
  @("DataView") DATA_VIEW, @("BigInt") BIGINT_, @("WeakRef") WEAKREF,
  @("FinalizationRegistry") FINALIZATION_REGISTRY, @("Map") MAP, @("Set") SET_,
  @("WeakMap") WEAKMAP, @("WeakSet") WEAKSET, @("Iterator") ITERATOR,
  @("Iterator Helper") ITERATOR_HELPER, @("Iterator Concat") ITERATOR_CONCAT,
  @("Iterator Wrap") ITERATOR_WRAP, @("Map Iterator") MAP_ITERATOR,
  @("Set Iterator") SET_ITERATOR, @("Array Iterator") ARRAY_ITERATOR,
  @("String Iterator") STRING_ITERATOR,
  @("RegExp String Iterator") REGEXP_STRING_ITERATOR,
  @("Generator") GENERATOR, @("Proxy") PROXY_, @("Promise") PROMISE_,
  @("PromiseResolveFunction") PROMISE_RESOLVE_FUNCTION,
  @("PromiseRejectFunction") PROMISE_REJECT_FUNCTION,
  @("AsyncFunction") ASYNC_FUNCTION, @("AsyncFunctionResolve") ASYNC_FUNCTION_RESOLVE,
  @("AsyncFunctionReject") ASYNC_FUNCTION_REJECT,
  @("AsyncGeneratorFunction") ASYNC_GENERATOR_FUNCTION,
  @("AsyncGenerator") ASYNC_GENERATOR, @("EvalError") EVAL_ERROR,
  @("RangeError") RANGE_ERROR, @("ReferenceError") REFERENCE_ERROR,
  @("SyntaxError") SYNTAX_ERROR, @("TypeError") TYPE_ERROR, @("URIError") URI_ERROR,
  @("InternalError") INTERNAL_ERROR, @("AggregateError") AGGREGATE_ERROR,
  @("<brand>") PRIVATE_BRAND, @("Symbol.toPrimitive") SYMBOL_TO_PRIMITIVE,
  @("Symbol.iterator") SYMBOL_ITERATOR, @("Symbol.match") SYMBOL_MATCH,
  @("Symbol.matchAll") SYMBOL_MATCH_ALL, @("Symbol.replace") SYMBOL_REPLACE,
  @("Symbol.split") SYMBOL_SPLIT, @("Symbol.toStringTag") SYMBOL_TO_STRING_TAG,
  @("Symbol.isConcatSpreadable") SYMBOL_IS_CONCAT_SPREADABLE,
  @("Symbol.hasInstance") SYMBOL_HAS_INSTANCE, @("Symbol.species") SYMBOL_SPECIES,
  @("Symbol.unscopables") SYMBOL_UNSCOPABLES,
  @("Symbol.asyncIterator") SYMBOL_ASYNC_ITERATOR
}

enum JSClassRange {
  STD,
}

enum JSClass {
  @(JSClassRange.STD) @JSClassShortDef(JSWknownAtom.OBJECT_) OBJECT = 1,
  @(JSClassRange.STD) @JSClassShortDef(JSWknownAtom.ARRAY) ARRAY,
  @(JSClassRange.STD) @JSClassShortDef(JSWknownAtom.ERROR) ERROR,
  @(JSClassRange.STD) @JSClassShortDef(JSWknownAtom.NUMBER_) NUMBER,
  @(JSClassRange.STD) @JSClassShortDef(JSWknownAtom.STRING_) STRING,
  @(JSClassRange.STD) @JSClassShortDef(JSWknownAtom.BOOLEAN_) BOOLEAN,
  @(JSClassRange.STD) @JSClassShortDef(JSWknownAtom.SYMBOL_) SYMBOL,
  @(JSClassRange.STD) @JSClassShortDef(JSWknownAtom.ARGUMENTS_) ARGUMENTS,
  @(JSClassRange.STD) @JSClassShortDef(JSWknownAtom.ARGUMENTS_) MAPPED_ARGUMENTS,
  @(JSClassRange.STD) @JSClassShortDef(JSWknownAtom.DATE) DATE,
  @(JSClassRange.STD) @JSClassShortDef(JSWknownAtom.OBJECT_) MODULE_NS,
  @(JSClassRange.STD) @JSClassShortDef(JSWknownAtom.FUNCTION_) C_FUNCTION,
  @(JSClassRange.STD) @JSClassShortDef(JSWknownAtom.FUNCTION_) BYTECODE_FUNCTION,
  @(JSClassRange.STD) @JSClassShortDef(JSWknownAtom.FUNCTION_) BOUND_FUNCTION,
  @(JSClassRange.STD) @JSClassShortDef(JSWknownAtom.FUNCTION_) C_FUNCTION_DATA,
  @(JSClassRange.STD) @JSClassShortDef(JSWknownAtom.GENERATOR_FUNCTION) GENERATOR_FUNCTION,
  @(JSClassRange.STD) @JSClassShortDef(JSWknownAtom.FOR_IN_ITERATOR) FOR_IN_ITERATOR,
  @(JSClassRange.STD) @JSClassShortDef(JSWknownAtom.REGEXP) REGEXP,
  @(JSClassRange.STD) @JSClassShortDef(JSWknownAtom.ARRAY_BUFFER) ARRAY_BUFFER,
  @(JSClassRange.STD) @JSClassShortDef(JSWknownAtom.SHARED_ARRAY_BUFFER) SHARED_ARRAY_BUFFER,
  @(JSClassRange.STD) @JSClassShortDef(JSWknownAtom.UINT8CLAMPEDARRAY) UINT8C_ARRAY,
  @(JSClassRange.STD) @JSClassShortDef(JSWknownAtom.INT8ARRAY) INT8_ARRAY,
  @(JSClassRange.STD) @JSClassShortDef(JSWknownAtom.UINT8ARRAY) UINT8_ARRAY,
  @(JSClassRange.STD) @JSClassShortDef(JSWknownAtom.INT16ARRAY) INT16_ARRAY,
  @(JSClassRange.STD) @JSClassShortDef(JSWknownAtom.UINT16ARRAY) UINT16_ARRAY,
  @(JSClassRange.STD) @JSClassShortDef(JSWknownAtom.INT32ARRAY) INT32_ARRAY,
  @(JSClassRange.STD) @JSClassShortDef(JSWknownAtom.UINT32ARRAY) UINT32_ARRAY,
  @(JSClassRange.STD) @JSClassShortDef(JSWknownAtom.BIGINT64ARRAY) BIG_INT64_ARRAY,
  @(JSClassRange.STD) @JSClassShortDef(JSWknownAtom.BIGUINT64ARRAY) BIG_UINT64_ARRAY,
  @(JSClassRange.STD) @JSClassShortDef(JSWknownAtom.FLOAT16ARRAY) FLOAT16_ARRAY,
  @(JSClassRange.STD) @JSClassShortDef(JSWknownAtom.FLOAT32ARRAY) FLOAT32_ARRAY,
  @(JSClassRange.STD) @JSClassShortDef(JSWknownAtom.FLOAT64ARRAY) FLOAT64_ARRAY,
  @(JSClassRange.STD) @JSClassShortDef(JSWknownAtom.DATA_VIEW) DATAVIEW,
  @(JSClassRange.STD) @JSClassShortDef(JSWknownAtom.BIGINT_) BIG_INT,
  @(JSClassRange.STD) @JSClassShortDef(JSWknownAtom.MAP) MAP,
  @(JSClassRange.STD) @JSClassShortDef(JSWknownAtom.SET_) SET,
  @(JSClassRange.STD) @JSClassShortDef(JSWknownAtom.WEAKMAP) WEAKMAP,
  @(JSClassRange.STD) @JSClassShortDef(JSWknownAtom.WEAKSET) WEAKSET,
  @(JSClassRange.STD) @JSClassShortDef(JSWknownAtom.ITERATOR) ITERATOR,
  @(JSClassRange.STD) @JSClassShortDef(JSWknownAtom.ITERATOR_CONCAT) ITERATOR_CONCAT,
  @(JSClassRange.STD) @JSClassShortDef(JSWknownAtom.ITERATOR_HELPER) ITERATOR_HELPER,
  @(JSClassRange.STD) @JSClassShortDef(JSWknownAtom.ITERATOR_WRAP) ITERATOR_WRAP,
  @(JSClassRange.STD) @JSClassShortDef(JSWknownAtom.MAP_ITERATOR) MAP_ITERATOR,
  @(JSClassRange.STD) @JSClassShortDef(JSWknownAtom.SET_ITERATOR) SET_ITERATOR,
  @(JSClassRange.STD) @JSClassShortDef(JSWknownAtom.ARRAY_ITERATOR) ARRAY_ITERATOR,
  @(JSClassRange.STD) @JSClassShortDef(JSWknownAtom.STRING_ITERATOR) STRING_ITERATOR,
  @(JSClassRange.STD) @JSClassShortDef(JSWknownAtom.REGEXP_STRING_ITERATOR) REGEXP_STRING_ITERATOR,
  @(JSClassRange.STD) @JSClassShortDef(JSWknownAtom.GENERATOR) GENERATOR,
  @(JSClassRange.STD) @JSClassShortDef(JSWknownAtom.OBJECT_) GLOBAL_OBJECT
}

enum JSAtomType : ubyte {
  STRING = 1,
  GLOBAL_SYMBOL,
  SYMBOL,
  PRIVATE
}

import std.container.dlist;
import std.traits;

class JSRuntime {
  DList!size_t context_list;
  DList!size_t gc_obj_list;
  DList!size_t gc_zero_ref_count_list;
  DList!size_t tmp_obj_list;
  size_t malloc_gc_threshold = 256 * 1024;
  DList!size_t weakref_list;
  ulong[string] atom_hash;
  JSString[] atom_array;

  this() {
    JS_InitAtoms;
    init_class_range!(JSClassRange.STD);
  }

  void JS_InitAtoms() {
    int atom_type;
    static foreach (i; EnumMembers!JSWknownAtom) {
      if (i == JSWknownAtom.PRIVATE_BRAND)
        atom_type = JSAtomType.PRIVATE;
      else if (i >= JSWknownAtom.SYMBOL_TO_PRIMITIVE)
        atom_type = JSAtomType.SYMBOL;
      else
        atom_type = JSAtomType.STRING;
      JS_NewAtomInit(getUDAs!(i, string), atom_type);
    }
  }

  void JS_NewAtomInit(string str, int atom_type) =>
    JS_NewAtom(new JSString(str, 0), atom_type);

  void JS_NewAtom(JSString jsstr, int atom_type) {
    if (atom_type < JSAtomType.SYMBOL) {
      if (jsstr.atom_type == atom_type) {
        assert(0);
      }
      ulong* i = jsstr.str in atom_hash;
      if (i !is null) {
        assert(0);
      }
    } else {
      assert(0);
    }

    import std.range.primitives;
    if (atom_array.empty) {
      atom_array ~= new JSString;
      atom_array.back.atom_type = JSAtomType.SYMBOL;
    }

    JSString p;

    if (jsstr) {
      if (jsstr.atom_type == 0) {
        p = jsstr;
        p.atom_type = atom_type;
      } else {
        assert(0);
      }
    } else {
      assert(0);
    }

    ulong i = atom_array.length++;
    atom_array[i] = p;

    p.atom_type = atom_type;

    if (atom_type != JSAtomType.SYMBOL)
      atom_hash[jsstr.str] = i;
  }

  int init_class_range(JSClassRange class_range)() {
    static foreach (clazz; EnumMembers!JSClass) {
      static if (hasUDA!(clazz, class_range)) {
        assert(0);
      } 
    }

    assert(0);
  }
}

abstract class JSGCObject {
  int ref_count = 1;

  JSObject asObject() => null;
  JSString asString() => null;
  JSStringRope asStringRope() => null;
}

class JSString : JSGCObject {
  bool is_wide_char;
  int atom_type;
  string str;
  int atom_free_index;

  this() {}
  this(string s, bool is_wch) {
    str = s;
    is_wide_char = is_wch;
  }

  override JSString asString() => null;
}

union JSValueUnion {
  int int32;
  double float64;
  long short_big_int;
  JSGCObject ptr;
}

struct JSValue {
  JSValueUnion u;
  long tag;
}

union JSArrayUnion {
  JSValue values;
}

union JSObjectUnion {
  JSArrayUnion[] array;
}

class JSObject : JSGCObject {
  JSObjectUnion u;
  override JSObject asObject() => this;
}

class JSStringRope : JSGCObject {
  override JSStringRope asStringRope() => this;
}

struct JSClassShortDef {
  ulong class_name;
  void function(JSRuntime rt, JSValue val) finalizer;
  void function(JSRuntime rt, JSValue val, JS_MarkFunc mark_func) gc_mark;
}
alias JS_MarkFunc = void function(JSRuntime rt, JSGCObject gco);

void js_array_finalizer(JSRuntime rt, JSValue val) {
  JSObject p = val.u.ptr.asObject;
  assert(0);
}

void js_array_mark(JSRuntime rt, JSValue val, JS_MarkFunc mark_func) {
  JSObject p = val.u.ptr.asObject;
  assert(0);
}

void main(string[] args) {
  JSRuntime rt = new JSRuntime;
}
