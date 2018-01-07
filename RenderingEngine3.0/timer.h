#pragma once

#include <chrono>
#include <thread>

namespace rcq
{
	class timer
	{
	public:
		void start()
		{
			time = std::chrono::high_resolution_clock::now();
		}
		void stop()
		{
			auto end_time = std::chrono::high_resolution_clock::now();
			duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - time).count() / 1000000.f;
		}
		float get()
		{
			return duration;
		}
		void wait_until(std::chrono::milliseconds d)
		{
			duration = d.count() / 1000.f;
			std::this_thread::sleep_until(time + d);

		}
	private:
		std::chrono::time_point<std::chrono::steady_clock> time;
		float duration;
	};
}