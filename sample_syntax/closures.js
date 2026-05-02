//[[
//    Closures in JavaScript: A function that captures and remembers variables from its enclosing scope
//    even after that scope has finished executing.
//]]

// EXAMPLE 1: Basic Counter (Most Common Closure Example)
function createCounter(initialValue) {
    let count = initialValue || 0;
    
    // This inner function "closes over" the 'count' variable
    return function() {
        count = count + 1;
        return count;
    };
}

// Create multiple independent counters
const counter1 = createCounter(10);
const counter2 = createCounter(100);

console.log("=== Basic Counter Example ===");
console.log(counter1());  // 11
console.log(counter1());  // 12
console.log(counter2());  // 101
console.log(counter2());  // 102
console.log();


// EXAMPLE 2: Accumulator with Reset Capability
function createAccumulator(startValue) {
    let total = startValue || 0;
    
    // Return multiple functions as an object (closure factory)
    return {
        add: function(value) {
            total = total + value;
            return total;
        },
        subtract: function(value) {
            total = total - value;
            return total;
        },
        get: function() {
            return total;
        },
        reset: function(value) {
            total = value || 0;
            return total;
        }
    };
}

const acc = createAccumulator(50);
console.log("=== Accumulator Example ===");
console.log("Initial:", acc.get());      // 50
console.log("Add 25:", acc.add(25));     // 75
console.log("Subtract 10:", acc.subtract(10));  // 65
console.log("Reset to 0:", acc.reset(0));       // 0
console.log();


// EXAMPLE 3: Private Variable with Getter/Setter (Encapsulation)
function createPerson(name, age) {
    // Private variables
    let _name = name;
    let _age = age;
    
    // Return closure functions that manage private state
    return {
        getName: function() { return _name; },
        getAge: function() { return _age; },
        setAge: function(newAge) {
            if (newAge > 0) {
                _age = newAge;
                return true;
            }
            return false;
        },
        birthday: function() {
            _age = _age + 1;
            return `${_name} is now ${_age} years old!`;
        },
        toString: function() {
            return `Person: ${_name}, Age: ${_age}`;
        }
    };
}

const john = createPerson("John", 30);
const jane = createPerson("Jane", 25);

console.log("=== Private Variables / Encapsulation Example ===");
console.log(john.getName());           // John
console.log(john.getAge());            // 30
console.log(john.birthday());          // John is now 31 years old!
console.log(jane.toString());          // Person: Jane, Age: 25
console.log(john.setAge(35));          // true
console.log(john.toString());          // Person: John, Age: 35
console.log();


// EXAMPLE 4: Function Factory with Memoization (Caching)
function createMemoizedFunction(fn) {
    const cache = {};
    
    return function(x) {
        if (cache[x] !== undefined) {
            console.log("Cache hit for value:", x);
            return cache[x];
        } else {
            console.log("Computing value for:", x);
            const result = fn(x);
            cache[x] = result;
            return result;
        }
    };
}

// Expensive computation example
function expensiveSquare(x) {
    // Simulate expensive operation
    let sum = 0;
    for (let i = 1; i <= 1000000; i++) { sum = sum + i; }
    return x * x;
}

const memoizedSquare = createMemoizedFunction(expensiveSquare);

console.log("=== Memoization Example ===");
console.log("Result:", memoizedSquare(5));   // Computes
console.log("Result:", memoizedSquare(5));   // Cache hit
console.log("Result:", memoizedSquare(10));  // Computes
console.log("Result:", memoizedSquare(10));  // Cache hit
console.log();


// EXAMPLE 5: Event Handler with State (Callback Closure)
function createButtonHandler(buttonId) {
    let clickCount = 0;
    
    // This closure remembers which button it belongs to
    return function() {
        clickCount = clickCount + 1;
        console.log(`Button [${buttonId}] clicked ${clickCount} time(s)`);
        
        // Return true when clicked 5 times
        return clickCount >= 5;
    };
}

const button1Handler = createButtonHandler("Start");
const button2Handler = createButtonHandler("Stop");

console.log("=== Event Handler Example ===");
for (let i = 1; i <= 7; i++) {
    button1Handler();
    if (i === 3) button2Handler();
    if (i === 6) button2Handler();
}
console.log();


// EXAMPLE 6: Iterator Generator (Closures as Iterators)
function range(start, finish, step) {
    step = step || (start <= finish ? 1 : -1);
    let current = start - step;
    
    // Return iterator closure
    return function() {
        current = current + step;
        if ((step > 0 && current <= finish) || (step < 0 && current >= finish)) {
            return current;
        }
        return null;
    };
}

console.log("=== Iterator Example ===");
console.log("Counting up:");
for (let i = range(1, 5); ;) {
    const val = i();
    if (val === null) break;
    console.log("  ", val);
}

console.log("Counting down:");
for (let i = range(10, 6, -1); ;) {
    const val = i();
    if (val === null) break;
    console.log("  ", val);
}
console.log();


// EXAMPLE 7: Function with Private Helper (Partial Application)
function createLogger(prefix) {
    // Private helper function
    function formatMessage(level, message) {
        const now = new Date();
        const timeStr = `${now.getHours().toString().padStart(2, '0')}:${now.getMinutes().toString().padStart(2, '0')}:${now.getSeconds().toString().padStart(2, '0')}`;
        return `[${timeStr}] ${level}: ${message}`;
    }
    
    // Return specialized logging functions
    return {
        info: function(msg) {
            console.log(formatMessage("INFO", prefix + " " + msg));
        },
        error: function(msg) {
            console.log(formatMessage("ERROR", prefix + " " + msg));
        },
        debug: function(msg) {
            console.log(formatMessage("DEBUG", prefix + " " + msg));
        }
    };
}

const appLogger = createLogger("APP");
const dbLogger = createLogger("DB");

console.log("=== Logger Example ===");
appLogger.info("Application started");
dbLogger.error("Connection failed");
appLogger.debug("Processing request #1234");
dbLogger.info("Retrying connection...");
console.log();


// EXAMPLE 8: Simple State Machine using Closure
function createLightSwitch() {
    let state = "off";  // Private state: off, on, broken
    
    return {
        toggle: function() {
            if (state === "broken") {
                console.log("Light is broken, cannot toggle!");
                return false;
            }
            
            if (state === "off") {
                state = "on";
                console.log("Light turned ON");
            } else if (state === "on") {
                state = "off";
                console.log("Light turned OFF");
            }
            return true;
        },
        
        "break": function() {
            if (state !== "broken") {
                state = "broken";
                console.log("Light is now BROKEN!");
            }
        },
        
        getState: function() { return state; }
    };
}

const livingRoomLight = createLightSwitch();
console.log("=== State Machine Example ===");
livingRoomLight.toggle();   // ON
livingRoomLight.toggle();   // OFF
livingRoomLight.toggle();   // ON
livingRoomLight["break"]();    // BROKEN
livingRoomLight.toggle();   // Can't toggle, broken
console.log("Final state:", livingRoomLight.getState());

console.log("\n" + "=".repeat(50));
console.log("Closures Demo Complete!");
console.log("=" + "=".repeat(49));