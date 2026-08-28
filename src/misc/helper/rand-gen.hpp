#ifndef RAND_GEN_HPP
#define RAND_GEN_HPP

#include <random>
#include <stdint.h>

namespace misc
{

class RandGen
{public:
    explicit RandGen(uint32_t seed) : engine(seed) {}

    int int_range(int min, int max)
    {
        std::uniform_int_distribution<int> dist(min, max);
        return dist(engine);
    }

    float float_range(float min, float max)
    {
        std::uniform_real_distribution<float> dist(min, max);
        return dist(engine);
    }

  private:
    std::mt19937 engine;
};

}  // namespace rand

#endif