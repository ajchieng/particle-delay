#pragma once

#include <cstdint>

//==============================================================================
// Emitted by the ParticleSystem every time a particle bounces on the floor. It
// is a pure description of "an echo should happen now with these properties" -
// the processor turns each event into a replay voice for the captured hit.
//
// Mapping from the particle that produced it:
//   x position   -> pan          (0 = hard left, 1 = hard right)
//   source ID      -> captured stereo hit
//   time since hit -> elapsedMs  (used by the audible bounce window)
//   energy       -> gain
//   impact speed -> brightness   (harder slam = brighter)
struct EchoEvent
{
    uint64_t sourceId = 0;
    float elapsedMs  = 100.0f;
    float gain       = 1.0f;
    float pan        = 0.5f;
    float brightness = 1.0f;   // 0 = dark, 1 = bright
};
