#include "containers.h"

using namespace rcq;

containers* containers::m_instance = nullptr;

containers::containers()
{
	m_pool_mat_static.resize(POOL_MAT_STATIC_SIZE);
	m_pool_mat_dynamic.resize(POOL_MAT_DYNAMIC_SIZE);
	m_pool_tr_static.resize(POOL_TR_STATIC_SIZE);
	m_pool_tr_dynamic.resize(POOL_TR_DYNAMIC_SIZE);

	for (size_t i = 0; i < POOL_MAT_STATIC_SIZE; ++i)
		m_available_mat_static.push(i);
	for (size_t i = 0; i < POOL_MAT_DYNAMIC_SIZE; ++i)
		m_available_mat_dynamic.push(i);
	for (size_t i = 0; i < POOL_TR_STATIC_SIZE; ++i)
		m_available_tr_static.push(i);
	for (size_t i = 0; i < POOL_TR_DYNAMIC_SIZE; ++i)
		m_available_tr_dynamic.push(i);
}


containers::~containers()
{
}

void containers::init()
{
	if (m_instance != nullptr)
	{
		throw std::runtime_error("containers are already initialised!");
	}

	m_instance = new containers();
}

void containers::destroy()
{
	if (m_instance == nullptr)
	{
		throw std::runtime_error("cannot destroy containers, they don't exist!");
	}

	delete m_instance;
}

material_data* containers::new_mat(USAGE usage)
{
	size_t index;
	material_data* ret=nullptr;
	switch (usage)
	{
	case USAGE_STATIC:
		if (m_available_mat_static.empty())
		{
			throw std::runtime_error("out of material container memory!");
		}
		index = m_available_mat_static.top();
		m_available_mat_static.pop();
		ret=&m_pool_mat_static[index];
		break;
	case USAGE_DYNAMIC:
		if (m_available_mat_dynamic.empty())
		{
			throw std::runtime_error("out of material container memory!");
		}
		index = m_available_mat_dynamic.top();
		m_available_mat_dynamic.pop();
		ret = &m_pool_mat_dynamic[index];
		break;
	}
	return ret;
}

void containers::delete_mat(material_data* p, USAGE usage)
{
	size_t index;
	switch (usage)
	{
	case USAGE_STATIC:
		index = p - m_pool_mat_static.data();
		if (index<0 || index>POOL_MAT_STATIC_SIZE)
		{
			throw std::runtime_error("invalid pointer, cannot delete material");
		}
		m_available_mat_static.push(index);
		break;
	case USAGE_DYNAMIC:
		index = p - m_pool_mat_dynamic.data();
		if (index<0 || index>POOL_MAT_DYNAMIC_SIZE)
		{
			throw std::runtime_error("invalid pointer, cannot delete material");
		}
		m_available_mat_dynamic.push(index);
		break;
	}
}

transform_data* containers::new_tr(USAGE usage)
{
	size_t index;
	transform_data* ret=nullptr;
	switch (usage)
	{
	case USAGE_STATIC:
		if (m_available_tr_static.empty())
		{
			throw std::runtime_error("out of material container memory!");
		}
		index = m_available_tr_static.top();
		m_available_tr_static.pop();
		ret = &m_pool_tr_static[index];
		break;
	case USAGE_DYNAMIC:
		if (m_available_tr_dynamic.empty())
		{
			throw std::runtime_error("out of material container memory!");
		}
		index = m_available_tr_dynamic.top();
		m_available_tr_dynamic.pop();
		ret = &m_pool_tr_dynamic[index];
		break;
	}
	return ret;
}

void containers::delete_tr(transform_data* p, USAGE usage)
{
	size_t index;
	switch (usage)
	{
	case USAGE_STATIC:
		index = p - m_pool_tr_static.data();
		if (index<0 || index>POOL_TR_STATIC_SIZE)
		{
			throw std::runtime_error("invalid pointer, cannot delete material");
		}
		m_available_tr_static.push(index);
		break;
	case USAGE_DYNAMIC:
		index = p - m_pool_tr_dynamic.data();
		if (index<0 || index>POOL_TR_DYNAMIC_SIZE)
		{
			throw std::runtime_error("invalid pointer, cannot delete material");
		}
		m_available_tr_dynamic.push(index);
		break;
	}
}