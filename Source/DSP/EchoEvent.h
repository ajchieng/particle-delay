#pragma once

//==============================================================================
// Emitted by the ParticleSystem every time a particle bounces on the floor. It
// is a pure description of "an echo should happen now with these properties" -
// the processor turns each event into a short EchoGrain that actually reads the
// delay buffer (see PluginProcessor).
//
// Mapping from the particle that produced it:
//   x position   -> pan          (0 = hard left, 1 = hard right)
//   time since hit -> delayMs    (every bounce replays the source; clamped to MIN..MAX)
//   energy       -> gain
//   impact speed -> brightness   (harder slam = brighter)
struct EchoEvent
{
    float delayMs    = 100.0f;
    float gain       = 1.0f;
    float pan        = 0.5f;
    float brightness = 1.0f;   // 0 = dark, 1 = bright
};
