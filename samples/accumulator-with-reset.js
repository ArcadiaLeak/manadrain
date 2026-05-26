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
