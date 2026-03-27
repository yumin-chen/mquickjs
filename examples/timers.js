print("Start timers example...");

setTimeout(function() {
    print("Timer 1 (500ms) expired");
}, 500);

setTimeout(function() {
    print("Timer 2 (200ms) expired");
}, 200);

setTimeout(function() {
    print("Timer 3 (1000ms) expired. Exiting.");
}, 1000);

print("Main script finished, waiting for timers...");
