# Particle Delay Parameter Guide

Particle Delay detects transients in the incoming audio. Each detected hit
launches particles, and every floor bounce creates an echo of that hit.

## Quick Reference

| Parameter | Range | Default | Main effect |
|---|---:|---:|---|
| Mix | 0-100% | 35% | Balance between dry input and echoes |
| Particles | 1-32 | 8 | Number of particles launched per detected hit |
| Gravity | 0.0625x to 16x | 1.0x | Free timing speed or Hybrid offset |
| Timing Mode | Free, Tempo, Hybrid | Free | How first-impact timing is calculated |
| Timing Division | note values | 1/4 | Note value for Tempo and Hybrid timing |
| Bounce | 10-99% | 72% | How strongly particles rebound |
| Scatter | 0-100% | 50% | Stereo spread and release-time variation |
| Decay | 0.90-0.9999 | 0.995 | How quickly particle energy fades |
| Capture Length | 80-500 ms | 250 ms | Maximum captured body of each hit |
| Smoothness | 0-100% | 50% | Replay attack and release fades |
| Delay Min | 1-20000 ms or synced | 60 ms | Start of the audible bounce window |
| Delay Max | 1-20000 ms or synced | 6000 ms | End of the audible bounce window |
| Threshold | 0.001-1.0 | 0.15 | Input level required to detect a hit |
| Output | -24 to +12 dB | 0 dB | Final plugin output level |
| Feedback | 0-100% | 0% | Extends echo-train life beyond Bounce/Decay |
| High Pass | 20-2000 Hz | 20 Hz | Removes lows from the wet signal |
| Low Pass | 500-20000 Hz | 20 kHz | Rolls off highs from the wet signal |
| Diffuse | 0-100% | 0% | Smears echoes into a diffuse wash |
| Size | 0-100% | 50% | Length and spread of the diffusion |
| Width | 0-200% | 100% | Stereo width of the wet signal |

