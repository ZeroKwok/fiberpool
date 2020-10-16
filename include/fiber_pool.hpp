#ifndef fiber_pool_h__
#define fiber_pool_h__

/*!
 * \file   fiber_pool.hpp 
 *
 * \author zero kwok
 * \date   2020-10
 *
 */

#include "_config.hpp"
#include <boost/atomic.hpp>
#include <boost/thread.hpp>
#include <boost/fiber/fiber.hpp>
#include <boost/fiber/future.hpp>
#include <boost/fiber/condition_variable.hpp>

// 扩展boost::this_fiber
namespace boost {
    namespace this_fiber {
        FIBER_POOL_DECL bool interrupted();
    }
}

namespace fiber_pool {

// 扩展boost::fibers::fiber
class FIBER_POOL_DECL fiber
{
public:
    typedef boost::fibers::fiber base_type;
    typedef base_type::id id;

    fiber(base_type&& fiber);
    fiber(fiber&& other);
    ~fiber();

    fiber() = delete;
    fiber & operator=(fiber const&) = delete;

    id   get_id() const noexcept;
    bool joinable() const noexcept;
    void join();
    void interrupt();

protected:
    base_type m_base;
};

/*!
 *   纤程池对象
 *   内部维护工作线程
 */
class FIBER_POOL_DECL pool
{
    boost::atomic_int                            m_pool_state{ stoped };
    boost::mutex                                 m_mutex_stop;
    boost::fibers::condition_variable_any        m_condition_stop;
    std::vector<boost::thread>                   m_threads;
public:
    pool(size_t threads = -1);
    ~pool();

    enum state_t
    {
        running,    ///< 运行阶段, 此时可以投递任务到池中
        cleaning,   ///< 清理阶段, 此时池正在清理所有未决的任务, 所有post将抛出异常. 
        stoped,
    };

    /// 返回池的状态
    state_t state() const;

    /// 可运行对象的抽象
    struct abstract_runnable
    {
        static boost::atomic_size_t count_;

        virtual ~abstract_runnable() {}
        virtual void operator()() = 0;
    };

    typedef std::unique_ptr<abstract_runnable> runnable_ptr;

    /// 可运行对象的封装, 联合参数一起构成闭包
    template< typename Fn, typename ... Arg >
    class closure : public abstract_runnable
    {
        bool                             init_{ false };
        typename std::decay< Fn >::type  fn_;
        std::tuple< Arg ... >            arg_;
    public:
        closure() = delete;
        closure(closure const&) = delete;
        closure& operator=(closure const&) = delete;
        closure& operator=(closure &&) = delete;

        closure(Fn && fn, Arg ... arg)
            : fn_(std::forward< Fn >(fn))
            , arg_(std::forward< Arg >(arg) ...)
            , init_(true)
        {
            ++count_;
        }

        closure(closure&& other)
        {
            std::swap(fn_, other.fn_);
            std::swap(arg_, other.arg_);
            std::swap(init_, other.init_);
        }

        ~closure()
        {
            if (init_)
                --count_;
        }

        void operator()()
        {
            if (!boost::this_fiber::interrupted())
            {
                try
                {
                    auto fn = std::move(fn_);
                    auto arg = std::move(arg_);

#if defined(BOOST_NO_CXX17_STD_APPLY)
                    boost::context::detail::apply(std::move(fn), std::move(arg));
#else
                    std::apply(std::move(fn), std::move(arg));
#endif
                }
                catch (...)
                {
#if BOOST_OS_WINDOWS
                    ::OutputDebugStringA("*** Warnings ***\r\n"); 
                    ::OutputDebugStringA("An unhandled exception occurred during fiber_pool running.\r\n");
#endif
                }
            }
        }
    };

    /*!
     *  提交一个可调用对象作为纤程到工作线程执行.
     *  可调用对象抛出的任何异常将被丢弃.
     */
    template<typename Fn, typename ... Arg>
    fiber post(Fn&& fn, Arg&& ... arg)
    {
        if (state() != running)
            throw std::runtime_error("The task cannot be delivered at this time.");

        return std::move(dispatch(
            runnable_ptr(new closure<Fn, Arg ...>{ 
                std::forward< Fn >(fn), std::forward< Arg >(arg) ... })));
    }

    /// 返回池中所有未决的纤程数.
    size_t fiber_count() const;

    /*!
     *   等待所有未决的任务执行完毕后关闭纤程池.
     *   在等待过程中池的状态将设置为cleaning, 即不允许通过post()投递任务;
     *   函数返回后池的状态将被设置为stoped.
     */
    void shutdown();

protected:
    fiber dispatch(pool::runnable_ptr&& runnable);
};

} // fiber_pool

/// 返回fiber_pool的唯一实例
FIBER_POOL_DECL fiber_pool::pool& get_fiber_pool(size_t threads = -1);

/*! 
 *   投递任务到池中执行, 返回future. 类似于async
 */
template< typename Fn, typename ... Args >
boost::fibers::future<
    typename std::result_of<
    typename std::decay< Fn >::type(typename std::decay< Args >::type ...)
    >::type
>
post_fiber(Fn && fn, Args ... args)
{
    typedef typename std::result_of<
        typename std::decay< Fn >::type(typename std::decay< Args >::type ...)
    >::type     result_type;

    boost::fibers::packaged_task< result_type(typename std::decay< Args >::type ...) > pt{
        std::forward< Fn >(fn) };
    boost::fibers::future< result_type > f{ pt.get_future() };

    get_fiber_pool().post(std::move(pt), std::forward< Args >(args) ...);

    return f;
}

#endif // fiber_pool_h__
