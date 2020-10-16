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

// ��չboost::this_fiber
namespace boost {
    namespace this_fiber {
        FIBER_POOL_DECL bool interrupted();
    }
}

namespace fiber_pool {

// ��չboost::fibers::fiber
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
 *   �˳̳ض���
 *   �ڲ�ά�������߳�
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
        running,    ///< ���н׶�, ��ʱ����Ͷ�����񵽳���
        cleaning,   ///< ����׶�, ��ʱ��������������δ��������, ����post���׳��쳣. 
        stoped,
    };

    /// ���سص�״̬
    state_t state() const;

    /// �����ж���ĳ���
    struct abstract_runnable
    {
        static boost::atomic_size_t count_;

        virtual ~abstract_runnable() {}
        virtual void operator()() = 0;
    };

    typedef std::unique_ptr<abstract_runnable> runnable_ptr;

    /// �����ж���ķ�װ, ���ϲ���һ�𹹳ɱհ�
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
     *  �ύһ���ɵ��ö�����Ϊ�˳̵������߳�ִ��.
     *  �ɵ��ö����׳����κ��쳣��������.
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

    /// ���س�������δ�����˳���.
    size_t fiber_count() const;

    /*!
     *   �ȴ�����δ��������ִ����Ϻ�ر��˳̳�.
     *   �ڵȴ������гص�״̬������Ϊcleaning, ��������ͨ��post()Ͷ������;
     *   �������غ�ص�״̬��������Ϊstoped.
     */
    void shutdown();

protected:
    fiber dispatch(pool::runnable_ptr&& runnable);
};

} // fiber_pool

/// ����fiber_pool��Ψһʵ��
FIBER_POOL_DECL fiber_pool::pool& get_fiber_pool(size_t threads = -1);

/*! 
 *   Ͷ�����񵽳���ִ��, ����future. ������async
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
