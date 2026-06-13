#include "TransientDetector.h"

void TransientDetector::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;

    // ~120 ms release on the peak follower: the envelope snaps up on a hit and
    // decays back below the threshold so the next hit reads as a fresh rising edge.
    release = (float) std::exp (-1.0 / (0.120 * sampleRate));

    // 50 ms minimum spacing between spawns.
    cooldownLength = (int) (0.050 * sampleRate);

    reset();
}

void TransientDetector::reset()
{
    envelope        = 0.0f;
    previousEnv     = 0.0f;
    cooldownSamples = 0;
}

bool TransientDetector::processSample (float left, float right, float threshold)
{
    const float mono = 0.5f * (std::abs (left) + std::abs (right));

    // Peak follower: instant attack, exponential release.
    previousEnv = envelope;
    envelope    = juce::jmax (mono, envelope * release);

    if (cooldownSamples > 0)
    {
        --cooldownSamples;
        return false;
    }

    const bool crossedUp = (envelope > threshold) && (previousEnv <= threshold);

    if (crossedUp)
    {
        cooldownSamples = cooldownLength;
        return true;
    }

    return false;
}
