#include "core.h"

using namespace rcq;

core* core::m_instance = nullptr;

core::core()
{
	m_should_end = false;
	m_looping_thread = std::thread(std::bind(&core::loop, this));
}


core::~core()
{
}

void core::init()
{
	if (m_instance != nullptr)
	{
		throw std::runtime_error("core is already initialised!");
	}
	m_instance = new core();
}

void core::destroy()
{
	if (m_instance == nullptr)
	{
		throw std::runtime_error("cannot destroy core, it doesn't exist!");
	}

	delete m_instance;
}

void core::loop()
{
	while (!m_should_end)
	{
		if (!m_package_queue.empty())
		{
			//TODO: process package

			//TODO: render
			

		}
	}
}
