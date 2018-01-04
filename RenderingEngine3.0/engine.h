#pragma once

#include "vk_memory.h"
#include "vk_allocator.h"
#include "freelist_host_memory.h"
#include "monotonic_buffer_device_memory.h"

#include "slot_map.h"

#include "renderables.h"
#include "res_data.h"
#include "res_image.h"
#include "resources.h"
#include "pipeline.h"
#include "base_info.h"
#include "render_settings.h"

#include "const_frustum_split_count.h"
#include "const_swap_chain_image_count.h"

#include "enum_rp.h"
#include "enum_cp.h"
#include "enum_gp.h"
#include "enum_secondary_cb.h"
#include "enum_fb.h"
#include "enum_res_image.h"
#include "enum_sampler_type.h"
#include "enum_semaphore.h"
#include "enum_fence.h"
#include "enum_event.h"
#include "enum_cb.h"
#include "enum_cpool.h"


namespace rcq
{
	class engine
	{
	public:
		static void init(const base_info& info);
		static void destroy();
		static engine* instance() { return m_instance; }

		void render()
		{
			calc_projs();
			process_render_settings();
			record_and_submit();
		}

		void add_opaque_object(base_resource* mesh, base_resource* opaque_material, base_resource* transform, slot* s)
		{
			renderable<REND_TYPE_OPAQUE_OBJECT>* new_obj = m_opaque_objects.push(*s);
			new_obj->mesh_vb = reinterpret_cast<resource<RES_TYPE_MESH>*>(mesh->data)->vb;
			new_obj->mesh_ib = reinterpret_cast<resource<RES_TYPE_MESH>*>(mesh->data)->ib;
			new_obj->mesh_veb = reinterpret_cast<resource<RES_TYPE_MESH>*>(mesh->data)->veb;
			new_obj->mesh_index_size = reinterpret_cast<resource<RES_TYPE_MESH>*>(mesh->data)->size;

			new_obj->mat_opaque_ds = reinterpret_cast<resource<RES_TYPE_MAT_OPAQUE>*>(opaque_material->data)->ds;
			new_obj->tr_ds = reinterpret_cast<resource<RES_TYPE_TR>*>(transform->data)->ds;
		}

		bool destroy_opaque_object(slot s)
		{
			return m_opaque_objects.destroy(s);
		}

		void set_sky(base_resource* sky)
		{
			m_sky.ds = reinterpret_cast<resource<RES_TYPE_SKY>*>(sky->data)->ds;
			m_sky_valid = true;
		}

		void set_terrain(base_resource* terrain);
		void set_water(base_resource* water);

		void destroy_sky()
		{
			m_sky_valid = false;
		}

		void destroy_terrain()
		{
			m_terrain_valid = false;
		}

		void destroy_water()
		{
			m_water_valid = false;
		}

	private:

		//ctor, dtor, singleton pattern
		engine(const base_info& info);
		~engine();
		engine(const engine&) = delete;
		engine(engine&&) = delete;
		engine& operator=(const engine&) = delete;
		engine& operator=(engine&&) = delete;
		static engine* m_instance;

		//create functions
		void create_memory_resources_and_containers();
		void create_render_passes();
		void create_graphics_pipelines();
		void create_compute_pipelines();
		void create_buffers_and_images();
		void allocate_and_record_cbs();
		void allocate_and_update_dss();
		void create_descriptor_pool();
		void create_samplers();
		void create_framebuffers();
		void create_sync_objects();

		//base info
		const base_info& m_base;

		//render functions
		void calc_projs();
		void process_render_settings();
		void record_and_submit();
		
		//render passes, graphics and compute pipelines
		VkRenderPass m_rps[RP_COUNT];
		pipeline m_gps[GP_COUNT];
		pipeline m_cps[CP_COUNT];

		//command pools
		VkCommandPool m_cpools[CPOOL_COUNT];

		//command buffers
		VkCommandBuffer m_cbs[CB_COUNT];
		VkCommandBuffer m_present_cbs[SWAP_CHAIN_IMAGE_COUNT];
		VkCommandBuffer m_secondary_cbs[SECONDARY_CB_COUNT];

		//synchronization objects
		VkSemaphore m_semaphores[SEMAPHORE_COUNT];
		VkSemaphore m_present_ready_ss[SWAP_CHAIN_IMAGE_COUNT];
		VkFence m_fences[FENCE_COUNT];
		VkEvent m_events[EVENT_COUNT];
		std::atomic_bool m_render_dispatched;

