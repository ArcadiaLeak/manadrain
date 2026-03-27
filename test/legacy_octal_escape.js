A// LEGACY OCTAL ESCAPE SEQUENCES (non‑strict mode only)
// They are deprecated – avoid using them in new code.

// 1. Octal escapes in strings: \ followed by 1–3 octal digits
let greeting = "\110\145\154\154\157\41"; // octal: H e l l o !
console.log(greeting); // Output: Hello!

// 2. Copyright symbol (octal 251)
let copyright = "Copyright \251 2025";
console.log(copyright); // Output: Copyright © 2025

// 3. Octal 123 = 'S'
let letter = "Octal 123 gives: \123";
console.log(letter); // Output: Octal 123 gives: S

// 4. Legacy numeric octal literal (also deprecated)
let octalNumber = 0123; // equals decimal 83
console.log("Legacy octal literal 0123 =", octalNumber); // Output: 83

// Note: In strict mode ('use strict';) these would cause syntax errors.
