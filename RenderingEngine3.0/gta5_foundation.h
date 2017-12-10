#pragma once

#include <vulkan\vulkan.h>

namespace rcq
{
	const uint32_t ENVIRONMENT_MAP_SIZE = 128;
	const uint32_t DIR_SHADOW_MAP_SIZE = 1024;
	const uint32_t FRUSTUM_SPLIT_COUNT = 4;

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

	

	


	namespace render_pass_dir_shadow_map_gen
	{
		enum ATT
		{
			ATT_DEPTH,
			ATT_COUNT
		};

		enum DEP
		{
			DEP_BEGIN,
			DEP_END,
			DEP_COUNT
		};

		namespace subpass_unique
		{
			constexpr VkAttachmentReference ref_depth = { ATT_DEPTH,  VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

			constexpr VkSubpassDescription create_subpass()
			{
				VkSubpassDescription subpass = {};
				subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
				subpass.pDepthStencilAttachment = &ref_depth;
				subpass.colorAttachmentCount = 0;
				return subpass;
			}

			
		}//namespace subpass_unique

		constexpr std::array<VkAttachmentDescription, ATT_COUNT> get_attachments()
		{

			std::array<VkAttachmentDescription, ATT_COUNT> atts = {};
			atts[ATT_DEPTH].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			atts[ATT_DEPTH].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
			atts[ATT_DEPTH].format = VK_FORMAT_D32_SFLOAT;
			atts[ATT_DEPTH].samples = VK_SAMPLE_COUNT_1_BIT;
			atts[ATT_DEPTH].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			atts[ATT_DEPTH].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			atts[ATT_DEPTH].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			atts[ATT_DEPTH].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

			return atts;
		}

		constexpr auto get_dependencies()
		{
			std::array<VkSubpassDependency, DEP_COUNT> d = {};
			d[DEP_BEGIN].srcSubpass = VK_SUBPASS_EXTERNAL;
			d[DEP_BEGIN].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			d[DEP_BEGIN].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
			d[DEP_BEGIN].dstSubpass = 0;
			d[DEP_BEGIN].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
			d[DEP_BEGIN].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;

			d[DEP_END].srcSubpass = 0;
			d[DEP_END].srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
			d[DEP_END].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
			d[DEP_END].dstSubpass = VK_SUBPASS_EXTERNAL;
			d[DEP_END].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			d[DEP_END].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			return d;
		}

		constexpr std::array<VkSubpassDependency, DEP_COUNT> deps=get_dependencies();
		std::array<VkAttachmentDescription, ATT_COUNT> atts=get_attachments();
		constexpr VkSubpassDescription sp_unique=subpass_unique::create_subpass();
		
		constexpr VkRenderPassCreateInfo create_create_info()
		{
			VkRenderPassCreateInfo pass = {};
			pass.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
			pass.attachmentCount = ATT_COUNT;
			//pass.dependencyCount = DEP_COUNT;
			pass.pAttachments = atts.data();
			pass.subpassCount = 1;
			pass.pSubpasses = &sp_unique;
			//pass.pDependencies = deps.data();
			return pass;
		}
		constexpr VkRenderPassCreateInfo create_info=create_create_info();
	}//namespace render_pass_dir_shadow_map_gen

	namespace render_pass_preimage_assembler
	{
		enum ATT
		{
			ATT_BASECOLOR_SSAO,
			ATT_METALNESS_SSDS,
			ATT_PREIMAGE,
			ATT_DEPTHSTENCIL,
			ATT_COUNT
		};

		enum SUBPASS
		{
			SUBPASS_SS_DIR_SHADOW_MAP_BLUR,
			SUBPASS_SSAO_MAP_BLUR,
			SUBPASS_SSR_RAY_CASTING,
			SUBPASS_IMAGE_ASSEMBLER,
			SUBPASS_SKY_DRAWER,
			SUBPASS_SUN_DRAWER,
			SUBPASS_COUNT
		};

		enum DEP
		{
			DEP_SSDS_BLUR_IMAGE_ASSEMBLER,
			DEP_SSAO_BLUR_IMAGE_ASSEMBLER,
			DEP_SSR_RAY_CASTING_IMAGE_ASSEMBLER,
			DEP_IMAGE_ASSEMBLER_SKY_DRAWER,
			DEP_SKY_DRAWER_SUN_DRAWER,
			DEP_COUNT
		};

