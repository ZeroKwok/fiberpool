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

void fiber_pool::pool::abstract_runnable::finish()
{
    boost::this_fiber::properties<
        fiber_pool::fiber_properties>().finish();
}

// pool::abstract_runnable
boost::atomic_size_t fiber_pool::pool::abstract_runnable::count_{ 0 };

//////////////////////////////////////////////////////////////////////////

namespace fiber_pool {

fiber::fiber()
{}

fiber::fiber(fiber& right)
    : m_base(std::move(right.m_base))
{}

fiber::fiber(fiber && right)
    : m_base(std::move(right.m_base))
{}

fiber::fiber(base_type&& fiber)
    : m_base(std::move(fiber))
{}

fiber::~fiber()
{
    if (m_base.joinable())
        m_base.detach();
}

fiber& fiber::operator=(fiber& right)
{
    m_base = std::move(right.m_base);
    return *this;
}

fiber& fiber::operator=(fiber&& right)
{
    m_base = std::move(right.m_base);
    return *this;
}

fiber::id fiber::get_id() const noexcept
{
    return m_base.get_id();
}

bool fiber::finshed() const noexcept
{
    return const_cast<fiber*>(this)->
        m_base.properties<fiber_properties>().finished();
}

bool fiber::joinable() const noexcept
{
    return m_base.joinable();
}

void fiber::join()
{
    return m_base.join();
}

void fiber::interrupt()
{
    m_base.properties<fiber_properties>().interrupt();
}

//////////////////////////////////////////////////////////////////////////

pool::pool(size_t threads /*= -1*/)
{
    // 默认使用逻辑处理器的2倍
    if (threads == -1)
        threads = std::max(boost::thread::hardware_concurrency(), 2u) * 2u;

    // 启动工作线程
    for (size_t i = 0; i < threads; ++i)
    {
        m_threads.emplace_back([this]()
        {
            // 初始化调度算法
            boost::fibers::use_scheduling_algorithm<
                shared_work_with_properties>(true);

            // 将线程挂起, 内部会将执行绪交给调度器
            boost::unique_lock<boost::mutex> lock(m_mutex_stop);
            m_condition_stop.wait(lock, [this]() {
                return m_pool_state.load() > running;
            });
        });
    }

    // 视实例化自己的为主线程
    shared_work_global_config_single::get_mutable_instance()
        .set_main_thread(boost::this_thread::get_id());

    // 表示池的状态
    m_pool_state.store(running);
}

pool::~pool()
{
    if (state() != stoped)
    {
#if BOOST_OS_WINDOWS
        ::OutputDebugStringA("*** Warnings ***\r\n");
        ::OutputDebugStringA("shutdown() must be called before ~pool::().\r\n");
#endif
        std::terminate();
    }
}

pool::state_t pool::state() const noexcept
{
    return static_cast<state_t>(m_pool_state.load());
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
    return abstract_runnable::count_.load();
}

void pool::shutdown(bool wait/* = false*/) noexcept
{
    // 唤醒退出工作线程
    {
        boost::unique_lock<boost::mutex> lock(m_mutex_stop);
        m_pool_state.store(wait ? waiting : cleaning);
        m_condition_stop.notify_all();
    }

    for (auto& thread : m_threads)
    {
        if (thread.joinable())
        {
            while (!thread.try_join_for(boost::chrono::milliseconds(100)))
            {
                if (fiber_count() == 0)
                {
                    boost::unique_lock<boost::mutex> lock(m_mutex_stop);
                    m_pool_state.store(cleaning);
                    m_condition_stop.notify_all();
                }
            }
        }
    }

    m_pool_state.store(stoped);
}

fiber_pool::pool& get_fiber_pool(size_t threads/* = -1*/)
{
    static fiber_pool::pool _pool{ threads };
    return _pool;
}

} // fiber_pool
