#include "Random.h"

namespace Walnut {

	thread_local RNG Random::s_RandomEngine;
	std::uniform_int_distribution<RNG::result_type> Random::s_Distribution;

}