#include "WetFinisher.h"
#include <cmath>

namespace
{
    // G = g/(1+g) for a TPT one-pole at cutoff fc. Cutoff is clamped away from
    // 0 and Nyquist so tan() stays finite.
    float onePoleG (double fc, double fs)
    {
        fc = juce::jlimit (1.0, fs * 0.49, fc);
        const float g = std::tan ((float) (juce::MathConstants<double>::pi * fc / fs));
        return g / (1.0f + g);
    }
}

void WetFinisher::prepare (double newSampleRate)
{
    sampleRate = std::isfinite (newSampleRate) && newSampleRate > 0.0
        ? newSampleRate
        : 44100.0;

    // Size the all-pass ring buffers for the longest delay (Size = 1.0).
    for (int ch = 0; ch < 2; ++ch)
    {
        const float decorrel = ch == 1 ? rightDecorrelation : 1.0f;
        for (int s = 0; s < numAllpass; ++s)
        {
            const int capacity = (int) std::ceil (
                baseDelaysMs[(size_t) s] * decorrel * 0.001f * (float) sampleRate) + 2;
            diffuser[ch][s].prepareTo (capacity);
        }
    }

    // Neutral defaults; the processor pushes real values every block.
    setHighPass (20.0f);
    setLowPass (20000.0f);
    setDiffuse (0.0f, 0.5f);
    setWidth (1.0f);

    reset();
}

void WetFinisher::reset()
{
    for (int ch = 0; ch < 2; ++ch)
    {
        highPass[ch].reset();
        lowPass[ch].reset();
        for (int s = 0; s < numAllpass; ++s)
            diffuser[ch][s].reset();
    }
}

void WetFinisher::setHighPass (float hz)
{
    highPassG = onePoleG ((double) hz, sampleRate);
}

void WetFinisher::setLowPass (float hz)
{
    lowPassG = onePoleG ((double) hz, sampleRate);
}

void WetFinisher::setDiffuse (float amount, float size)
{
    diffuseAmount = juce::jlimit (0.0f, 1.0f, amount);

    const float scale = minSizeScale
        + (1.0f - minSizeScale) * juce::jlimit (0.0f, 1.0f, size);

    for (int ch = 0; ch < 2; ++ch)
    {
        const float decorrel = ch == 1 ? rightDecorrelation : 1.0f;
        for (int s = 0; s < numAllpass; ++s)
        {
            const int len = (int) std::round (
                baseDelaysMs[(size_t) s] * decorrel * scale * 0.001f * (float) sampleRate);
            diffuser[ch][s].setLength (juce::jmax (1, len));
        }
    }
}

void WetFinisher::setWidth (float w)
{
    width = juce::jlimit (0.0f, 2.0f, w);
}

void WetFinisher::process (float& left, float& right)
{
    float ch[2] = { left, right };

    for (int c = 0; c < 2; ++c)
    {
        float x = ch[c];

        // High-pass (complement of the one-pole low-pass), then low-pass.
        x -= highPass[c].lowpass (x, highPassG);
        x  = lowPass[c].lowpass (x, lowPassG);

        // Series all-pass diffuser, crossfaded against the dry filtered signal.
        if (diffuseAmount > 0.0f)
        {
            float wet = x;
            for (int s = 0; s < numAllpass; ++s)
                wet = diffuser[c][s].process (wet, allpassG);
            x += diffuseAmount * (wet - x);
        }

        ch[c] = x;
    }

    // Stereo width via mid/side.
    const float mid  = 0.5f * (ch[0] + ch[1]);
    const float side = 0.5f * (ch[0] - ch[1]) * width;
    left  = mid + side;
    right = mid - side;
}