		namespace subpass_ssr_ray_casting
		{
			constexpr VkAttachmentReference ref_depth = { ATT_DEPTHSTENCIL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL };

			constexpr auto create_subpass()
			{
				VkSubpassDescription s = {};
				s.pDepthStencilAttachment = &ref_depth;
				s.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
				return s;
			}

			

		}// namespace ssr_ray_casting

		namespace subpass_sun_drawer
		{
			constexpr VkAttachmentReference preimage = { ATT_PREIMAGE, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
			constexpr VkAttachmentReference ref_depth = { ATT_DEPTHSTENCIL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL };

			constexpr VkSubpassDescription create_subpass()
			{
				VkSubpassDescription s = {};
				s.colorAttachmentCount = 1;
				s.pColorAttachments = &preimage;
				s.pDepthStencilAttachment = &ref_depth;
				s.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
				return s;
			}

			
		}// namespace subpass_sun_drawer

		namespace subpass_sky_drawer
		{
			VkAttachmentReference preimage = { ATT_PREIMAGE, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
			VkAttachmentReference ref_depth = { ATT_DEPTHSTENCIL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL };

			constexpr VkSubpassDescription create_subpass()
			{
				VkSubpassDescription s = {};
				s.colorAttachmentCount = 1;
				s.pColorAttachments = &preimage;
				s.pDepthStencilAttachment = &ref_depth;
				s.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
				return s;
			}

			
		}// namespace subpass_sky_drawer

		namespace subpass_ss_dir_shadow_map_blur
		{
			constexpr VkAttachmentReference ssds_out = { ATT_METALNESS_SSDS, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

			constexpr VkSubpassDescription create_subpass()
			{
				VkSubpassDescription s = {};
				s.colorAttachmentCount = 1;
				s.pColorAttachments = &ssds_out;
				s.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
				return s;
			}

			
		}//namespace subpass_ss_dir_shadow_map_blur

		namespace subpass_ssao_map_blur
		{
			constexpr VkAttachmentReference ssao_out = { ATT_BASECOLOR_SSAO, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
			constexpr uint32_t pres = ATT_METALNESS_SSDS;

			constexpr VkSubpassDescription create_subpass()
			{
				VkSubpassDescription s = {};
				s.colorAttachmentCount = 1;
				s.pColorAttachments = &ssao_out;
				s.preserveAttachmentCount = 1;
				s.pPreserveAttachments = &pres;
				s.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
				return s;
			}
		}//subpass_ssao_map_blur

		namespace subpass_image_assembler
		{
			enum REF_IN
			{
				REF_IN_BASECOLOR_SSAO,
				REF_IN_METALNESS_SSDS,
				REF_IN_COUNT
			};

			constexpr auto create_refs_in()
			{
				std::array<VkAttachmentReference, REF_IN_COUNT> ref = {};
				ref[REF_IN_METALNESS_SSDS].attachment = ATT_METALNESS_SSDS;
				ref[REF_IN_METALNESS_SSDS].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				ref[REF_IN_BASECOLOR_SSAO].attachment = ATT_BASECOLOR_SSAO;
				ref[REF_IN_BASECOLOR_SSAO].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				return ref;
			}
			constexpr std::array<VkAttachmentReference, REF_IN_COUNT>  ref_in = create_refs_in();
			constexpr VkAttachmentReference depth_ref = { ATT_DEPTHSTENCIL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL };
			constexpr VkAttachmentReference color_out = { ATT_PREIMAGE, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

			constexpr VkSubpassDescription create_subpass()
			{
				VkSubpassDescription s = {};
				s.inputAttachmentCount = REF_IN_COUNT;
				s.pInputAttachments = ref_in.data();
				s.colorAttachmentCount = 1;
				s.pColorAttachments = &color_out;
				s.pDepthStencilAttachment = &depth_ref;
				s.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
				return s;
			}


			
		}//namespace subpass_image_assembler

