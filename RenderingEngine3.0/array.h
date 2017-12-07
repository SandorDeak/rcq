#pragma once

namespace rcq
{
	template<typename T, size_t _size>
	class alignas(alignof(T)) array
	{
	public:
		constexpr T& operator[](size_t index)
		{
			return *reinterpret_cast<T*>(m_data + sizeof(T)*index);
		}

		constexpr size_t size()
		{
			return _size;
		}

		constexpr T* data()
		{
			return reinterpret_cast<T*>(m_data);
		}

	private:
		char m_data[sizeof(T)*_size];
	};
}