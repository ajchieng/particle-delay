#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace
{
    // Keep the original Sync choices and default intact so old sessions and
    // host automation mappings remain loadable. The parameter is no longer read.
    const std::array<const char*, 7> kLegacySyncChoices {{
        "1/4", "1/4T", "1/8D", "1/8", "1/8T", "1/16", "1/16T"
    }};
    constexpr int kDefaultSyncIndex = 3;

    float finiteOrZero (float value)
    {
        return std::isfinite (value) ? value : 0.0f;
    }
}

//==============================================================================
ParticleDelayAudioProcessor::ParticleDelayAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

ParticleDelayAudioProcessor::~ParticleDelayAudioProcessor() = default;

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
ParticleDelayAudioProcessor::createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto pct = [] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; };
    auto ms  = [] (float v, int) { return String (v, 0) + " ms"; };
    auto hz  = [] (float v, int)
    {
        return v >= 1000.0f ? String (v / 1000.0f, 1) + " kHz"
                            : String (roundToInt (v)) + " Hz";
    };
    auto dec2 = [] (float v, int) { return String (v, 2); };

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "MIX", 1 }, "Mix",
        NormalisableRange<float> (0.0f, 1.0f), 0.35f,
        AudioParameterFloatAttributes().withStringFromValueFunction (pct)));

    layout.add (std::make_unique<AudioParameterInt> (
        ParameterID { "PARTICLES", 1 }, "Particles", 1, 32, 8));

    {
        StringArray syncChoices;
        for (const auto* choice : kLegacySyncChoices)
            syncChoices.add (choice);

        layout.add (std::make_unique<AudioParameterChoice> (
            ParameterID { "SYNC", 1 }, "Sync", syncChoices, kDefaultSyncIndex));
    }

    {
        NormalisableRange<float> r (ParticleSystem::minimumGravityMultiplier,
                                    ParticleSystem::maximumGravityMultiplier);
        r.setSkewForCentre (ParticleSystem::defaultGravityMultiplier);
        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { "GRAVITY", 1 }, "Gravity", r,
            ParticleSystem::defaultGravityMultiplier,
            AudioParameterFloatAttributes()
                .withLabel ("x")
                .withStringFromValueFunction (
                    [] (float value, int) { return String (value, 2) + "x"; })));
    }

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "BOUNCE", 1 }, "Bounce",
        NormalisableRange<float> (0.1f, 0.99f), 0.72f,
        AudioParameterFloatAttributes().withStringFromValueFunction (pct)));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "SCATTER", 1 }, "Scatter",
        NormalisableRange<float> (0.0f, 1.0f), 0.5f,
        AudioParameterFloatAttributes().withStringFromValueFunction (pct)));

    {
        NormalisableRange<float> r (0.90f, 0.9999f);
        r.setSkewForCentre (0.995f);    // useful resolution close to 1.0
        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { "DECAY", 1 }, "Decay", r, 0.995f,
            AudioParameterFloatAttributes().withStringFromValueFunction (
                [] (float v, int) { return String (v, 3); }))); // 0.90-0.9999: 2dp is too coarse
    }

    {
        NormalisableRange<float> r (CapturedHitBank::minimumCaptureMs,
                                    CapturedHitBank::maximumCaptureMs);
        r.setSkewForCentre (250.0f);
        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { "CAPTURE_MAX_MS", 1 }, "Capture Length", r, 250.0f,
            AudioParameterFloatAttributes().withStringFromValueFunction (ms)));
    }

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "SMOOTHNESS", 1 }, "Smoothness",
        NormalisableRange<float> (0.0f, 1.0f), 0.5f,
        AudioParameterFloatAttributes().withStringFromValueFunction (pct)));

    {
        NormalisableRange<float> r (1.0f, maxDelayMs);
        r.setSkewForCentre (120.0f);
        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { "DELAY_MIN_MS", 1 }, "Delay Min", r, 60.0f,
            AudioParameterFloatAttributes().withStringFromValueFunction (ms)));
    }

    {
        NormalisableRange<float> r (1.0f, maxDelayMs);
        r.setSkewForCentre (3000.0f);
        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { "DELAY_MAX_MS", 1 }, "Delay Max", r, 6000.0f,
            AudioParameterFloatAttributes().withStringFromValueFunction (ms)));
    }

    layout.add (std::make_unique<AudioParameterBool> (
        ParameterID { "DELAY_MIN_SYNC", 1 }, "Delay Min Sync", false));
    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { "DELAY_MIN_DIV", 1 }, "Delay Min Division",
        DelaySync::labels(), 3)); // 1/32

    layout.add (std::make_unique<AudioParameterBool> (
        ParameterID { "DELAY_MAX_SYNC", 1 }, "Delay Max Sync", false));
    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { "DELAY_MAX_DIV", 1 }, "Delay Max Division",
        DelaySync::labels(), 10)); // 1/2

    {
        NormalisableRange<float> r (0.001f, 1.0f);
        r.setSkewForCentre (0.1f);
        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { "THRESHOLD", 1 }, "Threshold", r, 0.15f,
            AudioParameterFloatAttributes().withStringFromValueFunction (dec2)));
    }

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "OUTPUT", 1 }, "Output",
        NormalisableRange<float> (-24.0f, 12.0f), 0.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction (
            [] (float v, int) { return String (v, 1) + " dB"; })));

    //==========================================================================
    // SPACE: the wet-bus "lush" finishing stage. All default to neutral so old
    // sessions and the documented presets are unaffected until a user dials them.
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "FEEDBACK", 1 }, "Feedback",
        NormalisableRange<float> (0.0f, 1.0f), 0.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction (pct)));

    {
        NormalisableRange<float> r (20.0f, 2000.0f);
        r.setSkewForCentre (200.0f);
        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { "WET_HP", 1 }, "High Pass", r, 20.0f,
            AudioParameterFloatAttributes().withStringFromValueFunction (hz)));
    }

    {
        NormalisableRange<float> r (500.0f, 20000.0f);
        r.setSkewForCentre (3000.0f);
        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { "WET_LP", 1 }, "Low Pass", r, 20000.0f,
            AudioParameterFloatAttributes().withStringFromValueFunction (hz)));
    }

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "DIFFUSE", 1 }, "Diffuse",
        NormalisableRange<float> (0.0f, 1.0f), 0.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction (pct)));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "DIFFUSE_SIZE", 1 }, "Size",
        NormalisableRange<float> (0.0f, 1.0f), 0.5f,
        AudioParameterFloatAttributes().withStringFromValueFunction (pct)));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "WET_WIDTH", 1 }, "Width",
        NormalisableRange<float> (0.0f, 2.0f), 1.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction (
            [] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; })));

    layout.add (std::make_unique<AudioParameterBool> (
        ParameterID { "BYPASS", 1 }, "Bypass", false));

    return layout;
}