		constexpr auto create_atts()
		{
			std::array<VkAttachmentDescription, ATT_COUNT> atts = {};

			/*atts[ATT_METALNESS_SSDS].format = VK_FORMAT_R32G32B32A32_SFLOAT;
			atts[ATT_METALNESS_SSDS].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			atts[ATT_METALNESS_SSDS].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			atts[ATT_METALNESS_SSDS].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
			atts[ATT_METALNESS_SSDS].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			atts[ATT_METALNESS_SSDS].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			atts[ATT_METALNESS_SSDS].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			atts[ATT_METALNESS_SSDS].samples = VK_SAMPLE_COUNT_1_BIT;

			/*atts[ATT_BASECOLOR_SSAO].format = VK_FORMAT_R32G32B32A32_SFLOAT;
			atts[ATT_BASECOLOR_SSAO].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			atts[ATT_BASECOLOR_SSAO].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			atts[ATT_BASECOLOR_SSAO].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
			atts[ATT_BASECOLOR_SSAO].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			atts[ATT_BASECOLOR_SSAO].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			atts[ATT_BASECOLOR_SSAO].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			atts[ATT_BASECOLOR_SSAO].samples = VK_SAMPLE_COUNT_1_BIT;*/

			/*atts[ATT_PREIMAGE].format = VK_FORMAT_R32G32B32A32_SFLOAT;
			atts[ATT_PREIMAGE].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			atts[ATT_PREIMAGE].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			atts[ATT_PREIMAGE].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			atts[ATT_PREIMAGE].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			atts[ATT_PREIMAGE].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			atts[ATT_PREIMAGE].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			atts[ATT_PREIMAGE].samples = VK_SAMPLE_COUNT_1_BIT;*/

			atts[ATT_DEPTHSTENCIL].format = VK_FORMAT_D32_SFLOAT_S8_UINT;
			atts[ATT_DEPTHSTENCIL].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
			atts[ATT_DEPTHSTENCIL].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
			atts[ATT_DEPTHSTENCIL].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
			atts[ATT_DEPTHSTENCIL].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			atts[ATT_DEPTHSTENCIL].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
			atts[ATT_DEPTHSTENCIL].stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
			atts[ATT_DEPTHSTENCIL].samples = VK_SAMPLE_COUNT_1_BIT;

			return atts;
		}
		constexpr auto atts = create_atts();

		constexpr auto create_deps()
		{
			std::array<VkSubpassDependency, DEP_COUNT> d = {};

			d[DEP_SSAO_BLUR_IMAGE_ASSEMBLER].srcSubpass = SUBPASS_SSAO_MAP_BLUR;
			d[DEP_SSAO_BLUR_IMAGE_ASSEMBLER].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			d[DEP_SSAO_BLUR_IMAGE_ASSEMBLER].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			d[DEP_SSAO_BLUR_IMAGE_ASSEMBLER].dstSubpass = SUBPASS_IMAGE_ASSEMBLER;
			d[DEP_SSAO_BLUR_IMAGE_ASSEMBLER].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			d[DEP_SSAO_BLUR_IMAGE_ASSEMBLER].dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;

			d[DEP_SSDS_BLUR_IMAGE_ASSEMBLER].srcSubpass = SUBPASS_SS_DIR_SHADOW_MAP_BLUR;
			d[DEP_SSDS_BLUR_IMAGE_ASSEMBLER].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			d[DEP_SSDS_BLUR_IMAGE_ASSEMBLER].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			d[DEP_SSDS_BLUR_IMAGE_ASSEMBLER].dstSubpass = SUBPASS_IMAGE_ASSEMBLER;
			d[DEP_SSDS_BLUR_IMAGE_ASSEMBLER].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			d[DEP_SSDS_BLUR_IMAGE_ASSEMBLER].dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;

			d[DEP_SSR_RAY_CASTING_IMAGE_ASSEMBLER].srcSubpass = SUBPASS_SSR_RAY_CASTING;
			d[DEP_SSR_RAY_CASTING_IMAGE_ASSEMBLER].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			d[DEP_SSR_RAY_CASTING_IMAGE_ASSEMBLER].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
			d[DEP_SSR_RAY_CASTING_IMAGE_ASSEMBLER].dstSubpass = SUBPASS_IMAGE_ASSEMBLER;
			d[DEP_SSR_RAY_CASTING_IMAGE_ASSEMBLER].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			d[DEP_SSR_RAY_CASTING_IMAGE_ASSEMBLER].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			d[DEP_IMAGE_ASSEMBLER_SKY_DRAWER].srcSubpass = SUBPASS_IMAGE_ASSEMBLER;
			d[DEP_IMAGE_ASSEMBLER_SKY_DRAWER].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			d[DEP_IMAGE_ASSEMBLER_SKY_DRAWER].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			d[DEP_IMAGE_ASSEMBLER_SKY_DRAWER].dstSubpass = SUBPASS_SKY_DRAWER;
			d[DEP_IMAGE_ASSEMBLER_SKY_DRAWER].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			d[DEP_IMAGE_ASSEMBLER_SKY_DRAWER].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

			d[DEP_SKY_DRAWER_SUN_DRAWER].srcSubpass = SUBPASS_SKY_DRAWER;
			d[DEP_SKY_DRAWER_SUN_DRAWER].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			d[DEP_SKY_DRAWER_SUN_DRAWER].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			d[DEP_SKY_DRAWER_SUN_DRAWER].dstSubpass = SUBPASS_SUN_DRAWER;
			d[DEP_SKY_DRAWER_SUN_DRAWER].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			d[DEP_SKY_DRAWER_SUN_DRAWER].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

			return d;
		}
		constexpr auto deps = create_deps();

