#pragma once

#include "structs.h"

class entity
{
public:
	entity(rcq::USAGE u);
	virtual ~entity();

protected:
	mesh m_mesh;
	size_t m_material_index;
	transform m_transform;
	int render_id;
};

