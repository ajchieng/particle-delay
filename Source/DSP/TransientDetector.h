#pragma once

#include <JuceHeader.h>

//==============================================================================
// Detects note/hit onsets in the input so we know when to launch particles.
//
// A fast peak follower with a slow release tracks the signal level. When that
// envelope crosses the threshold from below we report a single transient, then
// hold off for a short cooldown so one drum hit can't machine-gun the particle
// spawner every sample.
class TransientDetector
{
public:
    TransientDetector() = default;

    void prepare (double sampleRate);
    void reset();

    // Returns true exactly once at the start of each detected transient.
    bool processSample (float left, float right, float threshold);

private:
    double sampleRate     = 44100.0;
    float  envelope       = 0.0f;
    float  previousEnv    = 0.0f;
    float  release        = 0.9995f;  // recomputed in prepare()
    int    cooldownSamples = 0;
    int    cooldownLength   = 0;       // ~50 ms, recomputed in prepare()

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TransientDetector)
};