		constexpr std::array<VkSubpassDescription, SUBPASS_COUNT> subpasses = {
			subpass_ss_dir_shadow_map_blur::create_subpass(),
			subpass_ssao_map_blur::create_subpass(),
			subpass_ssr_ray_casting::create_subpass(),
			subpass_image_assembler::create_subpass(),
			subpass_sky_drawer::create_subpass(),
			subpass_sun_drawer::create_subpass()
		};

		constexpr auto create_create_info()
		{
			VkRenderPassCreateInfo r = { };
			r.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
			r.attachmentCount = ATT_COUNT;
			r.pAttachments = atts.data();
			r.dependencyCount = DEP_COUNT;
			r.pDependencies = deps.data();
			r.subpassCount = SUBPASS_COUNT;
			r.pSubpasses = subpasses.data();
			
			return r;
		}
		constexpr auto create_info = create_create_info();

	}// namespace render pass preimage assembler

	namespace render_pass_ssao_gen
	{
		enum ATT
		{
			ATT_SSAO_MAP,
			ATT_COUNT
		};

		namespace subpass_unique
		{
			constexpr VkAttachmentReference ref_depth = { ATT_SSAO_MAP, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

			constexpr VkSubpassDescription create_subpass()
			{
				VkSubpassDescription s = {};
				s.pDepthStencilAttachment = &ref_depth;
				s.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
				return s;
			}


			
		}//namespace subpass unique
		
		constexpr auto create_atts()
		{
			std::array<VkAttachmentDescription, ATT_COUNT> atts = {};
			atts[ATT_SSAO_MAP].format = VK_FORMAT_D32_SFLOAT;
			atts[ATT_SSAO_MAP].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			atts[ATT_SSAO_MAP].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
			atts[ATT_SSAO_MAP].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			atts[ATT_SSAO_MAP].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			atts[ATT_SSAO_MAP].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			atts[ATT_SSAO_MAP].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			atts[ATT_SSAO_MAP].samples = VK_SAMPLE_COUNT_1_BIT;

			return atts;
		}
		constexpr auto atts = create_atts();

		constexpr auto subpass = subpass_unique::create_subpass();

		constexpr auto create_create_info()
		{
			VkRenderPassCreateInfo r = {};
			r.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
			r.attachmentCount = ATT_COUNT;
			r.pAttachments = atts.data();
			r.subpassCount = 1;
			r.pSubpasses = &subpass;
			return r;
		}
		constexpr auto create_info = create_create_info();

	}//namespace render pass ssao map gen 


	namespace render_pass_gbuffer_assembler
	{
		enum ATT
		{
			ATT_DEPTHSTENCIL,
			ATT_POS_ROUGHNESS,
			ATT_BASECOLOR_SSAO,
			ATT_METALNESS_SSDS,
			ATT_NORMAL_AO,
			ATT_SS_DIR_SHADOW_MAP,
			ATT_COUNT
		};

