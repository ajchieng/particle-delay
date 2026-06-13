# Particle Delay Parameter Guide

Particle Delay detects transients in the incoming audio. Each detected hit
launches particles, and every floor bounce creates an echo of that hit.

## Quick Reference

| Parameter | Range | Default | Main effect |
|---|---:|---:|---|
| Mix | 0-100% | 35% | Balance between dry input and echoes |
| Particles | 1-32 | 8 | Number of particles launched per detected hit |
| Sync | 1/4 to 1/16T | 1/8 | Time from the hit to the first bounce |
| Bounce | 10-99% | 72% | How strongly particles rebound |
| Scatter | 0-100% | 50% | Stereo spread and release-time variation |
| Decay | 0.90-0.9999 | 0.995 | How quickly particle energy fades |
| Delay Min | 1-12000 ms or synced | 60 ms | Minimum audio read-back time |
| Delay Max | 1-12000 ms or synced | 1200 ms | Maximum audio read-back time |
| Threshold | 0.001-1.0 | 0.15 | Input level required to detect a hit |
| Output | -24 to +12 dB | 0 dB | Final plugin output level |

## Mix

Controls the balance between the original input and the generated echoes.

- **0%:** Dry input only.
- **50%:** Equal dry and wet levels.
- **100%:** Echoes only.

Start around 20-40% when using the plugin as an insert. Use 100% when placing it
on a dedicated send/return channel.

## Particles

Controls how many particles are launched when Threshold detects a transient.
Each particle follows a slightly different path and can produce several echoes.

- **Low values:** Fewer, clearer repeats.
- **High values:** Denser and louder echo clouds with more stereo activity.

High values can cause many echoes to overlap. Reduce Mix or Output if the result
becomes too loud.

## Sync

Sets the musical duration between the original hit and the first bounce. It
follows the host tempo. The available divisions are:

| Setting | Meaning |
|---|---|
| 1/4 | Quarter note |
| 1/4T | Quarter-note triplet |
| 1/8D | Dotted eighth note |
| 1/8 | Eighth note |
| 1/8T | Eighth-note triplet |
| 1/16 | Sixteenth note |
| 1/16T | Sixteenth-note triplet |

- **Longer divisions:** Slower, more spacious bounce patterns.
- **Shorter divisions:** Faster, tighter repeats.

After the first synced bounce, later bounce timing is produced by the particle
physics rather than a regular step sequencer.

## Bounce

Controls how much vertical speed and energy a particle keeps after hitting the
floor.

- **Low values:** The particle quickly settles, producing a short echo train.
- **High values:** The particle rebounds higher and produces more echoes over a
  longer period.

Values near 99% can create long, busy tails. Bounce and Decay both affect the
overall length, but Bounce changes the motion and spacing while Decay mainly
changes loudness.

## Scatter

Controls how widely particles spread and how much their release times are
staggered.

- **0%:** Particles launch nearly together and stay near the center.
- **High values:** Particles fan farther left and right and enter over a wider
  time window.

The particle's horizontal position controls stereo pan, so more Scatter creates
a wider stereo image. It does not randomize the synced fall duration.

## Decay

Controls how much particle energy remains after every physics update. Particle
energy controls echo gain.

- **Lower values:** Echoes become quiet quickly.
- **Higher values:** Echoes remain audible for longer.

This control is intentionally close to `1.0`; small adjustments can make a large
difference to the tail. If the effect is too dense, lower Decay before reducing
Particles.

## Delay Min

Sets the shortest delay-line read-back allowed for an echo.

Normally, each bounce reads back by the total time since the original hit so it
replays that hit. Delay Min clamps any shorter read-back time.

- **Lower values:** Allow very short, tight echoes.
- **Higher values:** Prevent extremely short reads and can make the beginning
  of the effect less immediate.

For predictable source-hit repeats, keep Delay Min below the duration selected
by Sync.

The controls below the knob select its operating mode:

- **Sync off:** The Delay Min knob is interpreted directly in milliseconds.
- **Sync on:** The selected note division is converted to milliseconds using
  the host BPM. The millisecond knob is retained but ignored until Sync is
  turned off.

## Delay Max

Sets the longest delay-line read-back allowed for an echo.

As the bounce train continues, the required read-back time grows. Once it
exceeds Delay Max, it is clamped. At that point later bounces no longer read the
exact original hit and may read other recent material or silence.

- **Lower values:** Shorter period in which bounces replay the original hit.
- **Higher values:** More of the visible bounce train can replay the hit.

Use a higher value for long Bounce settings or slower Sync divisions.

The controls below the knob select its operating mode:

- **Sync off:** The Delay Max knob is interpreted directly in milliseconds.
- **Sync on:** The selected note division is converted to milliseconds using
  the host BPM. The millisecond knob is retained but ignored until Sync is
  turned off.

Both delay sync selectors offer `1/64T`, `1/64`, `1/32T`, `1/32`, `1/16T`,
`1/16`, `1/8T`, `1/8`, `1/4T`, `1/4`, `1/2`, `1/2D`, and `1/1`. The standalone
app uses 120 BPM when no host tempo is available.

The maximum manual delay and internal delay buffer are 12000 ms.

## Threshold

Sets how loud the incoming audio must be before it launches particles.

- **Lower values:** Detect quieter details and trigger more often.
- **Higher values:** Ignore quieter material and respond only to stronger hits.

The value is a linear amplitude, not decibels. For example, `0.15` means an
input peak of roughly 15% of full scale. Threshold does not change echo volume;
it only controls when a burst is triggered.

If the plugin triggers continuously on background sound, raise Threshold. If it
misses intended drum hits or notes, lower Threshold.

## Output

Controls the final level after the dry and wet signals are mixed.

- **Negative values:** Reduce the plugin's total output.
- **0 dB:** No additional level change.
- **Positive values:** Boost the final output.

Use Output to level-match the processed signal with the bypassed signal. This
makes it easier to judge the effect without being biased by a louder result.

## Useful Interactions

- **Particles + Scatter:** More particles and more scatter create a wider,
  denser cloud.
- **Bounce + Decay:** Both extend the tail. Bounce changes bounce height and
  timing; Decay changes how quickly repeats become quiet.
- **Main Sync + Delay Max:** Slower first-bounce Sync settings need a higher
  Delay Max to keep replaying the original hit for later bounces.
- **Delay Min/Max Sync:** These two buttons tempo-sync the clamp boundaries;
  they do not replace the main Sync control that times the first bounce.
- **Threshold + Particles:** A low Threshold with many particles can create a
  very dense result because many input details launch large bursts.
- **Mix + Output:** Use Mix for the effect balance and Output for final
  level-matching.
