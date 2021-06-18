#include "fiber_pool.hpp"
#include "shared_work.hpp"

/*!
 *  @file   fiber_pool.cpp
 *
 *  @author zero kwok
 *  @date   2020-10
 *
 */

bool boost::this_fiber::interrupted()
{
    if (get_fiber_pool().state() > fiber_pool::pool::waiting)
        return true;

    return boost::this_fiber::properties<
        fiber_pool::fiber_properties>().interrupted();
}

FIBER_POOL_DECL void boost::this_fiber::bind_thread()
{
    boost::this_fiber::properties<
        fiber_pool::fiber_properties>().bind();

    if (fiber_pool::shared_work_global_config_single::get_const_instance().is_main_thread())
        throw std::runtime_error("The fibers cannot be bind to the main thread");
}

//////////////////////////////////////////////////////////////////////////

// pool::abstract_runnable
static boost::atomic_size_t __abstract_runnable_count{ 0 };

size_t fiber_pool::pool::abstract_runnable::count()
{
    return __abstract_runnable_count;
}

void fiber_pool::pool::abstract_runnable::increment()
{
    ++__abstract_runnable_count;
}

void fiber_pool::pool::abstract_runnable::decrement()
{
    --__abstract_runnable_count;
}

void fiber_pool::pool::abstract_runnable::finish()
{
    boost::this_fiber::properties<
        fiber_pool::fiber_properties>().finish();
}

//////////////////////////////////////////////////////////////////////////

namespace fiber_pool {

struct fiber_private
{
    bool interrupt_destruct_{false};
    boost::fibers::fiber fiber_;

    fiber_private() {}

    ~fiber_private()
    {
        if (interrupt_destruct_)
            fiber_.properties<fiber_properties>().interrupt();

        fiber_.detach();
    }
};

fiber::fiber()
{}

fiber::fiber(const fiber& right)
    : m_private(right.m_private)
{}

fiber::fiber(boost::fibers::fiber&& fiber)
    : m_private(std::make_shared<fiber_private>())
{
    m_private->fiber_.swap(fiber);
}

fiber& fiber::operator=(const fiber& right)
{
    m_private = right.m_private;
    return *this;
}

fiber::id fiber::get_id() const noexcept
{
    if (m_private)
        return m_private->fiber_.get_id();

    return fiber::id();
}

bool fiber::finshed() const noexcept
{
    if (m_private)
    {
        return const_cast<fiber*>(this)->
            m_private->fiber_.properties<fiber_properties>().finished();
    }

    return true;
}

bool fiber::joinable() const noexcept
{
    if (m_private)
        return m_private->fiber_.joinable();

    return false;
}

void fiber::join()
{
    if (m_private)
        m_private->fiber_.join();
}

void fiber::interrupt()
{
    if (m_private)
        m_private->fiber_.properties<fiber_properties>().interrupt();
}

void fiber::interrupt_on_destruct()
{
    if (m_private)
        m_private->interrupt_destruct_ = true;
}

//////////////////////////////////////////////////////////////////////////

struct pool_private
{
    boost::atomic_int                     pool_state{ pool::stoped };
    boost::mutex                          mutex_stop;
    boost::fibers::condition_variable_any condition_stop;
    std::vector<boost::thread>            threads;
};

//////////////////////////////////////////////////////////////////////////

pool::pool(size_t threads /*= -1*/)
{
    FIBER_POOL_INIT_PRIVATE(pool);

    // 视实例化自己的为主线程
    shared_work_global_config_single::get_mutable_instance()
        .set_main_thread(boost::this_thread::get_id());

    // 表示池的状态
    FIBER_POOL_PRIVATE(pool).pool_state.store(running);

    // 默认使用逻辑处理器的2倍
    if (threads == -1)
        threads = std::max(boost::thread::hardware_concurrency(), 2u) * 2u;

    // 启动工作线程
    for (size_t i = 0; i < threads; ++i)
    {
        FIBER_POOL_PRIVATE(pool).threads.emplace_back([this]()
        {
            // 初始化调度算法
            boost::fibers::use_scheduling_algorithm<
                shared_work_with_properties>(true);

            // 将线程挂起, 内部会将执行绪交给调度器
            boost::unique_lock<boost::mutex> lock(FIBER_POOL_PRIVATE(pool).mutex_stop);
            FIBER_POOL_PRIVATE(pool).condition_stop.wait(lock, [this]() {
                return FIBER_POOL_PRIVATE(pool).pool_state.load() > running;
            });

#if BOOST_OS_WINDOWS
            ::OutputDebugStringA("The worker thread exit!\r\n");
#endif
        });
    }
}

pool::~pool()
{
    if (state() != stoped)
    {
#if BOOST_OS_WINDOWS
        ::OutputDebugStringA("*** Warnings ***\r\n");
        ::OutputDebugStringA("The pool::shutdown() was not called before ~pool::pool() to clean up the resource.\r\n");
#endif
        shutdown(false);
    }

    FIBER_POOL_FREE_PRIVATE(pool);
}

pool::state_t pool::state() const noexcept
{
    return static_cast<state_t>(FIBER_POOL_PRIVATE(pool).pool_state.load());
}

fiber pool::dispatch(pool::runnable_ptr&& runnable)
{
    // 确保当前线程已经初始化调度算法
    thread_local static boost::atomic_bool has_not_init_alorithm{ true };

    if (has_not_init_alorithm.load())
    {
        has_not_init_alorithm.store(false);

        boost::fibers::use_scheduling_algorithm<shared_work_with_properties>(true);
    }

    // 启动
    return fiber{ boost::fibers::fiber(
        std::bind(&abstract_runnable::operator(), std::move(runnable))) };
}

size_t fiber_pool::pool::fiber_count() const noexcept
{
    return abstract_runnable::count();
}

void pool::shutdown(bool wait/* = false*/) noexcept
{
    // 唤醒退出工作线程
    {
        boost::unique_lock<boost::mutex> lock(FIBER_POOL_PRIVATE(pool).mutex_stop);
        FIBER_POOL_PRIVATE(pool).pool_state.store(wait ? waiting : cleaning);

        // 这里需要判断一下, 因为active是静态对象, pool也是静态对象, 故当active先析构时将会出现问题.
        // GuoJH by 2021-5-21 17:00:09 

        if (boost::fibers::context::active() != nullptr)
            FIBER_POOL_PRIVATE(pool).condition_stop.notify_all();
    }

    for (auto& thread : FIBER_POOL_PRIVATE(pool).threads)
    {
        if (thread.joinable())
        {
            while (!thread.try_join_for(boost::chrono::milliseconds(100)))
            {
                if (fiber_count() == 0)
                {
                    boost::unique_lock<boost::mutex> lock(FIBER_POOL_PRIVATE(pool).mutex_stop);
                    FIBER_POOL_PRIVATE(pool).pool_state.store(cleaning);

                    if (boost::fibers::context::active() != nullptr)
                        FIBER_POOL_PRIVATE(pool).condition_stop.notify_all();
                }
            }
        }
    }

    FIBER_POOL_PRIVATE(pool).pool_state.store(stoped);
}

fiber_pool::pool& get_fiber_pool(size_t threads/* = -1*/)
{
    static fiber_pool::pool _pool{ threads };
    return _pool;
}

} // fiber_pool
