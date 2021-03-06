#include "engine.h"

#include "resources.h"
#include "terrain_manager.h"
#include "rp_gbuffer_assembler.h"

#include "enum_cp.h"
#include "enum_rp.h"
#include "enum_gp.h"

using namespace rcq;

void engine::set_terrain(base_resource* terrain, base_resource** opaque_materials)
{
	bool types_valid = terrain->res_type == RES_TYPE_TERRAIN;
	for (uint32_t i = 0; i < 4; ++i)
	{
		types_valid = types_valid && ((*opaque_materials)->res_type == RES_TYPE_MAT_OPAQUE);
		while (!(*opaque_materials)->ready_bit.load());
		++opaque_materials;
	}
	assert(types_valid);
	opaque_materials -= 4;
	while (!terrain->ready_bit.load());
	

	auto t = reinterpret_cast<resource<RES_TYPE_TERRAIN>*>(terrain->data);

	terrain_manager::instance()->set_terrain(t);
	terrain_manager::instance()->init_resources(t->tile_count, t->level0_tile_size, t->mip_level_count,
		m_cps[CP_TERRAIN_TILE_REQUEST].ds, m_gps[GP_TERRAIN_DRAWER].ds);

	m_terrain.ds = t->ds;
	m_terrain.request_ds = t->request_ds;
	for (auto& mat_ds : m_terrain.opaque_material_dss)
		mat_ds = reinterpret_cast<resource<RES_TYPE_MAT_OPAQUE>*>((*(opaque_materials++))->data)->ds;

	//record terrain request cb
	{
		auto cb = m_cbs[CB_TERRAIN_REQUEST];
		VkCommandBufferBeginInfo begin = {};
		begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		begin.flags = 0;

		assert(vkBeginCommandBuffer(cb, &begin) == VK_SUCCESS);

		m_cps[CP_TERRAIN_TILE_REQUEST].bind(cb, VK_PIPELINE_BIND_POINT_COMPUTE);
		vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_COMPUTE, m_cps[CP_TERRAIN_TILE_REQUEST].pl,
			1, 1, &m_terrain.request_ds, 0, nullptr);

		glm::uvec2 group_count = t->tile_count;
		vkCmdDispatch(cb, group_count.x, group_count.y, 1);

		assert(vkEndCommandBuffer(cb) == VK_SUCCESS);
	}

	//record terrain drawer cb
	{
		VkCommandBufferInheritanceInfo inheritance = {};
		inheritance.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
		inheritance.framebuffer = m_fbs[FB_GBUFFER_ASSEMBLER];
		inheritance.occlusionQueryEnable = VK_FALSE;
		inheritance.renderPass = m_rps[RP_GBUFFER_ASSEMBLER];
		inheritance.subpass = rp_create_info<RP_GBUFFER_ASSEMBLER>::SUBPASS_GBUFFER_GEN;

		auto cb = m_secondary_cbs[SECONDARY_CB_TERRAIN_DRAWER];
		VkCommandBufferBeginInfo begin = {};
		begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		begin.pInheritanceInfo = &inheritance;
		begin.flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT | VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

		assert(vkBeginCommandBuffer(cb, &begin) == VK_SUCCESS);
		m_gps[GP_TERRAIN_DRAWER].bind(cb);
		vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_BEGIN_RANGE, m_gps[GP_TERRAIN_DRAWER].pl,
			1, 1, &m_terrain.ds, 0, nullptr);
		vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_gps[GP_TERRAIN_DRAWER].pl,
			2, 4, m_terrain.opaque_material_dss,
			0, nullptr);
		vkCmdDraw(cb, 4 * t->tile_count.x,
			t->tile_count.y, 0, 0);

		assert(vkEndCommandBuffer(cb) == VK_SUCCESS);
	}

	m_terrain_valid = true;
}

void engine::destroy_terrain()
{
	m_terrain_valid = false;
	VkFence fences[2] = { m_fences[FENCE_COMPUTE_FINISHED], m_fences[FENCE_RENDER_FINISHED] };
	vkWaitForFences(m_base.device, 2, fences, VK_TRUE, std::numeric_limits<uint64_t>::max());
	terrain_manager::instance()->destroy_resources();
}
