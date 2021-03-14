#include "fiber_pool.hpp"
#include <boost/format.hpp>
#include <boost/fiber/all.hpp>

int main()
{
    boost::mutex g_mutex;

    auto loop = [](int index)
    {
        boost::thread::id frist = boost::this_thread::get_id();

        // 绑定到线程
        //boost::this_fiber::bind_thread();
        //std::cout << "bind to thread: " << frist << std::endl;

        for (int i = 0; i < 1000; ++i)
        {
            // 执行某些操作或者检查
            ;

            // 中断时跳出循环
            if (boost::this_fiber::interrupted())
                break;

            // 每2秒执行一次
            boost::this_fiber::sleep_for(std::chrono::milliseconds(100));

            if (frist != boost::this_thread::get_id())
                std::cout << "The thread has been switched!" << std::endl;
        }
    };

    // 需要禁用主线程调度 !!!
    // 投递循环任务
    for (int i = 0; i < 1; ++i)
    {
        get_fiber_pool().post(loop, i);
    }

    //// 投递10000个任务并发执行
    //for (int i = 0; i < 10000; ++i)
    //{
    //    get_fiber_pool().post([&](const std::string& name)
    //    {
    //        boost::unique_lock<boost::mutex> lock(g_mutex);

    //        if (boost::this_fiber::interrupted())
    //            std::cout << "interrupted" <<std::endl;

    //        std::cout << boost::format("%1$4d - %2$4x - %3%")
    //            % boost::this_thread::get_id()
    //            % boost::this_fiber::get_id()
    //            % name << std::endl;

    //    }, "lambda-" + std::to_string(i));
    //}


    // 投递一个异步任务, 并获取其返回值
    auto future = get_fiber_pool().async([]()->int { return 6; });
    assert(future.get() == 6);

    // 终止并回收资源
    get_fiber_pool().shutdown();

    return 0;
}