		enum SUBPASS
		{
			SUBPASS_GBUFFER_GEN,
			SUBPASS_SS_DIR_SHADOW_MAP_GEN,
			SUBPASS_COUNT
		};

		enum DEP
		{
			DEP_GBUFFER_GEN_SSDS_GEN,
			DEP_COUNT
		};

		namespace subpass_gbuffer_gen
		{
			enum REF
			{
				REF_POS_ROUGHNESS,
				REF_FO_SSAO,
				REF_METALNESS_SSDS,
				REF_NORMAL_AO,
				REF_COUNT
			};

			constexpr auto get_refs_color()
			{
				std::array<VkAttachmentReference, REF_COUNT> refs = {};
				refs[REF_POS_ROUGHNESS].attachment = ATT_POS_ROUGHNESS;
				refs[REF_POS_ROUGHNESS].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

				refs[REF_FO_SSAO].attachment = ATT_BASECOLOR_SSAO;
				refs[REF_FO_SSAO].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

				refs[REF_METALNESS_SSDS].attachment = ATT_METALNESS_SSDS;
				refs[REF_METALNESS_SSDS].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

				refs[REF_NORMAL_AO].attachment = ATT_NORMAL_AO;
				refs[REF_NORMAL_AO].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
				return refs;
			}

			constexpr std::array<VkAttachmentReference, REF_COUNT> refs_color=get_refs_color();
			constexpr VkAttachmentReference ref_depth = { ATT_DEPTHSTENCIL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

			constexpr VkSubpassDescription create_subpass()
			{
				VkSubpassDescription s = {};
				s.colorAttachmentCount = REF_COUNT;
				s.pColorAttachments = refs_color.data();
				s.pDepthStencilAttachment = &ref_depth;
				s.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
				return s;
			}

			

			
		}//namespace subpass_gbuffer_gen

		namespace subpass_ss_dir_shadow_map_gen
		{
			constexpr VkAttachmentReference depth = { ATT_SS_DIR_SHADOW_MAP,  VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

			constexpr auto get_ref_ins()
			{
				std::array<VkAttachmentReference, 2> ins = {};
				ins[0].attachment = ATT_POS_ROUGHNESS;
				ins[0].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				ins[1].attachment = ATT_NORMAL_AO;
				ins[1].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				return ins;
			}
			constexpr std::array<VkAttachmentReference, 2> ref_ins = get_ref_ins();

			constexpr VkSubpassDescription create_subpass()
			{
				VkSubpassDescription s = {};
				s.pDepthStencilAttachment = &depth;
				s.pInputAttachments = ref_ins.data();
				s.inputAttachmentCount = 2;
				s.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
				return s;
			}

			
		}//namespace subpass_ss_dir_shadow_map_gen	

		constexpr auto get_attachments()
		{
			std::array<VkAttachmentDescription, ATT_COUNT> atts = {};

			atts[ATT_POS_ROUGHNESS].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			atts[ATT_POS_ROUGHNESS].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			atts[ATT_POS_ROUGHNESS].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			atts[ATT_POS_ROUGHNESS].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			atts[ATT_POS_ROUGHNESS].format = VK_FORMAT_R32G32B32A32_SFLOAT;
			atts[ATT_POS_ROUGHNESS].samples = VK_SAMPLE_COUNT_1_BIT;

			atts[ATT_BASECOLOR_SSAO].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			atts[ATT_BASECOLOR_SSAO].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			atts[ATT_BASECOLOR_SSAO].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			atts[ATT_BASECOLOR_SSAO].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			atts[ATT_BASECOLOR_SSAO].format = VK_FORMAT_R32G32B32A32_SFLOAT;
			atts[ATT_BASECOLOR_SSAO].samples = VK_SAMPLE_COUNT_1_BIT;

			atts[ATT_METALNESS_SSDS].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			atts[ATT_METALNESS_SSDS].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			atts[ATT_METALNESS_SSDS].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			atts[ATT_METALNESS_SSDS].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			atts[ATT_METALNESS_SSDS].format = VK_FORMAT_R32G32B32A32_SFLOAT;
			atts[ATT_METALNESS_SSDS].samples = VK_SAMPLE_COUNT_1_BIT;

			atts[ATT_NORMAL_AO].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			atts[ATT_NORMAL_AO].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			atts[ATT_NORMAL_AO].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			atts[ATT_NORMAL_AO].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			atts[ATT_NORMAL_AO].format = VK_FORMAT_R32G32B32A32_SFLOAT;
			atts[ATT_NORMAL_AO].samples = VK_SAMPLE_COUNT_1_BIT;

			atts[ATT_DEPTHSTENCIL].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			atts[ATT_DEPTHSTENCIL].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
			atts[ATT_DEPTHSTENCIL].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			atts[ATT_DEPTHSTENCIL].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			atts[ATT_DEPTHSTENCIL].format = VK_FORMAT_D32_SFLOAT_S8_UINT;
			atts[ATT_DEPTHSTENCIL].samples = VK_SAMPLE_COUNT_1_BIT;
			atts[ATT_DEPTHSTENCIL].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			atts[ATT_DEPTHSTENCIL].stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;

			atts[ATT_SS_DIR_SHADOW_MAP].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			atts[ATT_SS_DIR_SHADOW_MAP].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
			atts[ATT_SS_DIR_SHADOW_MAP].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			atts[ATT_SS_DIR_SHADOW_MAP].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			atts[ATT_SS_DIR_SHADOW_MAP].format = VK_FORMAT_D32_SFLOAT;
			atts[ATT_SS_DIR_SHADOW_MAP].samples = VK_SAMPLE_COUNT_1_BIT;

			return atts;
		}

