#include <tls/replicator.h>
#include <vector>
#include <thread>
#include <iostream>
#include <chrono>
#include <mutex>

using namespace std::chrono_literals;

void send_value(tls::replicator<int>& dest, int value) {
	std::cout << "\nmain: sending value " << value << '\n';
	dest.write(value);
	std::this_thread::sleep_for(1s);
}

int main() {
	tls::replicator<int> repl{ 1 };
	std::mutex cout_mtx;

	// The reader lambda to run on threads
	auto const reader = [&repl, &cout_mtx](int thread_index, int start_val) {
		int last_read = start_val;
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
				std::cout << thread_index << ": got new value " << val << '\n';
			}

			std::this_thread::sleep_for(20ms);
		}
	};

	// Fire up some threads
	constexpr int num_threads = 32;
	std::vector<std::thread> threads;
	for (int i = 0; i < num_threads; i++)
		threads.emplace_back(reader, i, repl.read());

	// send some values
	for (int i = 0; i < 25; i++) {
		send_value(repl, rand());
	}

	// send kill code
	send_value(repl, -1);

	for (auto& thr : threads)
		thr.join();
}
