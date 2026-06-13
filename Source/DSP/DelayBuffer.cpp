#include "DelayBuffer.h"

void DelayBuffer::prepare (double newSampleRate, int numChannels, float newMaxDelayMs)
{
    sampleRate = newSampleRate;
    maxDelayMs = newMaxDelayMs;

    // Length = max delay in samples, plus a couple of guard samples so the
    // interpolator's (index + 1) read never lands on the write head.
    const int maxDelaySamples = (int) std::ceil (maxDelayMs * 0.001 * sampleRate);
    bufferSize = juce::jmax (4, maxDelaySamples + 4);

    buffer.setSize (juce::jmax (2, numChannels), bufferSize, false, true, true);
    reset();
}

void DelayBuffer::reset()
{
    buffer.clear();
    writeIndex = 0;
}

void DelayBuffer::writeFrame (float left, float right)
{
    if (bufferSize <= 0)
        return;

    buffer.setSample (0, writeIndex, left);

    if (buffer.getNumChannels() > 1)
        buffer.setSample (1, writeIndex, right);

    if (++writeIndex >= bufferSize)
        writeIndex = 0;
}

float DelayBuffer::readSample (int channel, float delayMs) const
{
    if (bufferSize <= 0)
        return 0.0f;

    channel = juce::jlimit (0, buffer.getNumChannels() - 1, channel);

    const float clampedMs   = juce::jlimit (0.0f, maxDelayMs, delayMs);
    const float delaySamples = clampedMs * 0.001f * (float) sampleRate;

    // writeFrame advances the head after writing, so the most recent sample is
    // at (writeIndex - 1); a delay of 0 should read it.
    float readPosition = (float) (writeIndex - 1) - delaySamples;
    while (readPosition < 0.0f)
        readPosition += (float) bufferSize;

    const int   index1 = (int) readPosition;
    const int   index2 = (index1 + 1) % bufferSize;
    const float frac   = readPosition - (float) index1;

    const float s1 = buffer.getSample (channel, index1);
    const float s2 = buffer.getSample (channel, index2);

    return s1 + frac * (s2 - s1);
}