		constexpr auto get_dependencies()
		{
			std::array<VkSubpassDependency, DEP_COUNT> d = {};

			/*d[DEP_EXT_SSDS_GEN].srcSubpass = VK_SUBPASS_EXTERNAL;
			d[DEP_EXT_SSDS_GEN].srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
			d[DEP_EXT_SSDS_GEN].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			d[DEP_EXT_SSDS_GEN].dstSubpass = SUBPASS_SS_DIR_SHADOW_MAP_GEN;
			d[DEP_EXT_SSDS_GEN].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			d[DEP_EXT_SSDS_GEN].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;*/

			d[DEP_GBUFFER_GEN_SSDS_GEN].srcSubpass = SUBPASS_GBUFFER_GEN;
			d[DEP_GBUFFER_GEN_SSDS_GEN].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			d[DEP_GBUFFER_GEN_SSDS_GEN].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			d[DEP_GBUFFER_GEN_SSDS_GEN].dstSubpass = SUBPASS_SS_DIR_SHADOW_MAP_GEN;
			d[DEP_GBUFFER_GEN_SSDS_GEN].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			d[DEP_GBUFFER_GEN_SSDS_GEN].dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;


			/*d[DEP_END].srcSubpass = SUBPASS_IMAGE_ASSEMBLER;
			d[DEP_END].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			d[DEP_END].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			d[DEP_END].dstSubpass = VK_SUBPASS_EXTERNAL;
			d[DEP_END].dstStageMask = VK_PIPELINE_STAGE_;
			d[DEP_END].dstAccessMask = VK_ACCESS_PR;*/

			return d;
		}

		constexpr std::array<VkSubpassDependency, DEP_COUNT> deps=get_dependencies();
		constexpr std::array<VkAttachmentDescription, ATT_COUNT> atts = get_attachments();
		constexpr std::array<VkSubpassDescription, SUBPASS_COUNT> subpasses = 
		{
			subpass_gbuffer_gen::create_subpass(),
			subpass_ss_dir_shadow_map_gen::create_subpass()
		};

		constexpr VkRenderPassCreateInfo create_create_info()
		{
			VkRenderPassCreateInfo pass = {};

			pass.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
			pass.attachmentCount = ATT_COUNT;
			pass.dependencyCount = DEP_COUNT;
			pass.pAttachments = atts.data();
			pass.subpassCount = SUBPASS_COUNT;
			pass.pSubpasses = subpasses.data();
			pass.pDependencies = deps.data();

			return pass;
		}

		constexpr VkRenderPassCreateInfo create_info=create_create_info();
	}//namespace render_pass_frame_image_gen

	namespace render_pass_refraction_image_gen
	{
		enum ATT
		{
			ATT_REFRACTION_IMAGE,
			ATT_DEPTHSTENCIL,
			ATT_COUNT
		};

