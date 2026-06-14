#pragma once

#include <JuceHeader.h>
#include <array>
#include <vector>

//==============================================================================
// Wet-bus "lush" finishing stage. Runs on the summed wet signal after overlap
// normalisation and before the safety limiter, then the processor mixes the
// result with the dry path. The chain, in order, is:
//
//   High-pass -> Low-pass -> Diffuse -> Width
//
// At its default settings (HP 20 Hz, LP 20 kHz, diffuse 0, width 1.0) the stage
// is transparent, so sessions that never touch the SPACE panel are unchanged.
//
// Allocation-free once prepared: the diffuser's all-pass ring buffers are sized
// for the longest delay (Size = 1) in prepare() and only indexed thereafter.
class WetFinisher
{
public:
    WetFinisher() = default;

    void prepare (double sampleRate);
    void reset();

    // Cutoffs in Hz; clamped internally. A 20 Hz high-pass / 20 kHz low-pass is
    // effectively bypassed.
    void setHighPass (float hz);
    void setLowPass (float hz);

    // amount 0..1 crossfades dry -> diffused; size 0..1 scales the all-pass
    // delay lengths (longer = a wider, slower smear).
    void setDiffuse (float amount, float size);

    // 0 = mono, 1 = unchanged, 2 = doubled stereo width (M/S side gain).
    void setWidth (float w);

    void process (float& left, float& right);

private:
    static constexpr int numAllpass = 4;

    // Topology-preserving one-pole. process() returns the low-pass output; the
    // high-pass complement is (input - low-pass).
    struct OnePoleTPT
    {
        float z = 0.0f;
        void reset() noexcept { z = 0.0f; }
        float lowpass (float x, float G) noexcept
        {
            const float v = (x - z) * G;
            const float y = v + z;
            z = y + v;
            return y;
        }
    };

    // Schroeder all-pass: H(z) = (-g + z^-D) / (1 - g z^-D). Used in series to
    // smear transients into a diffuse tail.
    struct Allpass
    {
        std::vector<float> buffer;
        int length = 1;
        int index  = 0;

        void prepareTo (int capacity)
        {
            buffer.assign ((size_t) juce::jmax (1, capacity), 0.0f);
            length = (int) buffer.size();
            index  = 0;
        }
        void reset()
        {
            std::fill (buffer.begin(), buffer.end(), 0.0f);
            index = 0;
        }
        void setLength (int n)
        {
            length = juce::jlimit (1, (int) buffer.size(), n);
            if (index >= length)
                index = 0;
        }
        float process (float x, float g) noexcept
        {
            const float vDelayed = buffer[(size_t) index];
            const float v = x + g * vDelayed;
            const float y = -g * v + vDelayed;
            buffer[(size_t) index] = v;
            if (++index >= length)
                index = 0;
            return y;
        }
    };

    double sampleRate = 44100.0;

    // Precomputed G = g/(1+g) where g = tan(pi*fc/fs), one per filter.
    float highPassG = 0.0f;
    float lowPassG  = 1.0f;
    OnePoleTPT highPass[2];
    OnePoleTPT lowPass[2];

    Allpass diffuser[2][numAllpass];
    float diffuseAmount = 0.0f;

    float width = 1.0f;

    // Base all-pass delays (ms). The right channel is stretched a little for
    // stereo decorrelation; Size scales both between minSizeScale and 1.0.
    static constexpr std::array<float, numAllpass> baseDelaysMs { 13.6f, 19.2f, 27.3f, 38.4f };
    static constexpr float rightDecorrelation = 1.18f;
    static constexpr float minSizeScale       = 0.20f;
    static constexpr float allpassG           = 0.6f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WetFinisher)
};
