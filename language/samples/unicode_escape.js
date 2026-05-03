// =============================================================================
// JavaScript Parser Test: Character Escapes in Strings and Identifiers
// =============================================================================

// ---------- 1. Escaped Identifiers (Variable/Function Names) ----------
// Unicode escapes in identifiers (ASCII range)
var \u0061\u0062\u0063 = 123;            // "abc"
console.log(abc === 123);                // true

// Unicode escapes for non-ASCII characters
let \u03C0 = Math.PI;                    // π
console.log(\u03C0 === Math.PI);         // true

// Code point escapes in identifiers (ES6)
const \u{1D400} = "😀";                  // Grinning face
console.log(\u{1D400} === "😀");         // true

// Mixed escapes
var \u0068\u0065\u006C\u006C\u006F = "world";
console.log(hello === "world");          // true

// ---------- 2. Escapes in Property Names ----------
var obj = {};

// Bracket notation with escaped Unicode strings
obj['\u0061\u0062\u0063'] = 456;         // "abc"
console.log(obj.abc === 456);            // true

obj['\u03C0'] = 3.14159;                // π
console.log(obj['\u03C0'] === 3.14159); // true

// Computed property names with escapes (ES6)
var obj2 = {
    ['\u0061\u0062\u0063']: 789,         // abc: 789
    ['\u{1F600}']: "smile"               // 😀: "smile"
};
console.log(obj2.abc === 789);                   // true
console.log(obj2['\u{1F600}'] === "smile");      // true

// Escaped reserved word as property name (bracket notation)
var reserved = {
    ['\u0063\u006C\u0061\u0073\u0073']: "escaped class"
};
console.log(reserved['class'] === "escaped class"); // true

// ---------- 3. String Literal Escapes ----------
// 3.1 Single-quoted string with control characters
var s1 = 'It\'s a test.\nNew line.\tTab.\bBackspace.\fForm feed.\rCarriage return.\vVertical tab.';
console.log(s1.includes('\n'));           // true

// 3.2 Double-quoted string with hex escapes
var s2 = "She said, \"Hello!\"\nAnd then \x48\x65\x6c\x6c\x6f (Hello in hex).";
console.log(s2.includes('\x48\x65\x6c\x6c\x6f')); // true

// 3.3 Unicode escapes (4-digit)
var s3 = "\u0048\u0065\u006C\u006C\u006F \u03A9\u03A8\u03A0"; // "Hello ΩΨΠ"
console.log(s3 === "Hello ΩΨΠ");           // true

// 3.4 Code point escapes (curly braces, ES6)
var s4 = "\u{1F600} \u{1F604}";            // "😀 😄"
console.log(s4 === "😀 😄");                // true

// 3.5 Octal escapes (legacy, non-strict mode only)
// Wrapped in a non-strict function to avoid errors in strict mode
function testOctalEscapes() {
    var octalString = "\101\102\103";      // "ABC" (ASCII octal)
    console.log(octalString === "ABC");    // true
}
testOctalEscapes();

// 3.6 Null character escape
var s5 = "Before\0After";
console.log(s5.indexOf("\0") === 6);       // true

// 3.7 Backslash escape
var s6 = "Backslash: \\";
console.log(s6 === "Backslash: \\");       // true

// 3.8 Line continuation (escaped newline) – not an escape sequence in the same sense,
// but often tested in parsers.
var s7 = "This is a \
long string";
console.log(s7 === "This is a long string"); // true

// 3.9 Template literal escapes (ES6)
var s8 = `Li\`ne1\nLine2\tTabbed\u{1F600}`;
console.log(s8.includes('\n') && s8.includes('\t') && s8.includes('😀')); // true

// 3.10 Combination of escapes
var s9 = "\\\"\n\r\t\b\f\v\x41\u0042\u{43}"; // \" newline cr tab bs ff vt A B C
console.log(s9.length > 10);               // true

// ---------- 4. Escapes in Object Shorthand and Methods (ES6) ----------
var \u0061\u0062\u0063 = 10;               // variable "abc"
var obj3 = { \u0061\u0062\u0063 };         // property shorthand with escaped identifier
console.log(obj3.abc === 10);              // true

// Method definition with escaped name
var obj4 = {
    \u006D\u0065\u0074\u0068\u006F\u0064() { return "called"; }
};
console.log(obj4.method() === "called");   // true

// Computed method name with escapes
var obj5 = {
    ['\u006D\u0065\u0074\u0068\u006F\u0064']() { return "computed"; }
};
console.log(obj5.method() === "computed"); // true

// ---------- 5. Edge Cases ----------
// Escaped line terminator in string (should be ignored)
var s10 = "Hello\
World";
console.log(s10 === "HelloWorld");         // true

console.log("All escape tests passed (if no errors were thrown).");
