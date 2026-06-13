#pragma once

#include <JuceHeader.h>
#include <array>

namespace DelaySync
{
    struct Division
    {
        const char* label;
        double quarters;
    };

    inline constexpr std::array<Division, 13> divisions {{
        { "1/64T", 1.0 / 24.0 },
        { "1/64",  1.0 / 16.0 },
        { "1/32T", 1.0 / 12.0 },
        { "1/32",  1.0 / 8.0  },
        { "1/16T", 1.0 / 6.0  },
        { "1/16",  1.0 / 4.0  },
        { "1/8T",  1.0 / 3.0  },
        { "1/8",   1.0 / 2.0  },
        { "1/4T",  2.0 / 3.0  },
        { "1/4",   1.0        },
        { "1/2",   2.0        },
        { "1/2D",  3.0        },
        { "1/1",   4.0        },
    }};

    inline juce::StringArray labels()
    {
        juce::StringArray result;
        for (const auto& division : divisions)
            result.add (division.label);
        return result;
    }

    inline float milliseconds (int index, double bpm)
    {
        const int safeIndex = juce::jlimit (0, (int) divisions.size() - 1, index);
        const double safeBpm = juce::jmax (20.0, bpm);
        return (float) (divisions[(size_t) safeIndex].quarters * 60000.0 / safeBpm);
    }
}
