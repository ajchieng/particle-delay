#pragma once

#include <JuceHeader.h>

//==============================================================================
// A simple stereo circular delay line with fractional (linearly interpolated)
// reads. Writes advance a single shared write head one frame at a time; reads
// look back into the past by a delay time in milliseconds.
//
// Reading always tracks the write head at a fixed offset, so a grain that reads
// at a constant delayMs while the write head advances plays back a contiguous
// fragment of past audio and can never overrun the head.
class DelayBuffer
{
public:
    DelayBuffer() = default;

    // maxDelayMs sets the buffer length; reads are clamped to this range.
    void prepare (double sampleRate, int numChannels, float maxDelayMs);
    void reset();

    // Write one stereo frame and advance the write head by one sample.
    void writeFrame (float left, float right);

    // Read 'channel' delayMs in the past, with linear interpolation. delayMs is
    // clamped to [0, maxDelayMs].
    float readSample (int channel, float delayMs) const;

    double getSampleRate() const noexcept { return sampleRate; }

private:
    double sampleRate = 44100.0;
    int    bufferSize = 0;
    int    writeIndex = 0;
    float  maxDelayMs = 2000.0f;

    juce::AudioBuffer<float> buffer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DelayBuffer)
};
