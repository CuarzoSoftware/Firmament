#ifndef WORKER_H
#define WORKER_H

#include <Core/CZEventSource.h>
#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

struct sd_bus;

using namespace CZ;

/**
 * @brief Runs blocking work (D-Bus calls, child processes) off the UI thread.
 *
 * A single background thread runs the jobs in order, each with the thread's own system bus
 * connection (sd-bus connections can't be shared between threads). A job returns what to do next
 * on the main thread (e.g. apply the values it read), run from the event loop.
 *
 * Jobs must not touch the owner's state (copy what they need); their completions may. Destroying
 * the worker waits for the running job and drops the pending ones and their completions.
 */
class Worker final
{
public:
    /// The work (worker thread, @p bus may be nullptr), returning its completion (main thread, may be empty).
    using Job = std::function<std::function<void()>(sd_bus *bus)>;

    Worker() noexcept;
    ~Worker() noexcept;

    void run(Job job) noexcept;

private:
    void loop() noexcept;
    void dispatch() noexcept;

    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::deque<Job> m_jobs;
    std::vector<std::function<void()>> m_done;
    bool m_quit { false };
    std::shared_ptr<CZEventSource> m_wake; // eventfd: completions ready
    std::thread m_thread;
};

/**
 * Runs a command detached (its own session, reaped in the background), optionally with extra
 * environment variables ("NAME=value"). Doesn't wait for it.
 */
bool SpawnDetached(std::initializer_list<const char*> args, std::initializer_list<const char*> env = {}) noexcept;

#endif // WORKER_H
