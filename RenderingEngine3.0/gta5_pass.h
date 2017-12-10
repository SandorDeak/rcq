#pragma once
//#include "foundation.h"

//#include "foundation2.h"
#include "queue.h"
#include "array.h"
#include "vector.h"
#include "vk_memory.h"
#include "vk_allocator.h"
#include "pool_device_memory.h"
#include "freelist_host_memory.h"
#include "monotonic_buffer_device_memory.h"
#include "slot_map.h"

namespace rcq
{
	class gta5_pass
	{
	public:
		gta5_pass(const gta5_pass&) = delete;
		gta5_pass(gta5_pass&&) = delete;
		~gta5_pass();

		static void init(const base_info& info, size_t place);
		static void destroy();
		static gta5_pass* instance() { return m_instance; }

		void render();

		slot add_opaque_object(base_resource* mesh, base_resource* opaque_material, base_resource* transform)
		{
			slot ret;
			renderable<REND_TYPE_OPAQUE_OBJECT>* new_obj = m_opaque_objects.push(ret);
			new_obj->mesh_vb = reinterpret_cast<resource<RES_TYPE_MESH>*>(mesh->data)->vb;
			new_obj->mesh_ib = reinterpret_cast<resource<RES_TYPE_MESH>*>(mesh->data)->ib;
			new_obj->mesh_veb = reinterpret_cast<resource<RES_TYPE_MESH>*>(mesh->data)->veb;
			new_obj->mesh_index_size = reinterpret_cast<resource<RES_TYPE_MESH>*>(mesh->data)->size;

			new_obj->mat_opaque_ds = reinterpret_cast<resource<RES_TYPE_MAT_OPAQUE>*>(opaque_material->data)->ds;
			new_obj->tr_ds = reinterpret_cast<resource<RES_TYPE_TR>*>(transform->data)->ds;

			return ret;
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
		
		


		enum MEMORY
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
		};

		enum SECONDARY_CB
		{
			SECONDARY_CB_MAT_EM,
			SECONDARY_CB_SKYBOX_EM,
			SECONDARY_CB_DIR_SHADOW_GEN,
			SECONDARY_CB_MAT_OPAQUE,
			SECONDARY_CB_TERRAIN_DRAWER,
			SECONDARY_CB_COUNT
		};

		struct pipeline
		{
			VkPipeline ppl;
			VkPipelineLayout pl;
			VkDescriptorSet ds;
			VkDescriptorSetLayout dsl;
			void create_layout(VkDevice device, const std::vector<VkDescriptorSetLayout>& dsls, 
				const VkAllocationCallbacks* alloc, const std::vector<VkPushConstantRange>& push_constants = {})
			{
				std::vector<VkDescriptorSet> all_dsl(1 + dsls.size());
				all_dsl[0] = dsl;
				std::copy(dsls.begin(), dsls.end(), all_dsl.begin() + 1);
				pl = rcq::create_layout(device, all_dsl, push_constants, alloc);
			}
			void create_dsl(VkDevice device, const VkDescriptorSetLayoutCreateInfo& create_info, const VkAllocationCallbacks* alloc)
			{
				assert(vkCreateDescriptorSetLayout(device, &create_info, alloc, &dsl) == VK_SUCCESS);
			}
			void bind(VkCommandBuffer cb, VkPipelineBindPoint bind_point= VK_PIPELINE_BIND_POINT_GRAPHICS)
			{
				vkCmdBindPipeline(cb, bind_point, gp);
				vkCmdBindDescriptorSets(cb, bind_point, pl, 0, 1, &ds, 0, nullptr);
			}
		};

		struct res_image
		{
			VkImage image;
			VkImageView view;
		};

		

		struct framebuffers
		{
			VkFramebuffer environment_map_gen;
			VkFramebuffer dir_shadow_map_gen;
			VkFramebuffer gbuffer_assembler;
			VkFramebuffer ssao_map_gen;
			VkFramebuffer preimage_assembler;
			VkFramebuffer refraction_image_gen;
			VkFramebuffer water;
			VkFramebuffer postprocessing[SWAP_CHAIN_SIZE];
		};

		struct res_data
		{
			uint64_t data[RES_DATA_COUNT];

			uint64_t buffer_offset;
			VkBuffer buffer;
			uint64_t staging_buffer_offset;
			VkBuffer staging_buffer;
			VkDeviceMemory staging_buffer_mem;
			size_t size;
			std::array<size_t, RES_DATA_COUNT> offsets;

