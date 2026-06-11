//[[
//    Closures in JavaScript: A function that captures and remembers variables from its enclosing scope
//    even after that scope has finished executing.
//]]

// EXAMPLE 1: Basic Counter (Most Common Closure Example)
function createCounter(initialValue) {
  let count = initialValue || 0;

  // This inner function "closes over" the 'count' variable
  return function () {
    count = count + 1;
    return count;
  };
}

// Create multiple independent counters
let counter1 = createCounter(10);
let counter2 = createCounter(100);

console.log("=== Basic Counter Example ===");
console.log(counter1()); // 11
console.log(counter1()); // 12
console.log(counter2()); // 101
console.log(counter2()); // 102
console.log();
