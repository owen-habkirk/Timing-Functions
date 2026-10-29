# Timer & Stopwatch

A simple C++ utility for timing and measuring elapsed time.

## Timer

- `start()` — Starts the timer.
- `restart()` — Restarts the timer.
- `reset()` — Resets the timer without changing its duration.
- `changeDuration(time)` — Changes the timer's duration.
- `isDone()` — Checks if the timer has finished.
- `waitUntilDone()` — Waits until the timer finishes.

## Stopwatch

- `start()` — Starts the stopwatch.
- `pause()` — Pauses the stopwatch.
- `resume()` — Resumes the stopwatch.
- `getTime<Unit>()` — Gets the elapsed time in the specified unit.
- `startAfter(delay)` — Starts the stopwatch after a delay, resetting its elapsed time.
- `resumeAfter(delay)` — Resumes the stopwatch after a delay, preserving its elapsed time.

## Restrictions

- Uses `std::chrono` duration types for time values.
- Negative durations throw `std::invalid_argument`.
- Timing is not guaranteed to be exact due to OS scheduling and hardware limitations.
- Waiting uses a combination of sleeping and busy-spinning for improved accuracy.
- The timer is not thread-safe.
- The stopwatch uses mutexes for synchronization but should not be destroyed while other threads are accessing it.
- The stopwatch cannot be copied or moved.
- Requires C++11 or newer.
