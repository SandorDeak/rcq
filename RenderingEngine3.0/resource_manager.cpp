#include "resource_manager.h"

using namespace rcq;

resource_manager* resource_manager::m_instance = nullptr;

resource_manager::resource_manager()
{
}


resource_manager::~resource_manager()
{
}

void resource_manager::init()
{
	if (m_instance != nullptr)
	{
		throw std::runtime_error("engie is already initialised!");
	}
	m_instance = new resource_manager();
}

void resource_manager::destroy()
{
	if (m_instance == nullptr)
	{
		throw std::runtime_error("cannot destroy engine, it doesn't exist!");
	}

	delete m_instance;
}

void resource_manager::build_mesh(const build_mesh_info& info)
{

}
void resource_manager::build_material(const build_mat_info& info)
{

}
void resource_manager::build_transform(const build_tr_info& info)
{

}

void resource_manager::destroy_mesh(unique_id id)
{

}
void resource_manager::destroy_material(unique_id id)
{

}
void resource_manager::destroy_tr(const std::pair<unique_id, USAGE>& info)
{

}