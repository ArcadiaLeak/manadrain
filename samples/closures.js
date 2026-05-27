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

// EXAMPLE 2: Accumulator with Reset Capability
function createAccumulator(startValue) {
  let total = startValue || 0;

  // Return multiple functions as an object (closure factory)
  return {
    add: function (value) {
      total = total + value;
      return total;
    },
    subtract: function (value) {
      total = total - value;
      return total;
    },
    get: function () {
      return total;
    },
    reset: function (value) {
      total = value || 0;
      return total;
    },
  };
}

let acc = createAccumulator(50);
console.log("=== Accumulator Example ===");
console.log("Initial:", acc.get()); // 50
console.log("Add 25:", acc.add(25)); // 75
console.log("Subtract 10:", acc.subtract(10)); // 65
console.log("Reset to 0:", acc.reset(0)); // 0
console.log();
