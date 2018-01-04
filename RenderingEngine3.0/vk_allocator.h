#pragma once

#include "vulkan.h"
#include "host_memory.h"

namespace rcq
{
	class vk_allocator
	{
	public:
		vk_allocator() {}

		vk_allocator(host_memory* memory) :
			m_memory(memory)
		{
			m_callbacks.pUserData = reinterpret_cast<void*>(this);
			m_callbacks.pfnAllocation = &alloc_static;
			m_callbacks.pfnFree = &free_static;
			m_callbacks.pfnReallocation = &realloc_static;
			m_callbacks.pfnInternalAllocation = &internal_alloc_notification_static;
			m_callbacks.pfnInternalFree = &internal_free_notification_static;
		}

		void init(host_memory* memory)
		{
			m_memory = memory;

			m_callbacks.pUserData = reinterpret_cast<void*>(this);
			m_callbacks.pfnAllocation = &alloc_static;
			m_callbacks.pfnFree = &free_static;
			m_callbacks.pfnReallocation = &realloc_static;
			m_callbacks.pfnInternalAllocation = &internal_alloc_notification_static;
			m_callbacks.pfnInternalFree = &internal_free_notification_static;
		}

		operator const VkAllocationCallbacks*() const
		{
			return &m_callbacks;
		}
	private:
		VkAllocationCallbacks m_callbacks;
		host_memory* m_memory;

		void* alloc(size_t size, size_t alignment)
		{
			return reinterpret_cast<void*>(m_memory->allocate(size, alignment));  
		}

		void free(void* ptr)
		{
			if (ptr!=nullptr)
				m_memory->deallocate(reinterpret_cast<size_t>(ptr));
		}

		void* realloc(void* original, size_t size, size_t alignment, VkSystemAllocationScope scope)
		{
			//should be written!!!
			return nullptr;
		}

		void internal_alloc_notification(size_t size, VkInternalAllocationType type, VkSystemAllocationScope scope)
		{
		}

		void internal_free_notification(size_t size, VkInternalAllocationType type, VkSystemAllocationScope scope)
		{
		}

		static void* VKAPI_CALL alloc_static(void* user_data, size_t size, size_t alignment, VkSystemAllocationScope alloc_scope)
		{
			return static_cast<vk_allocator*>(user_data)->alloc(size, alignment);
		}

		static void VKAPI_CALL free_static(void* user_data, void* ptr)
		{
			static_cast<vk_allocator*>(user_data)->free(ptr);
		}

		static void* VKAPI_CALL realloc_static(void* user_data, void* original, size_t size, size_t alignment, VkSystemAllocationScope alloc_scope)
		{
			return static_cast<vk_allocator*>(user_data)->realloc(original, size, alignment, alloc_scope);
		}

		static void VKAPI_CALL internal_alloc_notification_static(void* user_data, size_t size, VkInternalAllocationType type,
			VkSystemAllocationScope scope)
		{
			static_cast<vk_allocator*>(user_data)->internal_alloc_notification(size, type, scope);
		}

		static void VKAPI_CALL internal_free_notification_static(void* user_data, size_t size, VkInternalAllocationType type,
			VkSystemAllocationScope scope)
		{
			static_cast<vk_allocator*>(user_data)->internal_free_notification(size, type, scope);
		}
	};
}