			static constexpr std::array<size_t, RES_DATA_COUNT> get_sizes()
			{
				return get_sizes_impl(std::make_index_sequence<RES_DATA_COUNT>());
			}

			template<size_t... res_data>
			static constexpr std::array<size_t, RES_DATA_COUNT> get_sizes_impl(std::index_sequence<res_data...>)
			{
				return { sizeof(ressource_data<res_data>), ... };
			}

			void calc_offset_and_size(uint64_t alignment)
			{
				constexpr auto res_data_sizes = res_data::get_sizes();
				size = 0;
				for (uint32_t i = 0; i < RES_DATA_COUNT; ++i)
				{
					offsets[i] = size;
					size += memory_resource::align(alignment, res_data_sizes[i]);
				}
			}

			//template<size_t... indices>
			void set_pointers(uint64_t base/*, std::index_sequence<indices...>*/)
			{
				for(uint32_t i=0; i<RES_DATA_COUNT; ++i)
				{ 
					data[i] = base + offsets[i];
				}
				/*
				auto l = { ([](auto&& p, char* val) {
					p = reinterpret_cast<std::remove_reference_t<decltype(p)>>(val);
				}(std::get<indices>(data), base + std::get<indices>(offsets))
					, 0)... };*/
			}
			template<uint32_t res_data>
			auto get()
			{
				return reinterpret_cast<resource_data<res_data>*>(data[res_data]);
			}
		};

		gta5_pass(const base_info& info);

		static gta5_pass* m_instance;
		const base_info& m_base;

		void create_memory_resources_and_containers();
		void create_render_passes();
		void create_graphics_pipelines();
		void create_compute_pipelines();
		void create_buffers_and_images();
		void allocate_and_update_dss();
		void create_descriptor_pool();
		void create_command_pool();
		void create_samplers();
		void create_framebuffers();
		void allocate_command_buffers();
		void record_present_command_buffers();
		void process_settings();
		std::array<glm::mat4, FRUSTUM_SPLIT_COUNT> calc_projs();
		void create_sync_objects();

		VkRenderPass m_rps[RENDER_PASS_COUNT];
		pipeline m_gps[GP_COUNT];
		pipeline m_cps[CP_COUNT];

		//resources
		VkDescriptorPool m_dp;
		res_data m_res_data;
		res_image m_res_image[RES_IMAGE_COUNT];
		VkSampler m_samplers[SAMPLER_COUNT];

		//render
		VkCommandPool m_graphics_cp;
		VkCommandPool m_present_cp;
		VkCommandPool m_compute_cp;
		framebuffers m_fbs;
		VkCommandBuffer m_render_cb;
		VkCommandBuffer m_terrain_request_cb;
		VkCommandBuffer m_water_fft_cb;
		VkCommandBuffer m_bloom_blur_cb;
		VkCommandBuffer m_present_cbs[SWAP_CHAIN_SIZE];
		VkCommandBuffer m_secondary_cbs[SECONDARY_CB_COUNT];
		VkSemaphore m_image_available_s;
		VkSemaphore m_render_finished_s;
		VkSemaphore m_present_ready_ss[SWAP_CHAIN_SIZE];
		VkFence m_render_finished_f;
		VkFence m_compute_finished_f;
		VkEvent m_water_tex_ready_e;
		VkSemaphore m_preimage_ready_s;
		VkSemaphore m_bloom_blur_ready_s;

		//memory resources
		freelist_host_memory m_host_memory;

		vk_memory m_vk_device_memory;
		monotonic_buffer_device_memory m_device_memory;

		vk_memory m_vk_mappable_memory;
		monotonic_buffer_device_memory m_mappable_memory;

		vk_allocator m_vk_alloc;


		slot_map<renderable<REND_TYPE_OPAQUE_OBJECT>> m_opaque_objects;
		renderable<REND_TYPE_SKY> m_sky;
		bool m_sky_valid;
		renderable<REND_TYPE_TERRAIN> m_terrain;
		bool m_terrain_valid;
		renderable<REND_TYPE_WATER> m_water;
		glm::uvec2 m_water_tiles_count;
		bool m_water_valid;

		render_settings m_render_settings;

		std::atomic_bool m_render_dispatched;

		glm::mat4 m_previous_proj_x_view;
		glm::vec3 m_previous_view_pos;

		//helper functions
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

		template<uint32_t rp_type>
		void create_render_pass_impl(VkRenderPass* rp);

		template<uint32_t... rp_types>
		void create_render_passes_impl(std::index_sequence<rp_types...>);

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


	};
}