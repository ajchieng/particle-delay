# Particle Delay

A physics-inspired delay plugin (JUCE / AU + Standalone). Instead of regular
delay repeats, every input transient launches a burst of virtual **particles**
into a 2D box. Each time a particle hits a wall it bounces and fires a short
echo "grain" read from a delay line:

| Particle property | Controls |
|-------------------|----------|
| x position        | stereo **pan** (0 = left, 1 = right) |
| time since hit    | **delay time** (each bounce replays the source transient) |
| energy            | echo **gain** (decays over time) |
| speed             | **brightness** (low-pass cutoff of the echo) |

Sync sets the first bounce, while bounce, scatter and decay shape the later
motion so the echo train accelerates and evolves instead of repeating on a
fixed grid.

## Controls

See [PARAMETER_GUIDE.md](PARAMETER_GUIDE.md) for a detailed explanation of
every control and how the parameters interact.

| Param | Range | Default | Meaning |
|-------|-------|---------|---------|
| Mix       | 0–100 %        | 35 %     | Dry/wet blend |
| Particles | 1–32           | 8        | Particles launched per transient |
| Sync      | 1/4–1/16T      | 1/8      | Host-synced time to the first bounce |
| Bounce    | 10–99 %        | 72 %     | Velocity/energy kept after a floor hit |
| Scatter   | 0–100 %        | 50 %     | Randomness of launch positions/velocities |
| Decay     | 0.90–0.9999    | 0.995    | Per-tick energy decay (how long particles live) |
| Delay Min | 1–12000 ms / sync | 60 ms | Shortest allowed source read-back time |
| Delay Max | 1–12000 ms / sync | 1200 ms | Longest allowed source read-back time |
| Threshold | 0.001–1.0      | 0.15     | Input level needed to spawn particles |
| Output    | -24–+12 dB     | 0 dB     | Output gain |

The editor shows a live view of the particle box so you can watch the echoes
being scattered.

## How it works

- **TransientDetector** — a peak follower with a 50 ms cooldown reports note
  onsets so one drum hit spawns particles once, not every sample.
- **ParticleSystem** — advances particle physics at a fixed **250 Hz control
  rate** (independent of the host sample rate). Host tempo and Sync determine
  the first fall time; bounce/scatter/decay shape later motion. Floor contacts
  emit `EchoEvent`s.
- **DelayBuffer** — a stereo circular buffer with linear-interpolated reads.
- **Delay sync** — independent buttons for Delay Min and Delay Max convert
  musical note divisions to milliseconds using the host BPM.
- **Echo grains** — each bounce becomes a ~45 ms windowed grain (Hann-ish
  envelope) so echoes sound like smooth fragments, not clicks. The wet bus is
  softly saturated (`tanh`) so dense clouds stay polite; the dry path is clean.

The audio thread does no allocation, locking, or I/O: particle and grain stores
are pre-reserved and capped, and full slots are recycled in place.

## Building

Requires CMake ≥ 3.25 and a C++17 toolchain (Xcode on macOS). JUCE 8.0.13 must
be available at `./JUCE`:

```sh
# Option A: symlink an existing JUCE checkout
ln -s ../chieng-saturation/JUCE JUCE
# Option B: fresh shallow clone
git clone --depth 1 --branch 8.0.13 https://github.com/juce-framework/JUCE.git JUCE
```

Then configure and build:

```sh
cmake -B build -G Xcode
cmake --build build --config Release
```

Artefacts land in `build/ParticleDelay_artefacts/Release/`. The AU is also
auto-copied to `~/Library/Audio/Plug-Ins/Components/` (Logic, etc.). Validate it
with:

```sh
auval -v aufx Ptd1 Chng
```

### Tests

An offline DSP test harness (no DAW needed) checks the delay line, transient
detector and particle/echo generation:

```sh
cmake -B build -G Xcode -DPARTICLEDELAY_BUILD_TESTS=ON
cmake --build build --config Release --target ParticleDelayTests
./build/ParticleDelayTests_artefacts/Release/ParticleDelayTests
```

## Starting points / presets

Dial these in by hand (saved with the host's own preset system):

**1. Subtle Scatter** — Mix 25 %, Particles 5, Sync 1/8, Bounce 65 %,
Scatter 30 %, Decay 0.990, Delay 60–700 ms.

**2. Bouncing Ball** — Mix 40 %, Particles 8, Sync 1/8, Bounce 82 %,
Scatter 45 %, Decay 0.995, Delay 40–1200 ms.

**3. Chaos Cloud** — Mix 65 %, Particles 20, Sync 1/16T, Bounce 92 %,
Scatter 90 %, Decay 0.998, Delay 20–1800 ms.

## Sound design targets

Snare → scattered rhythmic taps · drum loop → bouncing ghost echoes · piano
chord → sparkling fragments · vocal → stereo ghost repeats · synth stab →
glitchy spatial delay.
