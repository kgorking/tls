#include <vector>
#include <thread>
#include <iostream>
#include <chrono>
#include <mutex>

#include <tls/replicate.h>

using namespace std::chrono_literals;

void send_value(tls::replicate<int>& dest, int value) {
    std::cout << "\nmain: sending value " << value << std::endl;
    dest.write(value);
    std::this_thread::sleep_for(250ms);
}

int main() {
    // A mutex to prevent printing from being messed up
    std::mutex cout_mtx;

    // The replicator
    tls::replicate<int> repl{ 1 };

    // The reader lambda to run on threads
    auto const reader = [&repl, &cout_mtx](int const thread_index, int const start_val) {
        int last_read = start_val;
        int num_reads = 0;
        while (true) {
            int const val = repl.read();

            if (val == -1) {
                std::scoped_lock sl{ cout_mtx };
                std::cout << thread_index << ": exiting\n";
                return;
            }

            if (val != last_read) {
                last_read = val;

                std::scoped_lock sl{ cout_mtx };
                std::cout << "thread " << thread_index << ": got new value '" << val << "', read old value " << num_reads << " times\n";
                num_reads = 0;
            }

            //std::this_thread::sleep_for(5ms);
            std::this_thread::yield(); // will peg the cpu at 100% utilization

            num_reads++;
        }
    };

    // Fire up some threads
    int const num_threads = std::thread::hardware_concurrency();
    std::vector<std::thread> threads;
    for (int i = 1; i <= num_threads; i++)
        threads.emplace_back(reader, i, repl.read());

    // send some values
    tls::replicate<int> other_repl{ 1 };
    for (int i = 0; i < 25; i++) {
        send_value(other_repl, rand());
    }

    // send kill code
    send_value(repl, -1);
    send_value(other_repl, -1);

    for (auto& thr : threads)
        thr.join();
}
