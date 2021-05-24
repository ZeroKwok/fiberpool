#ifndef fiber_pool_h__
#define fiber_pool_h__

/*!
 *  @file   fiber_pool.hpp
 * 
 *  @author zero kwok
 *  @date   2020-10
 *
 *  @note   2021-03 已知bug, 在fiber_pool中使用boost::fibers::promise时可能导致等待线程中卡主, 请用std::promise代替.
 *
 */

#if defined(FIBERPOOL_BUILD_SHARED_LIB)                 //!< build shared lib
#   define FIBER_POOL_DECL __declspec(dllexport)
#elif defined(FIBERPOOL_USING_SHARED_LIB)               //!< using shared lib
#   define FIBER_POOL_DECL __declspec(dllimport)
#else
#   define FIBER_POOL_DECL                              //!< using or build static lib
#endif

 // Defined Private encapsulation implement
#define FIBER_POOL_DECL_PRIVATE(type) \
    intptr_t __##type##_private

#define FIBER_POOL_INIT_PRIVATE(type, ...) \
    __##type##_private = reinterpret_cast<intptr_t>( \
        new type##_private(__VA_ARGS__)); 

#define FIBER_POOL_FREE_PRIVATE(type) \
    do { \
        if(__##type##_private != 0) { \
            delete reinterpret_cast< \
                type##_private *>(__##type##_private); \
            __##type##_private = 0; \
        } \
    } while(0)