		//framebuffers
		VkFramebuffer m_fbs[FB_COUNT];
		VkFramebuffer m_postprocessing_fbs[SWAP_CHAIN_IMAGE_COUNT];

		//descriptor pool
		VkDescriptorPool m_dp;

		//samplers
		VkSampler m_samplers[SAMPLER_TYPE_COUNT];

		//resources
		res_data m_res_data;
		res_image m_res_image[RES_IMAGE_COUNT];

		//memory resources
		freelist_host_memory m_host_memory;
		vk_memory m_vk_device_memory;
		monotonic_buffer_device_memory m_device_memory;
		vk_memory m_vk_mappable_memory;
		monotonic_buffer_device_memory m_mappable_memory;
		vk_allocator m_vk_alloc;

		//renderables
		slot_map<renderable<REND_TYPE_OPAQUE_OBJECT>> m_opaque_objects;
		renderable<REND_TYPE_SKY> m_sky;
		bool m_sky_valid;
		renderable<REND_TYPE_TERRAIN> m_terrain;
		bool m_terrain_valid;
		renderable<REND_TYPE_WATER> m_water;
		glm::uvec2 m_water_tiles_count;
		bool m_water_valid;

		//info for rendering
		render_settings m_render_settings;
		glm::mat4 m_previous_proj_x_view;
		glm::vec3 m_previous_view_pos;
		glm::mat4 m_dir_shadow_projs[FRUSTUM_SPLIT_COUNT];


		//helper functions for creating pipelines and render passes
		template<uint32_t gp_id>
		void prepare_gp_create_info(VkGraphicsPipelineCreateInfo& create_info, VkPipelineLayoutCreateInfo& layout,
			VkPipelineShaderStageCreateInfo* shaders, VkShaderModuleCreateInfo* shader_modules, uint32_t& shader_index,
			VkDescriptorSetLayout* dsls, uint32_t& dsl_index,
			char* code, uint32_t code_index);

		template<uint32_t... gp_ids>
		void prepare_gp_create_infos(std::index_sequence<gp_ids...>, 
			VkGraphicsPipelineCreateInfo* create_infos, VkPipelineLayoutCreateInfo* layouts,
			VkPipelineShaderStageCreateInfo* shaders, VkShaderModuleCreateInfo* shader_modules, uint32_t& shader_index,
			VkDescriptorSetLayout* dsls, uint32_t& dsl_index,
			char* code, uint32_t code_index);

		template<uint32_t cp_id>
		void prepare_cp_create_info(VkComputePipelineCreateInfo& create_info, VkPipelineLayoutCreateInfo& layout, 
			VkShaderModuleCreateInfo& shader_module,
			VkDescriptorSetLayout* dsls, uint32_t& dsl_index,
			char* code, uint32_t code_index);

		template<uint32_t... cp_ids>
		void prepare_cp_create_infos(std::index_sequence<cp_ids...>,
			VkComputePipelineCreateInfo* create_infos, VkPipelineLayoutCreateInfo* layouts,
			VkShaderModuleCreateInfo* shader_modules,
			VkDescriptorSetLayout* dsls, uint32_t& dsl_index,
			char* code, uint32_t code_index);

		template<uint32_t rp_type>
		void create_render_pass_impl(VkRenderPass* rp);

		template<uint32_t... rp_types>
		void create_render_passes_impl(std::index_sequence<rp_types...>);
	};
}

/*enum MEMORY
{
MEMORY_STAGING_BUFFER,
MEMORY_BUFFER,
MEMORY_ENVIRONMENT_MAP_GEN_DEPTHSTENCIL,
MEMORY_ENVIRONMENT_MAP,
MEMORY_GB_POS_ROUGHNESS,
MEMORY_GB_BASECOLOR_SSAO,
MEMORY_GB_METALNESS_SSDS,
MEMORY_GB_NORMAL_AO,
MEMORY_GB_DEPTH,
MEMORY_DIR_SHADOW_MAP,
MEMORY_SS_DIR_SHADOW_MAP,
MEMORY_SSAO_MAP,
MEMORY_PREIMAGE,
MEMORY_PREV_IMAGE,
MEMORY_REFRACTION_IMAGE,
MEMORY_SSR_RAY_CASTING_COORDS,
MEMORY_BLOOM_BLUR,
MEMORY_COUNT
};*/