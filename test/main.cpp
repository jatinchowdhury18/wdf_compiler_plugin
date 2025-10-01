#include <iostream>
#include <span>

#include "wdf.h"

#if defined(WIN32)
#define DLL_EXPORT extern "C" __declspec(dllexport)
#else
#define DLL_EXPORT extern "C"
#endif

struct Processor
{
    Params params {};
    State state {};
    float fs {};

    void process (std::span<float> data)
    {
        Impedances impedances {};
        calc_impedances (impedances, fs, params);
        for (auto& x : data)
        {
            x = ::process (state, impedances, x);
        }
    }
};


DLL_EXPORT void* create_processor()
{
    std::cout << "Creating processor..." << std::endl;
    return new Processor();
}

Processor* cast (void* processor)
{
    return static_cast<Processor*> (processor);
}

DLL_EXPORT void delete_processor (void* processor)
{
    std::cout << "Deleting processor..." << std::endl;
    delete cast (processor);
}

DLL_EXPORT void prepare (void* processor, double sample_rate, int samples_per_block)
{
    std::cout << "Preparing processor with sample rate: " << sample_rate << ", and buffer size: " << samples_per_block << std::endl;
    cast (processor)->fs = static_cast<float> (sample_rate);
}

DLL_EXPORT void reset (void* processor) noexcept
{
    cast (processor)->state = {};
}

DLL_EXPORT void process (void* processor, std::span<float> data)
{
    auto& proc = *cast (processor);
    proc.process (data);
}