#define FIBER_POOL_PRIVATE(type) \
    (*reinterpret_cast<type##_private *>(__##type##_private))

#if defined(WIN32) || defined(_WIN32)
#   ifndef WIN32_LEAN_AND_MEAN
#      define  WIN32_LEAN_AND_MEAN 
#   endif
#   ifndef  NOMINMAX
#      define  NOMINMAX
#   endif
#endif

#if _MSC_VER
#   ifndef _SILENCE_CXX17_RESULT_OF_DEPRECATION_WARNING
#       define _SILENCE_CXX17_RESULT_OF_DEPRECATION_WARNING 1
#   endif
#endif

#ifndef BOOST_CONTEXT_DYN_LINK
#    define BOOST_CONTEXT_DYN_LINK 1
#endif

#include <boost/any.hpp>
#include <boost/atomic.hpp>
#include <boost/thread.hpp>
#include <boost/fiber/fiber.hpp>
#include <boost/fiber/future.hpp>
#include <boost/fiber/condition_variable.hpp>

/*!
 *  扩展boost::this_fiber
 */
namespace boost {
    namespace this_fiber {
        /*!
         *  返回当前纤程是否被中断
         */
        FIBER_POOL_DECL bool interrupted();

        /*!
         *  绑定当前纤程到当前线程
         *  fiber_pool承诺绑定后纤程不会在线程间切换
         */
        FIBER_POOL_DECL void bind_thread();

        /*!
         *  返回当前纤程绑定的用户数据
         */
        FIBER_POOL_DECL boost::any& data();
    }
}

namespace fiber_pool {

/*!
 *  @brief 扩展boost::fibers::fiber
 *         使之可以优雅的终止未决的任务, 但同时使之丧失运行纤程的能力.
 *  @note  另一个区别是fiber对象析构时会自动与未决的纤程分离.
 */
class FIBER_POOL_DECL fiber
{
public:
    typedef boost::fibers::fiber::id id;

    fiber();
    fiber(const fiber& right);
    fiber(boost::fibers::fiber&& fiber);

    fiber & operator=(const fiber& right);

    id   get_id() const noexcept;
    bool finshed() const noexcept;      //!< 判断是否执行完成(执行完成或已经被中断)
    bool joinable() const noexcept;     //!< 类似于thread::joinable()(执行中或执行完成)
    void join();                        //!< 等待执行完成或者中断.
    void interrupt();                   //!< 请求中断该纤程, 参见 interrupted().
    void interrupt_on_destruct();       //!< 析构时请求中断

protected:
#if _MSC_VER
#   pragma warning (push)
#   pragma warning (disable:4251)
#endif
    std::shared_ptr<struct fiber_private> m_private;
#if _MSC_VER
#   pragma warning (pop)
#endif
};

/*!
 *  纤程池
 *  内部维护多个工作线程使之共享执行所有投递到池中的任务
 */
class FIBER_POOL_DECL pool
{
private:
    FIBER_POOL_DECL_PRIVATE(pool);

    /*!
     *  可运行对象的抽象
     */
    class FIBER_POOL_DECL abstract_runnable
    {
    public:
        virtual ~abstract_runnable() {}
        virtual void operator()() = 0;
                void increment();
                void decrement();
                void finish();
        static size_t count();
    };

    typedef std::unique_ptr<abstract_runnable> runnable_ptr;

    /*!
     *  可运行对象的封装, 联合参数一起构成闭包, 可以将其视为一个简易的std::function对象.
     */
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
            increment();
        }

        ~closure()
        {
            if (inited_)
                decrement();
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

            finish();
        }
    };

    /*!
     *  @brief  实例化池对象
     *
     *  @param threads 池中管理的线程数, -1则使用逻辑CPU*2;
     *  @see   get_fiber_pool().
     */
    pool(size_t threads = -1);

    /*!
     *  非成员函数, 用于实例化pool对象
     */
    friend FIBER_POOL_DECL pool& get_fiber_pool(size_t threads);

public:
    ~pool();

    enum state_t
    {
        running,    //!< 运行阶段, 此时可以投递任务到池中.
        waiting,    //!< 等待阶段, 此时池正在等待所有未决的任务, 所有dispatch将抛出异常. 
        cleaning,   //!< 清理阶段, 此时池正在终止所有未决的任务, 所有dispatch将抛出异常.
        stoped,     //!< 停止阶段, 此时池不执行任何操作, 并且不能再次运行.
    };

    /*!
     *  返回池的状态
     */
    state_t state() const noexcept;

    /*!
     *  @brief 投递一个可调用对象作为任务到纤程池中执行.
     *
     *  @note  可调用对象抛出的任何异常或返回值都将被丢弃, 若要捕获异常信息或者返回值可以通过
     *         std::packaged_task包装后再行投递.
     *  @see   pool::async().
     */
    template<typename Fn, typename ... Arg>
    fiber post(Fn&& fn, Arg ... arg)
    {
        // 这里参数不能为: Arg&& ... arg. 这可能导致传递引用类型到closure(), 
        // 从而让其保存了参数的应用而不是拷贝, 在未来使用时即发生未定义行为.
        // GuoJH By 2021-5-24

        if (state() != running)
            throw std::runtime_error("The task cannot be delivered at this time.");

        return std::move(dispatch(
            runnable_ptr(new closure<Fn, Arg ...>{ 
                std::forward< Fn >(fn), std::forward< Arg >(arg) ... })));
    }

    /*!
     *  @brief 类似于std::async(), 投递可调用对象到池中执行并返回future.
     *
     *  @note  该方法适用于对于只关心结果而不关心执行流程的任务, 若需要关心执行流程,
     *         比如在某个时候中断任务则建议通过post();
     *  @see   dispatch().
     */
    template< typename Fn, typename ... Args >
    boost::fibers::future<
        typename std::result_of<
        typename std::decay< Fn >::type(typename std::decay< Args >::type ...)
        >::type
    > async(Fn&& fn, Args ... args)
    {
        typedef typename std::result_of<
            typename std::decay< Fn >::type(typename std::decay< Args >::type ...)
        >::type     result_type;

        boost::fibers::packaged_task< result_type(
            typename std::decay< Args >::type ...) > pt{
            std::forward< Fn >(fn) };
        boost::fibers::future< result_type > f{ pt.get_future() };

        post(std::move(pt), std::forward< Args >(args) ...);

        return f;
    }

    /*!
     *  返回池中所有未决的纤程数.
     */
    size_t fiber_count() const noexcept;

    /*!
     *  @brief 停止分派任务并关闭纤程池
     *
     *  @param wait true 等待所有未决的任务执行完毕后关闭纤程池.
     *              false 不等待, 中断所有正在执行的任务, 还没有执行的任务将直接丢弃.
     *
     *  @note  在等待过程中池的状态将设置为waiting, 而另一种状态是cleaning, 这两种状态均不允许通过dispatch()分派任务,
     *         将抛出std::runtime_error()异常, 返回后池的状态将被设置为stoped.
     */
    void shutdown(bool wait = true) noexcept;

protected:

    /*!
     *  @brief 分派可调用对象到纤程池中
     * 
     *  @param runnable 表示一个可执行对象, 类似一个闭包.
     *  @return 返回指向该未决任务的句柄
     *  @note 如果池的状态state() != running, 将抛出std::runtime_error()异常.
     */
    fiber dispatch(pool::runnable_ptr&& runnable);
};

/*!
 *  @brief 返回fiber_pool::pool的唯一实例.
 *  @param threads 参见pool();
 *  @note  fiber_pool视第一次get_fiber_pool()的调用线程为"主线程", 
 *         fiber_pool承诺fiber绝不会在"主线程"中执行.
 */
FIBER_POOL_DECL fiber_pool::pool& get_fiber_pool(size_t threads = -1);

} // fiber_pool

/*!
 *  在名称空间外访问
 */
using fiber_pool::get_fiber_pool;

#endif // fiber_pool_h__
