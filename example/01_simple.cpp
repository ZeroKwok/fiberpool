#include "fiber_pool.hpp"
#include <boost/format.hpp>
#include <boost/fiber/all.hpp>

int main()
{
    boost::mutex g_mutex;

    // 投递10000个任务并发执行
    for (int i = 0; i < 10000; ++i)
    {
        get_fiber_pool().post([&](const std::string& name)
        {
            boost::unique_lock<boost::mutex> lock(g_mutex);

            if (boost::this_fiber::interrupted())
                std::cout << "interrupted" <<std::endl;

            std::cout << boost::format("%1$4d - %2$4x - %3%")
                % boost::this_thread::get_id()
                % boost::this_fiber::get_id()
                % name << std::endl;

        }, "lambda-" + std::to_string(i));
    }

    // 投递一个循环任务
    get_fiber_pool().post([]
    {
        while (true)
        {
            // 执行某些操作或者检查
            ;

            // 中断时跳出循环
            if (boost::this_fiber::interrupted())
                break;

            // 每2秒执行一次
            boost::this_fiber::sleep_for(std::chrono::seconds(2));
        }

    });


    // 投递一个异步任务, 并获取其返回值
    auto future = get_fiber_pool().async([]()->int { return 6; });
    assert(future.get() == 6);

    // 终止并回收资源
    get_fiber_pool().shutdown();

    return 0;
}