		namespace subpass_unique
		{
			constexpr VkAttachmentReference ref_depth = { ATT_DEPTHSTENCIL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL };
			constexpr VkAttachmentReference ref_refrimage = { ATT_REFRACTION_IMAGE, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

			constexpr VkSubpassDescription create_subpass()
			{
				VkSubpassDescription s = {};
				s.colorAttachmentCount = 1;
				s.pColorAttachments = &ref_refrimage;
				s.pDepthStencilAttachment = &ref_depth;
				s.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
				return s;
			}

			
		}// namespace subpass_bypass

		constexpr auto get_attachments()
		{
			std::array<VkAttachmentDescription, ATT_COUNT> atts = {};

			atts[ATT_REFRACTION_IMAGE].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			atts[ATT_REFRACTION_IMAGE].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			atts[ATT_REFRACTION_IMAGE].format = VK_FORMAT_R32G32B32A32_SFLOAT;
			atts[ATT_REFRACTION_IMAGE].samples = VK_SAMPLE_COUNT_1_BIT;
			atts[ATT_REFRACTION_IMAGE].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			atts[ATT_REFRACTION_IMAGE].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			atts[ATT_REFRACTION_IMAGE].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			atts[ATT_REFRACTION_IMAGE].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

			atts[ATT_DEPTHSTENCIL].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
			atts[ATT_DEPTHSTENCIL].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			atts[ATT_DEPTHSTENCIL].format = VK_FORMAT_D32_SFLOAT_S8_UINT;
			atts[ATT_DEPTHSTENCIL].samples = VK_SAMPLE_COUNT_1_BIT;
			atts[ATT_DEPTHSTENCIL].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
			atts[ATT_DEPTHSTENCIL].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			atts[ATT_DEPTHSTENCIL].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
			atts[ATT_DEPTHSTENCIL].stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;

			return atts;
		}

		constexpr std::array<VkAttachmentDescription, ATT_COUNT> atts = get_attachments();
		constexpr VkSubpassDescription sp_unique = subpass_unique::create_subpass();

		constexpr VkRenderPassCreateInfo create_create_info()
		{
			VkRenderPassCreateInfo pass = {};
			pass.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
			pass.attachmentCount = ATT_COUNT;
			pass.pAttachments = atts.data();
			pass.subpassCount = 1;
			pass.pSubpasses = &sp_unique;
			return pass;
		}
		constexpr VkRenderPassCreateInfo create_info = create_create_info();

	} //namespace refraction image gen




	namespace render_pass_water
	{
		enum ATT
		{
			ATT_FRAME_IMAGE,
			ATT_DEPTHSTENCIL,
			ATT_COUNT
		};

		namespace subpass_water_drawer
		{
			constexpr VkAttachmentReference ref_frame_im = { ATT_FRAME_IMAGE, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
			constexpr VkAttachmentReference ref_depth = { ATT_DEPTHSTENCIL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

			constexpr VkSubpassDescription create_subpass()
			{
				VkSubpassDescription s = {};
				s.colorAttachmentCount = 1;
				s.pColorAttachments = &ref_frame_im;
				s.pDepthStencilAttachment = &ref_depth;
				s.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
				return s;
			}

			
		} //namespace subpass water_drawer

		constexpr auto subpass = subpass_water_drawer::create_subpass();

		constexpr auto create_atts()
		{
			std::array<VkAttachmentDescription, ATT_COUNT> atts = {};

			atts[ATT_FRAME_IMAGE].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			atts[ATT_FRAME_IMAGE].finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			atts[ATT_FRAME_IMAGE].format = VK_FORMAT_R32G32B32A32_SFLOAT;
			atts[ATT_FRAME_IMAGE].samples = VK_SAMPLE_COUNT_1_BIT;
			atts[ATT_FRAME_IMAGE].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
			atts[ATT_FRAME_IMAGE].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			atts[ATT_FRAME_IMAGE].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			atts[ATT_FRAME_IMAGE].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

			atts[ATT_DEPTHSTENCIL].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			atts[ATT_DEPTHSTENCIL].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			atts[ATT_DEPTHSTENCIL].format = VK_FORMAT_D32_SFLOAT_S8_UINT;
			atts[ATT_DEPTHSTENCIL].samples = VK_SAMPLE_COUNT_1_BIT;
			atts[ATT_DEPTHSTENCIL].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
			atts[ATT_DEPTHSTENCIL].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			atts[ATT_DEPTHSTENCIL].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			atts[ATT_DEPTHSTENCIL].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

			return atts;
		}
		constexpr auto atts = create_atts();

		constexpr VkRenderPassCreateInfo create_create_info()
		{
			VkRenderPassCreateInfo pass = {};
			pass.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
			pass.attachmentCount = ATT_COUNT;
			pass.pAttachments = atts.data();
			pass.subpassCount = 1;
			pass.pSubpasses = &subpass;
			return pass;
		}
		constexpr VkRenderPassCreateInfo create_info = create_create_info();

	} //namespace render_pass_water

