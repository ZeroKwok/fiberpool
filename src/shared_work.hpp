#ifndef shared_work_h__
#define shared_work_h__

/*!
 * \file   shared_work.hpp
 *
 * \author zero kwok
 * \date   2020-10
 *
 */

#include <chrono>
#include <deque>
#include <mutex>
#include <condition_variable>

#include <boost/config.hpp>
#include <boost/atomic.hpp>
#include <boost/fiber/algo/algorithm.hpp>
#include <boost/fiber/context.hpp>
#include <boost/fiber/detail/config.hpp>
#include <boost/fiber/scheduler.hpp>

namespace fiber_pool {

class fiber_properties : public boost::fibers::fiber_properties
{
    int priority_;
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
};

class shared_work_with_properties :
    public boost::fibers::algo::algorithm_with_properties<fiber_properties>
{
    typedef std::deque<boost::fibers::context*> rqueue_type;
    typedef boost::fibers::scheduler::ready_queue_type lqueue_type;

    static rqueue_type     	rqueue_;
    static std::mutex   	rqueue_mtx_;

    lqueue_type            	lqueue_{};
    std::mutex              mtx_{};
    std::condition_variable cnd_{};
    bool                    flag_{ false };
    bool                    suspend_{ false };

public:
    shared_work_with_properties() = default;

    shared_work_with_properties(bool suspend);

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