//==============================================================================
void ParticleDelayAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);

    currentSampleRate = std::isfinite (sampleRate) && sampleRate > 0.0
        ? sampleRate
        : 44100.0;

    capturedHits.prepare (currentSampleRate);
    wetLimiter.prepare (currentSampleRate);
    wetFinisher.prepare (currentSampleRate);
    transientDetector.prepare (currentSampleRate);
    particleSystem.prepare (currentSampleRate, controlRateHz);

    samplesPerControlTick = juce::jmax (1.0, currentSampleRate / controlRateHz);
    controlAccumulator    = 0.0;

    echoEvents.clear();
    echoEvents.reserve (ParticleSystem::maxParticles * 2 + 16);
    activeVoiceCount = 0;

    mixSmoothed.reset (currentSampleRate, 0.02);
    outputSmoothed.reset (currentSampleRate, 0.02);
    mixSmoothed.setCurrentAndTargetValue (apvts.getRawParameterValue ("MIX")->load());
    outputSmoothed.setCurrentAndTargetValue (
        juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("OUTPUT")->load()));

    overlapGainSmoothed.reset (currentSampleRate, 0.015);
    overlapGainSmoothed.setCurrentAndTargetValue (1.0f);

    inputLevelEnv = 0.0f;
    inputLevel.store (0.0f, std::memory_order_relaxed);
}

