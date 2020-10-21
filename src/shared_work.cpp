#include "shared_work.hpp"

/*!
 * \file   shared_work.cpp 
 *
 * \author zero kwok
 * \date   2020-10
 *
 */

namespace fiber_pool {

    shared_work_with_properties::shared_work_with_properties(bool suspend)
        : suspend_{ suspend }
    {
    }

    void shared_work_with_properties::awakened(
        boost::fibers::context* ctx, fiber_properties& props) noexcept
    {
        if (ctx->is_context(boost::fibers::type::pinned_context))
        { /*<
                recognize when we're passed this thread's main fiber (or an
                implicit library helper fiber): never put those on the shared
                queue
            >*/
            lqueue_.push_back(*ctx);
        }
        else
        {
            ctx->detach();
            std::unique_lock< std::mutex > lk{ rqueue_mtx_ }; /*<
                    worker fiber, enqueue on shared queue
                >*/
            rqueue_.push_back(ctx);
        }
    }

    void shared_work_with_properties::property_change(
        boost::fibers::context* ctx, fiber_properties& props) noexcept
    {
        // 'ctx' might not be in our queue at all, if caller is changing the
        // priority of (say) the running fiber. If it's not there, no need to
        // move it: we'll handle it next time it hits awakened().
        if (!ctx->ready_is_linked())
        { /*<
            Your `property_change()` override must be able to
            handle the case in which the passed `ctx` is not in
            your ready queue. It might be running, or it might be
            blocked. >*/

            return;
        }

        // Found ctx: unlink it
        ctx->ready_unlink();

        // Here we know that ctx was in our ready queue, but we've unlinked
        // it. We happen to have a method that will (re-)add a context* to the
        // right place in the ready queue.
        awakened(ctx, props);
    }

    boost::fibers::context* shared_work_with_properties::pick_next() noexcept
    {
        boost::fibers::context* ctx = nullptr;
        std::unique_lock< std::mutex > lk{ rqueue_mtx_ };
        if (!rqueue_.empty()) { /*<
                pop an item from the ready queue
            >*/
            ctx = rqueue_.front();
            rqueue_.pop_front();
            lk.unlock();
            BOOST_ASSERT(nullptr != ctx);
            boost::fibers::context::active()->attach(ctx); /*<
                attach context to current scheduler via the active fiber
                of this thread
            >*/
        }
        else {
            lk.unlock();
            if (!lqueue_.empty()) { /*<
                    nothing in the ready queue, return main or dispatcher fiber
                >*/
                ctx = &lqueue_.front();
                lqueue_.pop_front();
            }
        }
        return ctx;
    }

    bool shared_work_with_properties::has_ready_fibers() const noexcept
    {
        std::unique_lock< std::mutex > lock{ rqueue_mtx_ };
        return !rqueue_.empty() || !lqueue_.empty();
    }

    void shared_work_with_properties::suspend_until(
        std::chrono::steady_clock::time_point const& time_point) noexcept
    {
        if (suspend_) {
            if ((std::chrono::steady_clock::time_point::max)() == time_point) {
                std::unique_lock< std::mutex > lk{ mtx_ };
                cnd_.wait(lk, [this]() { return flag_; });
                flag_ = false;
            }
            else {
                std::unique_lock< std::mutex > lk{ mtx_ };
                cnd_.wait_until(lk, time_point, [this]() { return flag_; });
                flag_ = false;
            }
        }
    }

    void shared_work_with_properties::notify() noexcept
    {
        if (suspend_) {
            std::unique_lock< std::mutex > lk{ mtx_ };
            flag_ = true;
            lk.unlock();
            cnd_.notify_all();
        }
    }

    // 静态成员对象对象实例化
    shared_work_with_properties::rqueue_type shared_work_with_properties::rqueue_{};
    std::mutex shared_work_with_properties::rqueue_mtx_{};

} // fiber_pool