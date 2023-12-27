#pragma once

#include <random>

#include <glm/glm.hpp>

#include "pcg_random.hpp"

#define RNG pcg32

const float MAX_FLTUINT = (float)std::numeric_limits<uint32_t>::max();

namespace Walnut {

	class Random
	{
	public:
		static void Init()
		{
			//pcg_extras::seed_seq_from<std::random_device> seed_source;
			//s_RandomEngine.seed(seed_source);
			s_RandomEngine.seed(1337);
		}

		static uint32_t UInt()
		{
			return s_Distribution(s_RandomEngine);
		}

		static uint32_t UInt(uint32_t min, uint32_t max)
		{
			return min + (s_Distribution(s_RandomEngine) % (max - min + 1));
		}

		static float Float()
		{
			return (float)s_Distribution(s_RandomEngine) / MAX_FLTUINT;
		}

		static float Float(float min, float max)
		{
			return Float() * (max - min) + min;
		}

		static glm::vec3 Vec3()
		{
			return glm::vec3(Float(), Float(), Float());
		}

		static glm::vec3 Vec3(float min, float max)
		{
			float minmax = (max - min) + min;
			return glm::vec3(Float() * minmax, Float() * minmax, Float() * minmax);
		}

		static glm::vec3 InUnitSphere()
		{
			return glm::normalize(Vec3(-1.0f, 1.0f));
		}
	private:
		static thread_local RNG s_RandomEngine;
		static std::uniform_int_distribution<RNG::result_type> s_Distribution;
	};

}


