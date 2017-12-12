#include "resource_manager.h"

using namespace rcq;

void resource_manager::destroy_loop()
{
	while (!m_should_end_destroy || !m_destroy_queue.empty())
	{
		if (m_destroy_queue.empty())
		{
			std::unique_lock<std::mutex> lock(m_destroy_queue_mutex);
			while (m_destroy_queue.empty())
				m_destroy_queue_cv.wait(lock);
		}

		base_resource* base_res = *m_destroy_queue.front();

		while (!base_res->ready_bit.load());

		switch (base_res->res_type)
		{
		case 0:
			destroy<0>(base_res);
			break;
		case 1:
			destroy<1>(base_res);
			break;
		case 2:
			destroy<2>(base_res);
			break;
		case 3:
			destroy<3>(base_res);
			break;
		case 4:
			destroy<4>(base_res);
			break;
		case 5:
			destroy<5>(base_res);
			break;
		}
		static_assert(6 == RES_TYPE_COUNT);

		m_destroy_queue.pop();
	}
}