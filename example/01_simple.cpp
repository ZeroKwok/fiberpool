#include "fiber_pool.hpp"
#include <boost/format.hpp>

int main()
{
    boost::mutex g_mutex;

    for (int i = 0; i < 10000; ++i)
    {
        get_fiber_pool().post([](const std::string& name)
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

    auto future = get_fiber_pool().async([]()->int { return 6;  });
    assert(future.get() == 6);

    get_fiber_pool().shutdown();

    return 0;
}


