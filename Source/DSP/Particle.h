#pragma once

//==============================================================================
// A single virtual particle living inside a normalised 2D box.
//
//   x = 0 -> hard left,  x = 1 -> hard right   (governs pan)
//   y = 0 -> top,        y = 1 -> floor
//
// The box has no side walls and no ceiling: particles are dropped from the
// middle, pulled down by gravity, and bounce only on the floor (y = 1), losing
// energy each bounce until they settle. Every floor contact fires one echo.
//
// Position and velocity are unitless and advance once per *control tick* (see
// ParticleSystem / the control rate in the processor), not once per audio
// sample. The default gravity/scatter/decay values only make musical sense at
// that control rate, which is why the physics is decoupled from the audio rate.
struct Particle
{
    float x = 0.5f;
    float y = 0.5f;

    float vx = 0.0f;
    float vy = 0.0f;

    float energy = 1.0f;   // 0..1, scales the echo gain; decays over time
    float age    = 0.0f;   // control ticks since spawn

    // Control ticks since the burst/hit that spawned this particle. This clock
    // keeps growing across bounces so every echo reads back the original hit.
    float delayClockTicks = 0.0f;

    bool alive = true;
};