	namespace render_pass_postprocessing
	{
		enum ATT
		{
			ATT_SWAP_CHAIN_IMAGE,
			ATT_PREV_IMAGE,
			ATT_COUNT
		};
		enum DEP
		{
			DEP_BEGIN,
			DEP_COUNT
		};

		namespace subpass_bypass
		{
			constexpr std::array<VkAttachmentReference, ATT_COUNT> atts = { 
				 ATT_SWAP_CHAIN_IMAGE, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				 ATT_PREV_IMAGE, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
			};

			constexpr VkSubpassDescription create_subpass()
			{
				VkSubpassDescription s = {};
				s.colorAttachmentCount = atts.size();
				s.pColorAttachments = atts.data();
				s.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
				return s;
			}

			

		}// namespace subpass_bypass

		constexpr std::array<VkAttachmentDescription, ATT_COUNT> get_attachments()
		{

			std::array<VkAttachmentDescription, ATT_COUNT> atts = {};
			atts[ATT_SWAP_CHAIN_IMAGE].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			atts[ATT_SWAP_CHAIN_IMAGE].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
			atts[ATT_SWAP_CHAIN_IMAGE].format = VK_FORMAT_B8G8R8A8_UNORM;
			atts[ATT_SWAP_CHAIN_IMAGE].samples = VK_SAMPLE_COUNT_1_BIT;
			atts[ATT_SWAP_CHAIN_IMAGE].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			atts[ATT_SWAP_CHAIN_IMAGE].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			atts[ATT_SWAP_CHAIN_IMAGE].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			atts[ATT_SWAP_CHAIN_IMAGE].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

			atts[ATT_PREV_IMAGE].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			atts[ATT_PREV_IMAGE].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			atts[ATT_PREV_IMAGE].format = VK_FORMAT_R32G32B32A32_SFLOAT;
			atts[ATT_PREV_IMAGE].samples = VK_SAMPLE_COUNT_1_BIT;
			atts[ATT_PREV_IMAGE].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			atts[ATT_PREV_IMAGE].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			atts[ATT_PREV_IMAGE].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			atts[ATT_PREV_IMAGE].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

			return atts;
		}

		constexpr auto get_dependencies()
		{
			std::array<VkSubpassDependency, DEP_COUNT> d = {};
			d[DEP_BEGIN].srcSubpass = VK_SUBPASS_EXTERNAL;
			d[DEP_BEGIN].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			d[DEP_BEGIN].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			d[DEP_BEGIN].dstSubpass = 0;
			d[DEP_BEGIN].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			d[DEP_BEGIN].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			return d;
		}

		constexpr std::array<VkSubpassDependency, DEP_COUNT> deps = get_dependencies();
		constexpr std::array<VkAttachmentDescription, ATT_COUNT> atts = get_attachments();
		constexpr VkSubpassDescription sp_bypass = subpass_bypass::create_subpass();

		constexpr VkRenderPassCreateInfo create_create_info()
		{
			VkRenderPassCreateInfo pass = {};
			pass.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
			pass.attachmentCount = ATT_COUNT;
			//pass.dependencyCount = DEP_COUNT;
			pass.pAttachments = atts.data();
			pass.subpassCount = 1;
			pass.pSubpasses = &sp_bypass;
			//pass.pDependencies = deps.data();
			return pass;
		}
		constexpr VkRenderPassCreateInfo create_info = create_create_info();

	}// namespace render_pass_postprocessing

	
}