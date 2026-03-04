module quickjs;

import std.container.dlist;

enum JSGCPhase {
  NONE,
  DECREF,
  REMOVE_CYCLES
}

enum JSWknownAtom : uint {
  @("null") NULL, @("false") FALSE, @("true") TRUE, @("if") IF,
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

class JSRuntime {
  DList!size_t context_list;
  DList!size_t gc_obj_list;
  DList!size_t gc_zero_ref_count_list;
  DList!size_t tmp_obj_list;
  size_t malloc_gc_threshold = 256 * 1024;
  DList!size_t weakref_list;

  int InitAtoms() {
    assert(0);
  }
}

void main(string[] args) {
  
}
