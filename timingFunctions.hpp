#include <chrono>
#include <thread>
#include <stdexcept>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <cstdint>

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
    #include <immintrin.h>
    #define CPU_PAUSE() _mm_pause()

#elif defined(__aarch64__) || defined(_M_ARM64)
    #if defined(_MSC_VER)
        #include <intrin.h>
        #define CPU_PAUSE() __yield()
    #elif defined(__GNUC__) || defined(__clang__)
        #define CPU_PAUSE() __asm__ __volatile__("yield")
    #else
        #define CPU_PAUSE() std::this_thread::yield()
    #endif
#else
    #define CPU_PAUSE() std::this_thread::yield()
#endif



template<typename unit>
class timer{
private:
    using clock = std::chrono::steady_clock;
    
    unit totalTime;
    bool started = false;
    clock::time_point startTime;
public:
    explicit timer(unit time) : totalTime(time){
        if(time < unit::zero()){
            //reject negative durations
            throw std::invalid_argument("Duration Cannot Be Negative");
        }
    }
    
    void changeDuration(unit newTime){
        //does not reset timer and checks for negatives
        //isDone will compare against this new time. If startTime - clock::now() > newTime it will imediatly return true
        if(newTime < unit::zero()){
            throw std::invalid_argument("Duration Cannot Be Negative");
        }
        totalTime = newTime;
    }
    
    
    
    void start(){
        if(!started){
            startTime = clock::now();
            started = true;
        }
    }
    
    void restart(){
        startTime = clock::now();
        started = true;
    }
    
    void reset(){
        //resets the timer but does not change the total time
        //call changeDuration to reset the deadline
        started = false;
    }
    
    bool isDone() const {
        return started && (clock::now() - startTime >= totalTime);
    }
    
    void waitUntilDone(){
        //Uses a hybrid sleep/spin setup
        //Sleeps for the majority of the time to save cpu usage
        //200µs before the end it wakes up so it can return with a higher accuracy to the target time
        //This will increase accuracy but it cannot guarantee it to be exact as the thread may still wake up too late
        //Do not call changeDuration() concurrently with this or it may spin longer than neccessary
        start();
        auto end = startTime + totalTime;
        //200µs is not universally optomized. Change this based on your os and hardware 
        constexpr auto spinWindow = std::chrono::microseconds(200);
        if(totalTime > spinWindow){
            std::this_thread::sleep_until(end - spinWindow);
        }
        while(clock::now() < end){
            CPU_PAUSE();
        }
    }
};

class stopwatch{
private:
    using clock = std::chrono::steady_clock;
    using ns = std::chrono::nanoseconds;

    ns duration{0};
    clock::time_point startTime{};
    bool running = false;

    mutable std::mutex mutex;
    std::condition_variable wakeCondition;

    std::mutex workerMutex;
    std::thread worker;

    std::atomic<std::uint64_t> generation{0};

    void cancelScheduledOperation(){
        generation.fetch_add(1, std::memory_order_acq_rel);
        wakeCondition.notify_all();
    }

    template<typename Unit>
    void scheduleAfter(Unit delay, bool resetTime){
        if(delay < Unit::zero()){
            throw std::invalid_argument("Duration cannot be negative");
        }

        std::lock_guard<std::mutex> workerLock(workerMutex);

        {
            std::lock_guard<std::mutex> lock(mutex);

            if(running){
                return;
            }

            cancelScheduledOperation();
        }

        if(worker.joinable()){
            worker.join();
        }

        const auto token = generation.load(std::memory_order_acquire);

        worker = std::thread([this, delay, resetTime, token](){
            const auto end = clock::now() + delay;

            constexpr auto spinWindow = std::chrono::microseconds(200);

            if(end - clock::now() > spinWindow){
                std::unique_lock<std::mutex> lock(mutex);

                wakeCondition.wait_until(lock, end - spinWindow, [this, token](){
                    return generation.load(std::memory_order_acquire) != token;
                });
            }

            while(clock::now() < end){
                if(generation.load(std::memory_order_acquire) != token){
                    return;
                }

                CPU_PAUSE();
            }

            std::lock_guard<std::mutex> lock(mutex);

            if(generation.load(std::memory_order_acquire) != token || running){
                return;
            }

            if(resetTime){
                duration = ns{0};
            }

            startTime = clock::now();
            running = true;
        });
    }

public:
    stopwatch() = default;

    ~stopwatch(){
        std::lock_guard<std::mutex> workerLock(workerMutex);

        cancelScheduledOperation();

        if(worker.joinable()){
            worker.join();
        }
    }

    stopwatch(const stopwatch&) = delete;
    stopwatch& operator=(const stopwatch&) = delete;
    stopwatch(stopwatch&&) = delete;
    stopwatch& operator=(stopwatch&&) = delete;

    void start(){
        std::lock_guard<std::mutex> lock(mutex);

        cancelScheduledOperation();

        if(running){
            return;
        }

        startTime = clock::now();
        running = true;
    }

    template<typename Unit>
    Unit getTime() const {
        std::lock_guard<std::mutex> lock(mutex);

        auto total = duration;

        if(running){
            total += std::chrono::duration_cast<ns>(clock::now() - startTime);
        }

        return std::chrono::duration_cast<Unit>(total);
    }

    void pause(){
        std::lock_guard<std::mutex> lock(mutex);

        cancelScheduledOperation();

        if(running){
            duration += std::chrono::duration_cast<ns>(clock::now() - startTime);

            running = false;
        }
    }

    void resume(){
        start();
    }

    template<typename Unit>
    void resumeAfter(Unit delay){
        scheduleAfter(delay, false);
    }

    template<typename Unit>
    void startAfter(Unit delay){
        scheduleAfter(delay, true);
    }
};
