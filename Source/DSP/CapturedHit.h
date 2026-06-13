#pragma once

#include <JuceHeader.h>
#include <array>
#include <cmath>
#include <cstdint>

//==============================================================================
// Fixed-capacity stereo transient capture storage. All audio buffers are
// allocated in prepare(); starting and recording captures is allocation-free.
class CapturedHitBank
{
public:
    static constexpr int maxCaptures = 16;
    static constexpr float minimumCaptureMs = 80.0f;
    static constexpr float maximumCaptureMs = 500.0f;

    struct StartResult
    {
        uint64_t sourceId = 0;
        uint64_t retiredSourceId = 0;
    };

    void prepare (double sampleRate);
    void reset();

    StartResult startCapture (float maximumMs);
    void finishMatureCaptures();
    void processSample (float left, float right);

    bool isActive (uint64_t sourceId) const;
    bool isRecording (uint64_t sourceId) const;
    int getAvailableSamples (uint64_t sourceId) const;
    float getSample (uint64_t sourceId, int channel, int sampleIndex) const;

    int getSlotForSource (uint64_t sourceId) const;
    bool isHandleActive (int slot, uint64_t sourceId) const;
    bool isHandleRecording (int slot, uint64_t sourceId) const;
    int getHandleAvailableSamples (int slot, uint64_t sourceId) const;
    float getHandleSample (int slot, uint64_t sourceId,
                           int channel, int sampleIndex) const;
    bool getHandleFrame (int slot, uint64_t sourceId, int sampleIndex,
                         float& left, float& right,
                         int& availableSamples, bool& recording) const;

private:
    struct Capture
    {
        juce::AudioBuffer<float> audio;
        uint64_t sourceId = 0;
        uint64_t startOrder = 0;
        int writtenSamples = 0;
        int finalLength = 0;
        int maximumSamples = 0;
        int belowThresholdSamples = 0;
        float peak = 0.0f;
        bool active = false;
        bool recording = false;
    };

    Capture* find (uint64_t sourceId);
    const Capture* find (uint64_t sourceId) const;
    void finishCapture (Capture& capture, bool trimSilence);

    std::array<Capture, maxCaptures> captures;
    double currentSampleRate = 44100.0;
    int allocatedSamples = 0;
    int minimumSamples = 0;
    int silenceSamples = 0;
    uint64_t nextSourceId = 1;
    uint64_t nextStartOrder = 1;
};

//==============================================================================
// Linked-stereo peak limiter with instantaneous gain reduction and a smooth
// release back to unity.
class StereoSafetyLimiter
{
public:
    void prepare (double sampleRate);
    void reset();
    void process (float& left, float& right);

    static constexpr float ceiling = 0.8912509f; // -1 dBFS

private:
    float gain = 1.0f;
    float releaseCoeff = 0.9995f;
};

namespace CapturedHitPlayback
{
    inline float envelope (int sampleIndex,
                           int availableSamples,
                           bool recording,
                           int attackSamples,
                           int releaseSamples)
    {
        float value = juce::jmin (
            1.0f, (float) sampleIndex / (float) juce::jmax (1, attackSamples));

        if (! recording)
        {
            const int remaining = availableSamples - sampleIndex;
            if (remaining <= releaseSamples)
                value *= juce::jlimit (
                    0.0f, 1.0f,
                    (float) (remaining - 1) / (float) juce::jmax (1, releaseSamples));
        }

        return value;
    }

    inline void placeStereo (float sourceLeft,
                             float sourceRight,
                             float pan,
                             float& outputLeft,
                             float& outputRight)
    {
        pan = juce::jlimit (0.0f, 1.0f, pan);
        const float width = 1.0f - std::abs (pan * 2.0f - 1.0f);
        const float mid = 0.5f * (sourceLeft + sourceRight);
        const float side = 0.5f * (sourceLeft - sourceRight) * width;
        const float angle = pan * juce::MathConstants<float>::halfPi;
        const float centreCompensation = std::sqrt (2.0f);

        outputLeft = (mid + side) * std::cos (angle) * centreCompensation;
        outputRight = (mid - side) * std::sin (angle) * centreCompensation;
    }
}