void ParticleDelayAudioProcessor::releaseResources()
{
    capturedHits.reset();
    wetLimiter.reset();
    wetFinisher.reset();
    transientDetector.reset();
    particleSystem.reset();
    activeVoiceCount = 0;
    echoEvents.clear();
}

bool ParticleDelayAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& mainOut = layouts.getMainOutputChannelSet();
    const auto& mainIn  = layouts.getMainInputChannelSet();

    if (mainOut != juce::AudioChannelSet::mono()
        && mainOut != juce::AudioChannelSet::stereo())
        return false;

    return mainIn == mainOut;
}

//==============================================================================
void ParticleDelayAudioProcessor::triggerReplay (const EchoEvent& e, float smoothness)
{
    if (! capturedHits.isActive (e.sourceId))
        return;

    ReplayVoice voice;
    voice.sourceId = e.sourceId;
    voice.captureSlot = capturedHits.getSlotForSource (e.sourceId);
    if (voice.captureSlot < 0)
        return;
    voice.gain = e.gain;
    voice.pan = juce::jlimit (0.0f, 1.0f, e.pan);

    smoothness = juce::jlimit (0.0f, 1.0f, smoothness);
    const float attackMs = juce::jmap (smoothness, 0.25f, 5.0f);
    const float releaseMs = juce::jmap (smoothness, 3.0f, 40.0f);
    voice.attackSamples = juce::jmax (
        1, (int) std::round (attackMs * 0.001 * currentSampleRate));
    voice.releaseSamples = juce::jmax (
        1, (int) std::round (releaseMs * 0.001 * currentSampleRate));

    // Brightness -> one-pole low-pass cutoff (exponential 600 Hz .. 16 kHz).
    const float fc = 600.0f * std::pow (16000.0f / 600.0f, juce::jlimit (0.0f, 1.0f, e.brightness));
    const float fcClamped = juce::jmin (fc, (float) currentSampleRate * 0.45f);
    voice.lpCoeff = juce::jlimit (
        0.0f, 1.0f,
        1.0f - std::exp (-juce::MathConstants<float>::twoPi
                         * fcClamped / (float) currentSampleRate));

    if (activeVoiceCount < maxReplayVoices)
    {
        activeVoices[(size_t) activeVoiceCount++] = voice;
        return;
    }

    int quietest = 0;
    for (int i = 1; i < activeVoiceCount; ++i)
        if (activeVoices[(size_t) i].lastLevel
            < activeVoices[(size_t) quietest].lastLevel)
            quietest = i;
    activeVoices[(size_t) quietest] = voice;
}

void ParticleDelayAudioProcessor::removeReplayVoice (int index)
{
    jassert (index >= 0 && index < activeVoiceCount);
    --activeVoiceCount;
    if (index != activeVoiceCount)
        activeVoices[(size_t) index] = activeVoices[(size_t) activeVoiceCount];
}

void ParticleDelayAudioProcessor::retireSource (uint64_t sourceId)
{
    if (sourceId == 0)
        return;

    particleSystem.retireSource (sourceId);
    for (int i = activeVoiceCount - 1; i >= 0; --i)
        if (activeVoices[(size_t) i].sourceId == sourceId)
            removeReplayVoice (i);
}

void ParticleDelayAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                                juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    if (apvts.getRawParameterValue ("BYPASS")->load() >= 0.5f)
    {
        const float meterRelease = std::exp (-1.0f / (0.3f * (float) currentSampleRate));
        for (int sample = 0; sample < numSamples; ++sample)
        {
            const float inL = finiteOrZero (buffer.getSample (0, sample));
            const float inR = numChannels > 1 ? finiteOrZero (buffer.getSample (1, sample)) : inL;
            const float inPeak = juce::jmax (std::abs (inL), std::abs (inR));
            inputLevelEnv = inPeak > inputLevelEnv ? inPeak : inputLevelEnv * meterRelease;
        }

        inputLevel.store (inputLevelEnv, std::memory_order_relaxed);
        return;
    }

    // Read parameters once per block (smoothed ones are ramped per sample).
    const float particleCount = apvts.getRawParameterValue ("PARTICLES")->load();
    const float bounce        = apvts.getRawParameterValue ("BOUNCE")->load();
    const float scatter       = apvts.getRawParameterValue ("SCATTER")->load();
    const float decay         = apvts.getRawParameterValue ("DECAY")->load();
    const float gravityAmount = apvts.getRawParameterValue ("GRAVITY")->load();
    const float captureMaxMs  = apvts.getRawParameterValue ("CAPTURE_MAX_MS")->load();
    const float smoothness    = apvts.getRawParameterValue ("SMOOTHNESS")->load();
    const float threshold     = apvts.getRawParameterValue ("THRESHOLD")->load();
    const float feedback      = apvts.getRawParameterValue ("FEEDBACK")->load();

    // SPACE wet-finishing stage (set once per block; transparent at defaults).
    wetFinisher.setHighPass (apvts.getRawParameterValue ("WET_HP")->load());
    wetFinisher.setLowPass  (apvts.getRawParameterValue ("WET_LP")->load());
    wetFinisher.setDiffuse  (apvts.getRawParameterValue ("DIFFUSE")->load(),
                             apvts.getRawParameterValue ("DIFFUSE_SIZE")->load());
    wetFinisher.setWidth    (apvts.getRawParameterValue ("WET_WIDTH")->load());

    mixSmoothed.setTargetValue (apvts.getRawParameterValue ("MIX")->load());
    outputSmoothed.setTargetValue (
        juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("OUTPUT")->load()));

    // Host tempo is used only by the optional Delay Min/Max window sync.
    double bpm = 120.0;   // fallback when the host gives no tempo (e.g. standalone)
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto hostBpm = pos->getBpm())
                if (std::isfinite (*hostBpm))
                    bpm = *hostBpm;
    bpm = juce::jlimit (20.0, 999.0, bpm);

    const bool delayMinSync = apvts.getRawParameterValue ("DELAY_MIN_SYNC")->load() >= 0.5f;
    const bool delayMaxSync = apvts.getRawParameterValue ("DELAY_MAX_SYNC")->load() >= 0.5f;

    const float delayMinMs = delayMinSync
        ? DelaySync::milliseconds ((int) apvts.getRawParameterValue ("DELAY_MIN_DIV")->load(), bpm)
        : apvts.getRawParameterValue ("DELAY_MIN_MS")->load();
    const float delayMaxMs = delayMaxSync
        ? DelaySync::milliseconds ((int) apvts.getRawParameterValue ("DELAY_MAX_DIV")->load(), bpm)
        : apvts.getRawParameterValue ("DELAY_MAX_MS")->load();

    const float gravity = ParticleSystem::gravityForMultiplier (
        gravityAmount, controlRateHz, ParticleSystem::spawnHeight);

    // Per-sample decay for the input meter (~300 ms release).
    const float meterRelease = std::exp (-1.0f / (0.3f * (float) currentSampleRate));

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const float inL = finiteOrZero (buffer.getSample (0, sample));
        const float inR = numChannels > 1
            ? finiteOrZero (buffer.getSample (1, sample))
            : inL;

        // Peak follower for the editor's threshold meter.
        const float inPeak = juce::jmax (std::abs (inL), std::abs (inR));
        inputLevelEnv = inPeak > inputLevelEnv ? inPeak : inputLevelEnv * meterRelease;

        // 1) Onset detection -> start a stereo capture and its particle burst.
        if (transientDetector.processSample (inL, inR, threshold))
        {
            // End mature older hits before the new onset sample is written, so
            // dense rhythmic material does not merge adjacent transients.
            capturedHits.finishMatureCaptures();
            const auto started = capturedHits.startCapture (captureMaxMs);
            retireSource (started.retiredSourceId);
            particleSystem.triggerBurst ((int) particleCount, scatter, started.sourceId);
        }

        // Every active recording capture receives this frame. A capture created
        // above therefore includes the onset sample itself.
        capturedHits.processSample (inL, inR);

        // 2) Advance physics; audible-window bounces become replay voices.
        controlAccumulator += 1.0;
        while (controlAccumulator >= samplesPerControlTick)
        {
            controlAccumulator -= samplesPerControlTick;

            echoEvents.clear();
            particleSystem.update (gravity, bounce, decay, feedback, delayMinMs, delayMaxMs, echoEvents);

            for (const auto& e : echoEvents)
                triggerReplay (e, smoothness);
        }

        // 3) Render every active captured-hit replay into the wet bus.
        float wetL = 0.0f;
        float wetR = 0.0f;

        int renderedVoiceCount = 0;
        int voiceIndex = 0;
        while (voiceIndex < activeVoiceCount)
        {
            auto& voice = activeVoices[(size_t) voiceIndex];
            float sourceL = 0.0f;
            float sourceR = 0.0f;
            int available = 0;
            bool recording = false;
            if (! capturedHits.getHandleFrame (
                    voice.captureSlot, voice.sourceId, voice.currentSample,
                    sourceL, sourceR, available, recording))
            {
                removeReplayVoice (voiceIndex);
                continue;
            }

            if (voice.currentSample >= available)
            {
                if (! recording)
                {
                    removeReplayVoice (voiceIndex);
                    continue;
                }
                ++voiceIndex;
                continue;
            }

            const float envelope = CapturedHitPlayback::envelope (
                voice.currentSample, available, recording,
                voice.attackSamples, voice.releaseSamples);

            float pannedL = 0.0f;
            float pannedR = 0.0f;
            CapturedHitPlayback::placeStereo (
                sourceL, sourceR, voice.pan, pannedL, pannedR);

            voice.lpStateLeft += voice.lpCoeff * (pannedL - voice.lpStateLeft);
            voice.lpStateRight += voice.lpCoeff * (pannedR - voice.lpStateRight);

            const float voiceGain = voice.gain * envelope;
            const float renderedL = voice.lpStateLeft * voiceGain;
            const float renderedR = voice.lpStateRight * voiceGain;
            wetL += renderedL;
            wetR += renderedR;
            voice.lastLevel = juce::jmax (std::abs (renderedL), std::abs (renderedR));
            ++voice.currentSample;
            ++renderedVoiceCount;
            ++voiceIndex;
        }

        // Slew the overlap-normalisation gain so the wet level doesn't lurch as
        // voices start and end (which happens constantly in dense clouds).
        overlapGainSmoothed.setTargetValue (
            1.0f / std::sqrt ((float) juce::jmax (1, renderedVoiceCount)));
        const float overlapGain = overlapGainSmoothed.getNextValue();
        wetL *= overlapGain;
        wetR *= overlapGain;

        // SPACE finishing (HP -> LP -> diffuse -> width) before the safety
        // limiter, so the -1 dB ceiling still bounds the final wet signal.
        wetFinisher.process (wetL, wetR);
        wetLimiter.process (wetL, wetR);

        const float mix     = mixSmoothed.getNextValue();
        const float outGain = outputSmoothed.getNextValue();

        if (numChannels > 1)
        {
            const float outL = (inL * (1.0f - mix) + wetL * mix) * outGain;
            const float outR = (inR * (1.0f - mix) + wetR * mix) * outGain;
            buffer.setSample (0, sample, finiteOrZero (outL));
            buffer.setSample (1, sample, finiteOrZero (outR));
        }
        else
        {
            const float wetMono = 0.5f * (wetL + wetR);
            const float output = (inL * (1.0f - mix) + wetMono * mix) * outGain;
            buffer.setSample (0, sample, finiteOrZero (output));
        }
    }

    inputLevel.store (inputLevelEnv, std::memory_order_relaxed);
}

//==============================================================================
juce::AudioProcessorEditor* ParticleDelayAudioProcessor::createEditor()
{
    return new ParticleDelayAudioProcessorEditor (*this);
}

//==============================================================================
void ParticleDelayAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void ParticleDelayAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto state = juce::ValueTree::fromXml (*xml);
            PresetManager::migrateState (state);
            apvts.replaceState (state);
        }
}

void ParticleDelayAudioProcessor::resetToDefaults()
{
    // Notifies the host and any attached editor controls, so this works whether
    // called from the UI button or a test. Covers every parameter type.
    for (auto* param : getParameters())
        param->setValueNotifyingHost (param->getDefaultValue());
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ParticleDelayAudioProcessor();
}
