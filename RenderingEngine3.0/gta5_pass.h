#pragma once
#include "foundation.h"

#include "foundation2.h"
#include "queue.h"
#include "array.h"
#include "vector.h"
#include "vk_memory_resource.h"
#include "vk_allocator.h"
#include "pool_memory_resource.h"
#include "freelist_resource_host.h"
#include "monotonic_buffer_resource.h"
#include "slot_map.h"

namespace rcq
{
	class gta5_pass
	{
	public:
		gta5_pass(const gta5_pass&) = delete;
		gta5_pass(gta5_pass&&) = delete;
		~gta5_pass();

		static void init(const base_info& info);
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
		enum RES_DATA
		{
			RES_DATA_ENVIRONMENT_MAP_GEN_MAT,
			RES_DATA_ENVIRONMENT_MAP_GEN_SKYBOX,
			RES_DATA_GBUFFER_GEN,
			RES_DATA_DIR_SHADOW_MAP_GEN,
			RES_DATA_SS_DIR_SHADOW_MAP_GEN,
			RES_DATA_IMAGE_ASSEMBLER,
			RES_DATA_SKY_DRAWER,
			RES_DATA_SUN_DRAWER,
			RES_DATA_TERRAIN_DRAWER,
			RES_DATA_TERRAIN_TILE_REQUEST,
			RES_DATA_WATER_COMPUTE,
			RES_DATA_WATER_DRAWER,
			RES_DATA_REFRACTION_MAP_GEN,
			RES_DATA_SSR_RAY_CASTING,
			RES_DATA_COUNT
		};
		enum RES_IMAGE
		{
			RES_IMAGE_ENVIRONMENT_MAP_GEN_DEPTHSTENCIL,
			RES_IMAGE_ENVIRONMENT_MAP,
			RES_IMAGE_GB_POS_ROUGHNESS,
			RES_IMAGE_GB_BASECOLOR_SSAO,
			RES_IMAGE_GB_METALNESS_SSDS,
			RES_IMAGE_GB_NORMAL_AO,
			RES_IMAGE_GB_DEPTH,
			RES_IMAGE_DIR_SHADOW_MAP,
			RES_IMAGE_SS_DIR_SHADOW_MAP,
			RES_IMAGE_SSAO_MAP,
			RES_IMAGE_PREIMAGE,
			RES_IMAGE_PREV_IMAGE,
			RES_IMAGE_REFRACTION_IMAGE,
			RES_IMAGE_SSR_RAY_CASTING_COORDS,
			RES_IMAGE_BLOOM_BLUR,
			RES_IMAGE_COUNT
		};


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

		enum SAMPLER
		{
			SAMPLER_UNNORMALIZED_COORD,
			SAMPLER_GENERAL,
			SAMPLER_COUNT
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

		static const uint32_t ENVIRONMENT_MAP_SIZE = 128;
		static const uint32_t DIR_SHADOW_MAP_SIZE = 1024;
		static const uint32_t FRUSTUM_SPLIT_COUNT = 4;

		enum RENDER_PASS
		{
			RENDER_PASS_ENVIRONMENT_MAP_GEN,
			RENDER_PASS_DIR_SHADOW_MAP_GEN,
			RENDER_PASS_GBUFFER_ASSEMBLER,
			RENDER_PASS_SSAO_MAP_GEN,
			RENDER_PASS_PREIMAGE_ASSEMBLER,
			RENDER_PASS_REFRACTION_IMAGE_GEN,
			RENDER_PASS_WATER,
			RENDER_PASS_POSTPROCESSING,
			RENDER_PASS_COUNT
		};

		enum GP
		{
			GP_ENVIRONMENT_MAP_GEN_MAT,
			GP_ENVIRONMENT_MAP_GEN_SKYBOX,

			GP_DIR_SHADOW_MAP_GEN,

			GP_GBUFFER_GEN,
			GP_SS_DIR_SHADOW_MAP_GEN,
			GP_SS_DIR_SHADOW_MAP_BLUR,
			GP_SSAO_GEN,
			GP_SSAO_BLUR,
			GP_SSR_RAY_CASTING,
			GP_IMAGE_ASSEMBLER,
			GP_TERRAIN_DRAWER,
			GP_SKY_DRAWER,
			GP_SUN_DRAWER,

			GP_REFRACTION_IMAGE_GEN,
			GP_WATER_DRAWER,

			GP_POSTPROCESSING,
			GP_COUNT
		};

		enum CP
		{
			CP_TERRAIN_TILE_REQUEST,
			CP_WATER_FFT,
			CP_BLOOM_BLUR,
			CP_COUNT
		};

		struct pipeline
		{
			VkPipeline gp;
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

