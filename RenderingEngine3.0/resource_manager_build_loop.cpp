#include "resource_manager.h"

using namespace rcq;

void resource_manager::build_loop()
{
	while (!m_should_end_build || !m_build_queue.empty())
	{
		if (m_build_queue.empty())
		{
			std::unique_lock<std::mutex> lock;
			while (m_build_queue.empty())
				m_build_queue_cv.wait(lock);
		}

		base_resource_build_info* info = m_build_queue.front();
		switch (info->resource_type)
		{
		case 0:
			build<0>(info->base_res, info->data);
			break;
		case 1:
			build<1>(info->base_res, info->data);
			break;
		case 2:
			build<2>(info->base_res, info->data);
			break;
		case 3:
			build<3>(info->base_res, info->data);
			break;
		case 4:
			build<4>(info->base_res, info->data);
		case 5:
			build<5>(info->base_res, info->data);
			break;
		}
		static_assert(6 == RES_TYPE_COUNT);

		m_build_queue.pop();

	}
}
