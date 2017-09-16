#pragma once

#include "structs.h"

class entity
{
public:
	entity();
	virtual ~entity();

protected:
	mesh m_mesh;
	size_t m_material_index;
	transform m_transform;
	rcq::unique_id id;
};

