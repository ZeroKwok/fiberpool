#ifndef shared_work_h__
#define shared_work_h__

/*!
 *  @file   shared_work.hpp
 *
 *  @author zero kwok
 *  @date   2020-10
 *
 */

#include <set>
#include <deque>
#include <mutex>
#include <chrono>
#include <condition_variable>

#include <boost/config.hpp>
#include <boost/atomic.hpp>
#include <boost/thread.hpp>
#include <boost/noncopyable.hpp>
#include <boost/serialization/singleton.hpp>
#include <boost/fiber/algo/algorithm.hpp>
#include <boost/fiber/context.hpp>
#include <boost/fiber/detail/config.hpp>
#include <boost/fiber/scheduler.hpp>

namespace fiber_pool {

class fiber_properties : public boost::fibers::fiber_properties
{
    int priority_;
    boost::atomic_bool binding_{ false };
    boost::atomic_bool finished_{ false };
    boost::atomic_bool interrupted_{ false };
public:
    fiber_properties(boost::fibers::context* ctx) 
        : boost::fibers::fiber_properties(ctx)
        , priority_(0)
    {
    }

    boost::fibers::context* context() {
        return ctx_;
    }

    bool interrupted() const {
        return interrupted_.load();
    }

    void interrupt() {
        interrupted_.store(true);
    }

    bool finished() const {
        return finished_.load();
    }

    void finish() {
        finished_.store(true);
    }

    void bind() {
        binding_.store(true);
    }

    bool binding() const {
        return binding_.load();
    }
};

class shared_work_global_config : boost::noncopyable
{
    boost::thread::id main_thread_id_;

    std::mutex mutex_;
    std::set<class shared_work_with_properties* > algos_;
public:

    ~shared_work_global_config()
    {
    }

    void set_main_thread(boost::thread::id id) {
        main_thread_id_ = id;
    }

    bool is_main_thread() const 
    {
        // 主线程id必须有效
        BOOST_ASSERT(main_thread_id_ != boost::thread::id());
        return boost::this_thread::get_id() == main_thread_id_;
    }

    void add_instance(class shared_work_with_properties* algo) 
    {
        std::unique_lock< std::mutex > lk{ mutex_ };
        algos_.insert(algo);
    }

    void remove_instance(class shared_work_with_properties* algo)
    {
        std::unique_lock< std::mutex > lk{ mutex_ };
        auto it = algos_.find(algo);
        if (it != algos_.end())
            algos_.erase(it);
    }

    void notify_one();
    void notify_all();
};

typedef boost::serialization::singleton<shared_work_global_config> shared_work_global_config_single;

class shared_work_with_properties :
    public boost::fibers::algo::algorithm_with_properties<fiber_properties>
{
    typedef std::deque<boost::fibers::context*> rqueue_type;
    typedef boost::fibers::scheduler::ready_queue_type lqueue_type;

    static rqueue_type      rqueue_;    // 共享队列
    static std::mutex       rqueue_mtx_;

    lqueue_type             pqueue_{};  // 优先队列, 绑定线程的纤程
    lqueue_type             lqueue_{};  // 本地队列, main context, dispatcher context
    std::mutex              mtx_{};
    std::condition_variable cnd_{};
    bool                    flag_{ false };
    bool                    suspend_{ false };

    // 全局配置
    shared_work_global_config& global_config_{shared_work_global_config_single::get_mutable_instance()};

public:
    shared_work_with_properties(bool suspend = true);
    ~shared_work_with_properties();

    shared_work_with_properties(shared_work_with_properties const&) = delete;
    shared_work_with_properties(shared_work_with_properties&&) = delete;

    shared_work_with_properties& operator=(shared_work_with_properties const&) = delete;
    shared_work_with_properties& operator=(shared_work_with_properties&&) = delete;

    void awakened(boost::fibers::context* ctx, fiber_properties& props) noexcept override;

    void property_change(boost::fibers::context* ctx, fiber_properties& props) noexcept override;

    boost::fibers::context* pick_next() noexcept override;

    bool has_ready_fibers() const noexcept override;

    void suspend_until(std::chrono::steady_clock::time_point const&) noexcept override;

    void notify() noexcept override;
};

} // fiber_pool

#endif // shared_work_h__