The last six parameters make up the **SPACE** panel, a wet-signal finishing
stage. See [SPACE Panel](#space-panel) below.

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

## Gravity

Controls how strongly particles accelerate toward the floor in **Free** timing
mode. In **Hybrid** timing mode it becomes an offset around the selected tempo
division. At `1.0x` in Free mode, a particle reaches its first impact in
approximately 250 ms from the default spawn height.

| Setting | Feel and approximate first impact |
|---|---|
| 0.0625x | Very floaty, about 1 second |
| 0.25x | Slow, about 500 ms |
| 1.0x | Default, about 250 ms |
| 4.0x | Heavy, about 125 ms |
| 16x | Very fast, about 63 ms |

- **Lower values:** Slower falls and wider bounce spacing.
- **Higher values:** Faster falls and tighter, heavier motion.

Impact time changes with the inverse square root of the multiplier. Every
impact is produced by the particle physics rather than a regular step sequencer.

## Timing Mode

Sets how the first particle impact is timed:

- **Free:** Existing behavior. Gravity sets the fall speed directly, independent
  of host tempo.
- **Tempo:** The selected Timing Division sets the first-impact time from the
  host BPM. Gravity is ignored for the fall speed.
- **Hybrid:** The selected Timing Division sets the center timing, then Gravity
  pushes the fall looser or tighter around that synced value.

Existing sessions and presets default to Free, so their sound is unchanged until
you select Tempo or Hybrid. The standalone app uses 120 BPM when no host tempo
is available.

## Timing Division

Selects the note value used by Tempo and Hybrid timing modes. It offers `1/64T`,
`1/64`, `1/32T`, `1/32`, `1/16T`, `1/16`, `1/8T`, `1/8`, `1/4T`, `1/4`, `1/2`,
`1/2D`, and `1/1`.

Delay Min and Delay Max still have their own independent sync controls. Timing
Division only changes particle fall timing; it does not change the audible
delay window by itself.

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

## Capture Length

Sets the maximum amount of stereo audio stored after each detected transient.

- Every capture records for at least 80 ms.
- After 80 ms, recording ends when the signal remains below 2% of the captured
  peak for 20 ms.
- Sustained material records until it reaches the selected 80-500 ms maximum.

Short settings produce tighter, more percussive repeats. Longer settings retain
more body from piano notes, vocals, and sustained hits. Up to 16 captured hits
can coexist; a seventeenth hit recycles the oldest capture and its particle
train.

## Smoothness

Controls the attack and release fades applied to every captured-hit replay.

- **0%:** Approximately 0.25 ms attack and 3 ms release.
- **50%:** Balanced transient preservation and click protection.
- **100%:** Approximately 5 ms attack and 40 ms release.

Lower values preserve sharper attacks. Higher values soften replay edges and
can make busy material sound more blended.

## Delay Min

Sets the beginning of the audible bounce-time window. Bounces before this time
remain visible but do not produce audio.

- **Lower values:** Allow earlier bounces to replay the captured hit.
- **Higher values:** Skip the early bounces and begin later in the train.

For predictable source-hit repeats, keep Delay Min below the first-impact time
set by Gravity.

The controls below the knob select its operating mode:

- **Sync off:** The Delay Min knob is interpreted directly in milliseconds.
- **Sync on:** The selected note division is converted to milliseconds using
  the host BPM. The millisecond knob is retained but ignored until Sync is
  turned off.

## Delay Max

Sets the end of the audible bounce-time window. Later particles continue moving
visually, but bounces after this time are silent.

- **Lower values:** Shorter audible echo trains.
- **Higher values:** More of the visible bounce train replays the captured hit.

Use a higher value for long Bounce settings or lower Gravity values.

The controls below the knob select its operating mode:

- **Sync off:** The Delay Max knob is interpreted directly in milliseconds.
- **Sync on:** The selected note division is converted to milliseconds using
  the host BPM. The millisecond knob is retained but ignored until Sync is
  turned off.

Both delay sync selectors offer `1/64T`, `1/64`, `1/32T`, `1/32`, `1/16T`,
`1/16`, `1/8T`, `1/8`, `1/4T`, `1/4`, `1/2`, `1/2D`, and `1/1`. The standalone
app uses 120 BPM when no host tempo is available.

If Delay Min is greater than Delay Max, the plugin automatically treats the
lower value as the window start and the higher value as the window end.

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

## SPACE Panel

The SPACE panel finishes the combined wet (echo) signal before it is mixed back
with the dry input. Feedback shapes the particle motion itself; the other five
controls form a wet-bus tone, diffusion, and width stage, applied in the order
High Pass, Low Pass, Diffuse, then Width. Every SPACE control defaults to a
neutral/off value, so the panel has no effect until you reach for it and
existing sessions sound identical.

## Feedback

Extends the life of each echo train. Unlike Bounce and Decay, Feedback reaches
beyond what those knobs can do on their own:

- It carries bounce energy past Bounce's 99% ceiling toward, but never reaching,
  perfectly elastic, so a particle keeps bouncing at a near-constant, musical
  interval instead of accelerating to rest.
- It slows energy fade past Decay's 0.9999 ceiling.
- It stretches the internal lifetime cap so a sustained train can ring out up to
  roughly 20 seconds at maximum.

- **0%:** No change; particle physics behave exactly as set by Bounce and Decay.
- **Higher values:** Long, sustaining tails that outlast any Bounce and Decay
  combination.

Feedback never spawns new particles, and raising it can only lengthen the tail,
never shorten it. The lifetime and energy limits keep it from self-oscillating,
so the tail always eventually fades to silence.

## High Pass

A high-pass filter applied to the wet signal only; the dry input is untouched.

- **20 Hz (default):** Effectively off.
- **Higher values:** Thin out the low end so echoes do not clutter the low
  frequencies. Useful on bass-heavy material or dense feedback tails.

## Low Pass

A low-pass filter applied to the wet signal only.

- **20 kHz (default):** Effectively off.
- **Lower values:** Darken the echoes for a warmer, dub-style or ambient tail.

Per-hit brightness already varies with impact speed; Low Pass sets a global
ceiling on top of that.

## Diffuse

Crossfades the wet signal through a series of all-pass filters that smear sharp
echoes into a softer, more reverberant wash.

- **0% (default):** Echoes pass through cleanly as discrete repeats.
- **Higher values:** Echoes blur together into a diffuse cloud. Pairs well with
  high Feedback and dense particle clouds.

## Size

Scales the diffusion delay lengths, and therefore the character of the smear.
Size has no effect while Diffuse is at 0%.

- **Lower values:** Shorter, tighter, more metallic diffusion.
- **Higher values:** Longer, more spacious smear.

## Width

Adjusts the stereo width of the wet signal using mid/side processing, applied
after diffusion.

- **0%:** The wet signal collapses to mono.
- **100% (default):** Unchanged stereo image.
- **200%:** Exaggerated stereo width.

Width affects only the echoes; the dry signal keeps its original image.

## Useful Interactions

- **Particles + Scatter:** More particles and more scatter create a wider,
  denser cloud.
- **Bounce + Decay:** Both extend the tail. Bounce changes bounce height and
  timing; Decay changes how quickly repeats become quiet.
- **Capture Length + Smoothness:** Longer captures preserve more body; higher
  Smoothness blends their replay boundaries.
- **Gravity + Delay Max:** Delay Max must extend past the first impact for that
  bounce to be audible.
- **Delay Min/Max Sync:** These two buttons tempo-sync the clamp boundaries;
  Gravity remains free-running and independent of host tempo.
- **Threshold + Particles:** A low Threshold with many particles can create a
  very dense result because many input details launch large bursts.
- **Mix + Output:** Use Mix for the effect balance and Output for final
  level-matching.
- **Feedback vs Bounce/Decay:** Use Bounce and Decay for the core motion and
  loudness of the train; reach for Feedback only when you want it to ring out
  longer than those knobs allow on their own.
- **Feedback + High Pass/Low Pass + Diffuse:** Long feedback tails can build up
  quickly. Roll off High Pass and Low Pass and add Diffuse to keep dense,
  sustaining tails smooth and controlled.
- **Width + Scatter:** Scatter spreads where echoes are placed; Width then
  narrows or widens that whole image while the dry signal stays centered.
