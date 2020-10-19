#include "fiber_pool.hpp"
#include <boost/fiber/channel_op_status.hpp>

#define CATCH_CONFIG_RUNNER
#include "catch.hpp"

using boost::fibers::future;


TEST_CASE("No of threads and fibers", "[default-pool]")
{
    SECTION("Initial")
    {
        CHECK(get_fiber_pool().state() == fiber_pool::pool::running);
    }

    SECTION("Submit 5 task")
    {
        // testing for number of currently running fibers
        // can be tricky, because all fibers are executed
        // asynchroniusly on a different thread than 
        // this code is run. 

        // we can solve this problem by using conditional variables
        // and suspending the current thread till number
        // of fibers reaches given value

        std::atomic<size_t> fiber_count{ 0 };
        boost::fibers::mutex mtx_count{};
        boost::fibers::condition_variable_any cnd_count{};

        std::vector<future<void>> ofs;

        for (size_t i = 0; i < 5; ++i)
        {
            ofs.emplace_back(get_fiber_pool().async(
                [&fiber_count, &cnd_count]()
            {
                // increase fiber cound and notify one thread
                // (i.e., the thread of this test) of the update
                ++fiber_count;
                cnd_count.notify_one();

                boost::this_fiber::sleep_for(std::chrono::seconds(1));
            }));
        }

        {
            // the main thread waits here till fiber_count reaches 5
            std::unique_lock<boost::fibers::mutex> lk{ mtx_count };
            cnd_count.wait(lk, [&fiber_count]() {return fiber_count == 5; });

            // when this happens we execute a check whether fiber_count()
            // actually indicated 5 fibers
            CHECK(get_fiber_pool().fiber_count() == 5);
        }

        // wait for all fibers to finish
        for (auto&& of : ofs)
            of.wait();

        // now fibers number should be 0
        CHECK(get_fiber_pool().fiber_count() == 0);
    }
}

TEST_CASE("Return value", "[default-pool]")
{
    SECTION("Using return statement, no input params")
    {
        auto opt_future = get_fiber_pool().async([]()
        {
            size_t r{ 0 };
            for (auto i : { 1,2,3 })
                r += i;

            return r;
        });

        auto r = opt_future.get();

        CHECK(r == 6);
    }

    SECTION("Using return statement, with input params")
    {
        struct InputObj
        {
            size_t val{ 5 };
        } in_obj;

        auto opt_future = get_fiber_pool().async(
            [](InputObj const& _in_obj)
        {
            size_t r{ 0 };
            for (auto i : { 1,2,3 })
                r += _in_obj.val;

            return r;
        }, std::cref(in_obj));

        auto r = opt_future.get();

        CHECK(r == 15);
    }

    SECTION("Using reference param")
    {
        std::vector<size_t> vec;

        auto opt_future = get_fiber_pool().async(
            [](std::vector<size_t>& _in_obj)
        {
            boost::this_fiber::sleep_for(std::chrono::milliseconds(100));
            _in_obj = { 1,2,3 };
        }, std::ref(vec));

        opt_future.wait();

        size_t vec_sum{};

        for (auto v : vec)
            vec_sum += v;

        CHECK(vec_sum == 6);
    }
}

TEST_CASE("Throw exception", "[default-pool]")
{
    auto opt_future1 = get_fiber_pool().async([]()
    {
        boost::this_fiber::sleep_for(std::chrono::seconds(1));
        throw std::runtime_error("some exception");
        return false;
    });

    auto opt_future2 = get_fiber_pool().async([]()
    {
        boost::this_fiber::sleep_for(std::chrono::milliseconds(500));
        throw std::runtime_error("some exception");
        return false;
    });

    CHECK_THROWS(opt_future1.get());
    CHECK_THROWS(opt_future2.get());

    CHECK(get_fiber_pool().fiber_count() == 0);
}

int main(int argc, char* argv[])
{
    int result = Catch::Session().run(argc, argv);
    get_fiber_pool().shutdown();
    return result;
}