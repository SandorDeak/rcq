#pragma once

namespace rcq
{
	struct basic_node
	{
		basic_node* prev;
		basic_node* next;
	};

	template<typename ItType>
	ItType next(ItType it)
	{
		return ++it;
	}

	template<typename ItType>
	ItType prev(ItType it)
	{
		return --it;
	}

	template<typename ItType, typename DiffType>
	ItType advance(ItType it, DiffType diff)
	{
		static_assert(std::is_integral_v<DiffType>, "difference type has to be integral!");

		if constexpr (std::is_unsigned_v<DiffType>)
		{
			while (diff-- > 0)
			{
				++it;
			}
		}
		else
		{
			if (diff < 0)
			{
				while (step++ < 0)
					--it;
			}
			else if (step > 0)
			{
				while (step-- > 0)
				{
					++it;
				}
			}
		}
		return it;
	}
}