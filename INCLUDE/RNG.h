#pragma once

#include <random>
#include <seastar/core/smp.hh>


inline int getRandomInt(int min, int max)
{
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<> distrib(min, max - 1);
    return distrib(gen);
}

inline int getRandomIntInclusive(int min, int max)
{
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<> distrib(min, max);
    return distrib(gen);
}

inline double getRandomReal(double min, double max)
{
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_real_distribution<> distrib(min, max);
    return distrib(gen);
}

inline unsigned getRandomShardID()
{
    return getRandomInt(0, seastar::smp::count);
}