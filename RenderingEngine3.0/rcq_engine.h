#pragma once

#include "base.h"
#include "resource_manager.h"
#include "engine.h"
#include "terrain_manager.h"

namespace rcq_user
{
	enum class resource : uint32_t
	{
		opaque_material = rcq::RES_TYPE_MAT_OPAQUE,
		sky = rcq::RES_TYPE_SKY,
		transform = rcq::RES_TYPE_TR,
		water = rcq::RES_TYPE_WATER,
		mesh = rcq::RES_TYPE_MESH,
		terrain = rcq::RES_TYPE_TERRAIN
	};

	enum class renderable : uint32_t
	{
		opaque_object = rcq::REND_TYPE_OPAQUE_OBJECT,
		water = rcq::REND_TYPE_WATER,
		sky = rcq::REND_TYPE_SKY,
		terrain = rcq::REND_TYPE_TERRAIN
	};

	inline void init()
	{
		rcq::base_create_info base_create;

		const char* device_extensions[]=
		{
			VK_KHR_SWAPCHAIN_EXTENSION_NAME
		};
		const char* instance_extensions[]=
		{
			VK_EXT_DEBUG_REPORT_EXTENSION_NAME
		};
		const char* validation_layers[] =
		{
			"VK_LAYER_LUNARG_standard_validation"
		};

		base_create.enable_validation_layers = true;
		base_create.device_extensions = device_extensions;
		base_create.device_extensions_count = 1;
		base_create.instance_extensions = instance_extensions;
		base_create.instance_extensions_count = 1;
		base_create.validation_layers = validation_layers;
		base_create.validation_layer_count = 1;
		
		base_create.window_name = "RCQ Engine";
		base_create.device_features.samplerAnisotropy = VK_TRUE;
		base_create.device_features.geometryShader = VK_TRUE;
		base_create.device_features.tessellationShader = VK_TRUE;
		base_create.device_features.depthBounds = VK_TRUE;
		base_create.device_features.sparseBinding = VK_TRUE;
		base_create.device_features.fillModeNonSolid = VK_TRUE;

		rcq::base::init(base_create);

		auto& b = rcq::base::instance()->get_info();
		rcq::resource_manager::init(b);
		rcq::engine::instance()->init(b);
		rcq::terrain_manager::init(b);
	}

	inline void destroy()
	{
		rcq::terrain_manager::destroy();
		rcq::engine::destroy();
		rcq::resource_manager::destroy();
		rcq::base::destroy();
	}

	struct resource_handle;
	struct renderable_handle;

	template<resource res>
	struct build_info : public rcq::resource<static_cast<uint32_t>(res)>::build_info {};

	struct resource_handle 
	{ 
	private:
		rcq::base_resource* value;

		friend void destroy_resource(resource_handle);
		template<resource res> friend void build_resource(resource_handle*, build_info<res>**);
		friend void add_opaque_object(resource_handle, resource_handle, resource_handle, renderable_handle*);
		friend void set_sky(resource_handle);
		friend void set_water(resource_handle);
		friend void set_terrain(resource_handle);
	};

	struct renderable_handle
	{
	private: 
		rcq::slot value;

		friend void add_opaque_object(resource_handle, resource_handle, resource_handle,
			renderable_handle*);

		friend void destroy_opaque_object(renderable_handle);
	};
	
	template<resource res>
	inline void build_resource(resource_handle* handle, build_info<res>** build_info)
	{
		rcq::resource_manager::instance()->build_resource<static_cast<uint32_t>(res)>(&handle->value, build_info);	
	}
	inline void destroy_resource(resource_handle handle)
	{
		rcq::resource_manager::instance()->destroy_resource(handle.value);
	}
	inline void dispatch_resource_builds()
	{
		rcq::resource_manager::instance()->dispach_builds();
	}
	inline void dispatch_resource_destroys()
	{
		rcq::resource_manager::instance()->dispatch_destroys();
	}
	inline void add_opaque_object(resource_handle mesh, resource_handle opaque_material, resource_handle transform,
		renderable_handle* handle)
	{
		rcq::engine::instance()->add_opaque_object(mesh.value, opaque_material.value, transform.value, &handle->value);
	}
	inline void destroy_opaque_object(renderable_handle handle)
	{
		rcq::engine::instance()->destroy_opaque_object(handle.value);
	}
	inline void set_sky(resource_handle sky_resource_handle)
	{
		rcq::engine::instance()->set_sky(sky_resource_handle.value);
	}
	inline void set_terrain(resource_handle terrain_resource_handle)
	{
		rcq::engine::instance()->set_terrain(terrain_resource_handle.value);
	}
	inline void set_water(resource_handle water_resource_handle)
	{
		rcq::engine::instance()->set_water(water_resource_handle.value);
	}
	inline void destroy_sky()
	{
		rcq::engine::instance()->destroy_sky();
	}
	inline void destroy_water()
	{
		rcq::engine::instance()->destroy_water();
	}
	inline void destroy_terrain()
	{
		rcq::engine::instance()->destroy_terrain();
	}

	inline GLFWwindow* get_window()
	{
		return rcq::base::instance()->get_info().window;
	}
}