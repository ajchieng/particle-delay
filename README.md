# Particle Delay

A physics-inspired delay plugin (JUCE / AU + Standalone). Every detected input
transient is captured in stereo and launches a burst of virtual **particles**.
Each floor bounce replays that particle's own captured hit:

| Particle property | Controls |
|-------------------|----------|
| x position        | stereo **pan** (0 = left, 1 = right) |
| time since hit    | whether the bounce is inside the audible **delay window** |
| energy            | echo **gain** (decays over time) |
| speed             | **brightness** (low-pass cutoff of the echo) |

Gravity can run freely, lock the first bounce to a tempo division, or offset a
tempo-locked fall in Hybrid mode. Bounce, scatter and decay shape the later
motion so the echo train accelerates and evolves instead of repeating on a
fixed grid.

## Controls

See [PARAMETER_GUIDE.md](PARAMETER_GUIDE.md) for a detailed explanation of
every control and how the parameters interact.

| Param | Range | Default | Meaning |
|-------|-------|---------|---------|
| Mix       | 0–100 %        | 35 %     | Dry/wet blend |
| Particles | 1–32           | 8        | Particles launched per transient |
| Gravity   | 0.0625x–16x    | 1.0x     | Free timing speed or Hybrid offset |
| Timing Mode | Free / Tempo / Hybrid | Free | How particle fall timing is calculated |
| Timing Division | note values | 1/4 | Note value for Tempo and Hybrid timing |
| Bounce    | 10–99 %        | 72 %     | Velocity/energy kept after a floor hit |
| Scatter   | 0–100 %        | 50 %     | Randomness of launch positions/velocities |
| Decay     | 0.90–0.9999    | 0.995    | Per-tick energy decay (how long particles live) |
| Capture Length | 80–500 ms | 250 ms | Maximum captured body of each detected hit |
| Smoothness | 0–100 % | 50 % | Replay attack/release fade length |
| Delay Min | 1–20000 ms / sync | 60 ms | Start of the audible bounce window |
| Delay Max | 1–20000 ms / sync | 6000 ms | End of the audible bounce window |
| Threshold | 0.001–1.0      | 0.15     | Input level needed to spawn particles |
| Output    | -24–+12 dB     | 0 dB     | Output gain |

The editor shows a live view of the particle box so you can watch the echoes
being scattered.

## How it works

- **TransientDetector** — a peak follower with a 50 ms cooldown reports note
  onsets so one drum hit spawns particles once, not every sample.
- **ParticleSystem** — advances particle physics at a fixed **250 Hz control
  rate** (independent of the host sample rate). The timing mode determines the
  first fall time; bounce/scatter/decay shape later motion. Floor contacts emit
  `EchoEvent`s.
- **CapturedHitBank** — adaptively records 80 ms up to the selected maximum,
  trims a decayed tail after 20 ms of silence, and keeps up to 16 hits separate.
- **Delay sync** — independent buttons for Delay Min and Delay Max convert
  musical note divisions to milliseconds using the host BPM.
- **Replay voices** — each audible bounce starts at the beginning of its own
  captured stereo hit. Adjustable fades, overlap normalization, and a linked
  -1 dB limiter keep dense clouds controlled.

The audio thread does no allocation, locking, or I/O: capture, particle, and replay stores
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

An offline DSP test harness (no DAW needed) checks hit capture, replay shaping,
limiting, transient detection, and particle/source ownership:

```sh
cmake -B build -G Xcode -DPARTICLEDELAY_BUILD_TESTS=ON
cmake --build build --config Release --target ParticleDelayTests
./build/ParticleDelayTests_artefacts/Release/ParticleDelayTests
```

## Starting points / presets

The factory preset menu includes practical starting points for common sources:

| Preset | Use |
|--------|-----|
| Subtle Scatter | Low-mix insert effect for gentle movement |
| Bouncing Ball | The core bouncing-delay sound |
| Chaos Cloud | Dense, dramatic particle scatter |
| Lush | Diffused SPACE-panel echo cloud |
| Drum Tight Slap | Short, controlled drum echo |
| Drum Room Scatter | Wider drum-loop movement |
| Snare Ghosts | Snare repeats without a huge tail |
| Vocal Doubler Cloud | Subtle vocal width and motion |
| Vocal Throw | More obvious delay throw for sends or automation |
| Piano Sparkle | Bright, light repeats for chords |
| Guitar Halo | Mid-width ambience for guitar or synth plucks |
| Dub Send | Darker, feedback-forward return effect |
| Ambient Wash | Long, diffuse, softened tail |
| Bass Safe Space | High-passed wet signal so lows stay clean |
| Wide Micro Scatter | Subtle stereo texture at low mix |

## Sound design targets

Snare → scattered rhythmic taps · drum loop → bouncing ghost echoes · piano
chord → sparkling fragments · vocal → stereo ghost repeats · synth stab →
glitchy spatial delay.
