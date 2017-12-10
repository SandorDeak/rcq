#pragma once

#include <stdint.h>

namespace rcq
{
	enum GP : uint32_t
	{
		GP_ENVIRONMENT_MAP_GEN_MAT,
		GP_ENVIRONMENT_MAP_GEN_SKYBOX,

		GP_DIR_SHADOW_MAP_GEN,

		GP_OPAQUE_OBJ_DRAWER,
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
}