		template<uint32_t res_data>
		struct resource_data;

		template<>
		struct resource_data<RES_DATA_ENVIRONMENT_MAP_GEN_MAT>
		{
			glm::mat4 view;
			glm::vec3 dir; //in worlds space
			uint32_t padding0;
			glm::vec3 irradiance;
			uint32_t padding1;
			glm::vec3 ambient_irradiance;
			uint32_t padding2;
		};
		template<>
		struct resource_data<RES_DATA_ENVIRONMENT_MAP_GEN_SKYBOX>
		{
			glm::mat4 view;
		};
		template<>
		struct resource_data<RES_DATA_GBUFFER_GEN>
		{
			glm::mat4 proj_x_view;
			glm::vec3 cam_pos;
		};
		template<>
		struct resource_data<RES_DATA_DIR_SHADOW_MAP_GEN>
		{
			glm::mat4 projs[FRUSTUM_SPLIT_COUNT];
		};
		template<>
		struct resource_data<RES_DATA_SS_DIR_SHADOW_MAP_GEN>
		{
			glm::mat4 projs[FRUSTUM_SPLIT_COUNT];
			glm::mat4 view;
			glm::vec3 light_dir;
			float near;
			float far;
		};
		template<>
		struct resource_data<RES_DATA_IMAGE_ASSEMBLER>
		{
			glm::mat4 previous_proj_x_view;
			glm::vec3 previous_view_pos;
			uint32_t padding0;
			glm::vec3 dir; //in view space
			float height_bias;
			glm::vec3 irradiance;
			uint32_t padding1;
			glm::vec3 ambient_irradiance;
			uint32_t padding2;
			glm::vec3 cam_pos;
			uint32_t padding3;
		};
		template<>
		struct resource_data<RES_DATA_SKY_DRAWER>
		{
			glm::mat4 proj_x_view_at_origin;
			glm::vec3 light_dir;
			float height;
			glm::vec3 irradiance;
			uint32_t padding0;
		};
		template<>
		struct resource_data<RES_DATA_SUN_DRAWER>
		{
			glm::mat4 proj_x_view_at_origin;
			glm::vec3 light_dir;
			uint32_t padding0;
			glm::vec3 helper_dir;
			float height;
			glm::vec3 irradiance;
			uint32_t padding1;
		};
		template<>
		struct resource_data<RES_DATA_TERRAIN_DRAWER>
		{
			glm::mat4 proj_x_view;
			glm::vec3 view_pos;
			uint32_t padding0;
		};
		template<>
		struct resource_data<RES_DATA_TERRAIN_TILE_REQUEST>
		{
			glm::vec3 view_pos;
			float near;
			float far;
			uint32_t padding0[3];
		};
		template<>
		struct resource_data<RES_DATA_WATER_DRAWER>
		{
			glm::mat4 proj_x_view;
			glm::mat4 mirrored_proj_x_view;
			glm::vec3 view_pos;
			float height_bias;
			glm::vec3 light_dir;
			uint32_t padding0;
			glm::vec3 irradiance;
			uint32_t padding1;
			glm::vec3 ambient_irradiance;
			uint32_t padding2;
			glm::vec2 tile_offset;
			glm::vec2 tile_size_in_meter;
			glm::vec2 half_resolution;
		};
		template<>
		struct resource_data<RES_DATA_REFRACTION_MAP_GEN>
		{
			glm::mat4 proj_x_view;
			glm::vec3 view_pos_at_ground;
			float far;
		};
		template<>
		struct resource_data<RES_DATA_SSR_RAY_CASTING>
		{
			glm::mat4 proj_x_view;
			glm::vec3 view_pos;
			float ray_length;
		};
		template<>
		struct resource_data<RES_DATA_WATER_COMPUTE>
		{
			glm::vec2 wind_dir;
			float one_over_wind_speed_to_the_4;
			float time;
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
		void create_dsls_and_allocate_dss();
		void create_graphics_pipelines();
		void create_compute_pipelines();
		void create_resources();
		void update_descriptor_sets();
		void create_command_pool();
		void create_samplers();
		void create_framebuffers();
		void allocate_command_buffers();
		void record_present_command_buffers();
		void process_settings();
		std::array<glm::mat4, FRUSTUM_SPLIT_COUNT> calc_projs();
		void create_sync_objects();

		VkRenderPass m_passes[RENDER_PASS_COUNT];
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
		freelist_resource_host m_host_memory_resource;
		monotonic_buffer_resource m_device_memory_resource;
		vk_memory_resource m_vk_mappable_memory_resource;
		monotonic_buffer_resource m_mappable_memory_resource;
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
	};
}