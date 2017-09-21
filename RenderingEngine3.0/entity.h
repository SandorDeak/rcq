#pragma once

#include "structs.h"


class entity
{
public:
	entity();
	virtual ~entity();

protected:
	friend class scene;

	rcq::unique_id m_mesh_id;
	rcq::unique_id m_material_id;
	rcq::unique_id m_transform_id;
	rcq::unique_id m_id;
};

