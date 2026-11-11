#include <Core/Worker.h>
#include <Core/Log.h>
#include <systemd/sd-bus.h>
#include <sys/eventfd.h>
#include <sys/wait.h>
#include <spawn.h>
#include <unistd.h>
#include <cstring>
#include <string>
#include <vector>

extern char **environ;

Worker::Worker() noexcept
{
    const int fd { eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK) };

    if (fd < 0)
    {
        Log(CZError, CZLN, "eventfd failed: {}", strerror(errno));
        return;
    }

    m_wake = CZEventSource::Make(fd, EPOLLIN, CZOwn::Own, [this](int fd, UInt32, CZEventSource*) {
        eventfd_t v;
        eventfd_read(fd, &v);
        dispatch();
    });

    m_thread = std::thread([this]{ loop(); });
}

Worker::~Worker() noexcept
{
    {
        std::lock_guard lock { m_mutex };
        m_quit = true;
        m_jobs.clear();
    }

    m_cv.notify_one();

    if (m_thread.joinable())
        m_thread.join();
}

void Worker::run(Job job) noexcept
{
    if (!m_wake)
    {
        // No eventfd: run it here.
        if (auto done { job(nullptr) })
            done();
        return;
    }

    {
        std::lock_guard lock { m_mutex };
        m_jobs.emplace_back(std::move(job));
    }

    m_cv.notify_one();
}

void Worker::loop() noexcept
{
    sd_bus *bus {};

    if (sd_bus_open_system(&bus) < 0)
        bus = nullptr;
    else
        sd_bus_set_method_call_timeout(bus, 5 * 1000 * 1000); // Never wait long for a busy service

    while (true)
    {
        Job job;

        {
            std::unique_lock lock { m_mutex };
            m_cv.wait(lock, [this]{ return m_quit || !m_jobs.empty(); });

            if (m_quit)
                break;

            job = std::move(m_jobs.front());
            m_jobs.pop_front();
        }

        auto done { job(bus) };

        if (!done)
            continue;

        {
            std::lock_guard lock { m_mutex };

            if (m_quit)
                break;

            m_done.emplace_back(std::move(done));
        }

        eventfd_write(m_wake->fd(), 1);
    }

    sd_bus_flush_close_unref(bus);
}

void Worker::dispatch() noexcept
{
    std::vector<std::function<void()>> done;

    {
        std::lock_guard lock { m_mutex };
        done.swap(m_done);
    }

    for (auto &f : done)
        f();
}

bool SpawnDetached(std::initializer_list<const char*> args, std::initializer_list<const char*> env) noexcept
{
    std::vector<char*> argv;

    for (const char *a : args)
        argv.push_back(const_cast<char*>(a));

    argv.push_back(nullptr);

    // The environment plus the extra variables (replacing the same names).
    std::vector<char*> envp;

    for (char **e = environ; *e; e++)
    {
        bool replaced { false };

        for (const char *x : env)
        {
            const char *eq { strchr(x, '=') };

            if (eq && strncmp(*e, x, size_t(eq - x + 1)) == 0)
                replaced = true;
        }

        if (!replaced)
            envp.push_back(*e);
    }

    for (const char *x : env)
        envp.push_back(const_cast<char*>(x));

    envp.push_back(nullptr);

    // posix_spawn (vfork-like): no copy of Firmament's address space, unlike fork().
    posix_spawnattr_t attr;
    posix_spawnattr_init(&attr);
    posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETSID | POSIX_SPAWN_SETSIGMASK);
    sigset_t none;
    sigemptyset(&none);
    posix_spawnattr_setsigmask(&attr, &none);

    pid_t pid;
    const int ret { posix_spawnp(&pid, argv[0], nullptr, &attr, argv.data(), envp.data()) };
    posix_spawnattr_destroy(&attr);

    if (ret != 0)
    {
        Log(CZWarning, CZLN, "Failed to run {}: {}", argv[0], strerror(ret));
        return false;
    }

    // Reap it whenever it exits (it may be a long running app).
    std::thread([pid]{ waitpid(pid, nullptr, 0); }).detach();
    return true;
}
