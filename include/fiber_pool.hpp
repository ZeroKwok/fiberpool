#ifndef fiber_pool_h__
#define fiber_pool_h__

/*!
 *  @file   fiber_pool.hpp
 * 
 *  @author zero kwok
 *  @date   2020-10
 */

#if defined(FIBERPOOL_BUILD_SHARED_LIB)                 //!< build shared lib
#   define FIBER_POOL_DECL __declspec(dllexport)
#elif defined(FIBERPOOL_USING_SHARED_LIB)               //!< using shared lib
#   define FIBER_POOL_DECL __declspec(dllimport)
#else
#   define FIBER_POOL_DECL                              //!< using or build static lib
#endif

#if defined(WIN32) || defined(_WIN32)
#   ifndef WIN32_LEAN_AND_MEAN
#      define  WIN32_LEAN_AND_MEAN 
#   endif
#   ifndef  NOMINMAX
#      define  NOMINMAX
#   endif
#endif

#include <boost/atomic.hpp>
#include <boost/thread.hpp>
#include <boost/fiber/fiber.hpp>
#include <boost/fiber/future.hpp>
#include <boost/fiber/condition_variable.hpp>

// 扩展boost::this_fiber
namespace boost {
    namespace this_fiber {
        //! 返回当前纤程是否被中断
        FIBER_POOL_DECL bool interrupted();
    }
}

namespace fiber_pool {

/*!
 *  扩展boost::fibers::fiber, 使之可以安全的终止未决的纤程, 
 *  但同时使之丧失运行纤程的能力.
 */
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
 *  纤程池对象, 内部维护多个工作线程使之共享执行所有投递到池中的纤程.
 */
class FIBER_POOL_DECL pool
{
    boost::atomic_int                     m_pool_state{ stoped };
    boost::mutex                          m_mutex_stop;
    boost::fibers::condition_variable_any m_condition_stop;
    std::vector<boost::thread>            m_threads;

    //! 可运行对象的抽象
    struct abstract_runnable
    {
        static boost::atomic_size_t count_; //!< 对象计数器

        virtual ~abstract_runnable() {}
        virtual void operator()() = 0;
    };

    typedef std::unique_ptr<abstract_runnable> runnable_ptr;

    //! 可运行对象的封装, 联合参数一起构成闭包, 可以将其视为一个简易的std::function对象.
    template< typename Fn, typename ... Arg >
    class closure : public abstract_runnable
    {
        bool                             inited_{ false };
        typename std::decay< Fn >::type  fn_;
        std::tuple< Arg ... >            arg_;
    public:
        closure() = delete;
        closure(closure const&) = delete;
        closure(closure &&) = delete;
        closure& operator=(closure const&) = delete;
        closure& operator=(closure&&) = delete;

        closure(Fn&& fn, Arg ... arg)
            : fn_(std::forward< Fn >(fn))
            , arg_(std::forward< Arg >(arg) ...)
            , inited_(true)
        {
            ++count_;
        }

        ~closure()
        {
            if (inited_)
                --count_;
        }

        void operator()()
        {
            if (!boost::this_fiber::interrupted())
            {
                try
                {
#if defined(BOOST_NO_CXX17_STD_APPLY)
                    boost::context::detail::apply(std::move(fn_), std::move(arg_));
#else
                    std::apply(std::move(fn_), std::move(arg_));
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
     *  @brief  实例化池对象
     *
     *  @param threads 池中管理的线程数, -1则使用逻辑CPU*2;
     *  @see   get_fiber_pool().
     */
    pool(size_t threads = -1);

    //! 非成员函数, 用于实例化pool对象
    friend pool& get_fiber_pool(size_t threads);

public:
    ~pool();

    enum state_t
    {
        running,    //!< 运行阶段, 此时可以投递任务到池中
        cleaning,   //!< 清理阶段, 此时池正在清理所有未决的任务, 所有post将抛出异常. 
        stoped,
    };

    //! 返回池的状态
    state_t state() const;

    /*!
     *  @brief 投递一个可调用对象作为纤程到纤程池中执行.
     *  
     *  @note  可调用对象抛出的任何异常或返回值都将被丢弃, 若要捕获异常信息或者返回值可以通过
     *      boost::fibers::packaged_task包装后再行投递, 参见pool::async().
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

    /*!
     *  @brief 类似于boost::fibers::async(), 投递任务到池中执行, 返回future.
     *
     *  @note  该方法适用于对于只关心结果而不关心执行流程的任务, 若需要关心执行流程,
     *       比如在某个时候中断任务则建议通过post();
     */
    template< typename Fn, typename ... Args >
    boost::fibers::future<
        typename std::result_of<
        typename std::decay< Fn >::type(typename std::decay< Args >::type ...)
        >::type
    > async(Fn&& fn, Args ... args);

    //! 返回池中所有未决的纤程数.
    size_t fiber_count() const;

    /*!
     *  等待所有未决的任务执行完毕后关闭纤程池.
     *  在等待过程中池的状态将设置为cleaning, 即不允许通过post()投递任务;
     *  函数返回后池的状态将被设置为stoped.
     */
    void shutdown();

protected:
    fiber dispatch(pool::runnable_ptr&& runnable);
};

/*!
 *  @brief 返回fiber_pool::pool的唯一实例.
 *  @param threads 参见pool();
 */
FIBER_POOL_DECL fiber_pool::pool& get_fiber_pool(size_t threads = -1);

template< typename Fn, typename ... Args >
boost::fibers::future<
    typename std::result_of<
    typename std::decay< Fn >::type(typename std::decay< Args >::type ...)
    >::type
  > fiber_pool::pool::async(Fn&& fn, Args ... args)
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

} // fiber_pool

//! 在名称空间外访问
using fiber_pool::get_fiber_pool;

#endif // fiber_pool_h__
