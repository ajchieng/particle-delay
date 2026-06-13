#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace
{
    // Tempo-sync divisions for the first bounce, in quarter-note units. The
    // choice parameter's options and the processBlock lookup share this table.
    struct SyncDivision { const char* label; double quarters; };
    const std::array<SyncDivision, 7> kSyncDivisions {{
        { "1/4",   1.0          },
        { "1/4T",  2.0 / 3.0    },
        { "1/8D",  0.75         },
        { "1/8",   0.5          },
        { "1/8T",  1.0 / 3.0    },
        { "1/16",  0.25         },
        { "1/16T", 1.0 / 6.0    },
    }};
    constexpr int kDefaultSyncIndex = 3;   // 1/8
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

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "MIX", 1 }, "Mix",
        NormalisableRange<float> (0.0f, 1.0f), 0.35f,
        AudioParameterFloatAttributes().withStringFromValueFunction (pct)));

    layout.add (std::make_unique<AudioParameterInt> (
        ParameterID { "PARTICLES", 1 }, "Particles", 1, 32, 8));

    {
        // Tempo-sync division: the first bounce lands exactly on this note value
        // (host BPM drives the fall speed). Replaces the old free Gravity control.
        StringArray syncChoices;
        for (const auto& d : kSyncDivisions)
            syncChoices.add (d.label);

        layout.add (std::make_unique<AudioParameterChoice> (
            ParameterID { "SYNC", 1 }, "Sync", syncChoices, kDefaultSyncIndex));
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
            ParameterID { "DECAY", 1 }, "Decay", r, 0.995f));
    }

    {
        NormalisableRange<float> r (1.0f, maxDelayMs);
        r.setSkewForCentre (120.0f);
        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { "DELAY_MIN_MS", 1 }, "Delay Min", r, 60.0f,
            AudioParameterFloatAttributes().withStringFromValueFunction (ms)));
    }

    {
        NormalisableRange<float> r (1.0f, maxDelayMs);
        r.setSkewForCentre (600.0f);
        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { "DELAY_MAX_MS", 1 }, "Delay Max", r, 1200.0f,
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
            ParameterID { "THRESHOLD", 1 }, "Threshold", r, 0.15f));
    }

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "OUTPUT", 1 }, "Output",
        NormalisableRange<float> (-24.0f, 12.0f), 0.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction (
            [] (float v, int) { return String (v, 1) + " dB"; })));

    return layout;
}

//==============================================================================
void ParticleDelayAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);

    currentSampleRate = sampleRate;

    delayBuffer.prepare (sampleRate, 2, maxDelayMs);
    transientDetector.prepare (sampleRate);
    particleSystem.prepare (sampleRate, controlRateHz);

    samplesPerControlTick = sampleRate / controlRateHz;
    controlAccumulator    = 0.0;

    grainLengthSamples = juce::jmax (1, (int) std::round (grainMs * 0.001 * sampleRate));

    echoEvents.clear();
    echoEvents.reserve (ParticleSystem::maxParticles * 2 + 16);
    activeGrains.clear();
    activeGrains.reserve (maxGrains + 16);

    mixSmoothed.reset (sampleRate, 0.02);
    outputSmoothed.reset (sampleRate, 0.02);
    mixSmoothed.setCurrentAndTargetValue (apvts.getRawParameterValue ("MIX")->load());
    outputSmoothed.setCurrentAndTargetValue (
        juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("OUTPUT")->load()));
}

void ParticleDelayAudioProcessor::releaseResources()
{
    delayBuffer.reset();
    transientDetector.reset();
    particleSystem.reset();
    activeGrains.clear();
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
void ParticleDelayAudioProcessor::triggerGrain (const EchoEvent& e)
{
    if ((int) activeGrains.size() >= maxGrains)
        return;   // drop rather than allocate / overload

    EchoGrain g;
    g.delayMs = e.delayMs;
    g.gain    = e.gain;

    const float pan = juce::jlimit (0.0f, 1.0f, e.pan);
    g.leftGain  = std::cos (pan * juce::MathConstants<float>::halfPi);
    g.rightGain = std::sin (pan * juce::MathConstants<float>::halfPi);

    g.currentSample = 0;
    g.totalSamples  = grainLengthSamples;

    // Brightness -> one-pole low-pass cutoff (exponential 600 Hz .. 16 kHz).
    const float fc = 600.0f * std::pow (16000.0f / 600.0f, juce::jlimit (0.0f, 1.0f, e.brightness));
    const float fcClamped = juce::jmin (fc, (float) currentSampleRate * 0.45f);
    g.lpCoeff = juce::jlimit (0.0f, 1.0f,
                              1.0f - std::exp (-juce::MathConstants<float>::twoPi
                                               * fcClamped / (float) currentSampleRate));
    g.lpState = 0.0f;
    g.alive   = true;

    activeGrains.push_back (g);
}

void ParticleDelayAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                                juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    // Read parameters once per block (smoothed ones are ramped per sample).
    const float particleCount = apvts.getRawParameterValue ("PARTICLES")->load();
    const float bounce        = apvts.getRawParameterValue ("BOUNCE")->load();
    const float scatter       = apvts.getRawParameterValue ("SCATTER")->load();
    const float decay         = apvts.getRawParameterValue ("DECAY")->load();
    const float threshold     = apvts.getRawParameterValue ("THRESHOLD")->load();

    mixSmoothed.setTargetValue (apvts.getRawParameterValue ("MIX")->load());
    outputSmoothed.setTargetValue (
        juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("OUTPUT")->load()));

    // Tempo sync: drive gravity so a centre-drop reaches the floor in exactly the
    // chosen note division at the host tempo (the "first bounce" lands on the grid).
    double bpm = 120.0;   // fallback when the host gives no tempo (e.g. standalone)
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto hostBpm = pos->getBpm())
                bpm = *hostBpm;
    bpm = juce::jmax (20.0, bpm);

    const bool delayMinSync = apvts.getRawParameterValue ("DELAY_MIN_SYNC")->load() >= 0.5f;
    const bool delayMaxSync = apvts.getRawParameterValue ("DELAY_MAX_SYNC")->load() >= 0.5f;

    const float delayMinMs = delayMinSync
        ? DelaySync::milliseconds ((int) apvts.getRawParameterValue ("DELAY_MIN_DIV")->load(), bpm)
        : apvts.getRawParameterValue ("DELAY_MIN_MS")->load();
    const float delayMaxMs = delayMaxSync
        ? DelaySync::milliseconds ((int) apvts.getRawParameterValue ("DELAY_MAX_DIV")->load(), bpm)
        : apvts.getRawParameterValue ("DELAY_MAX_MS")->load();

    const int syncIndex = juce::jlimit (0, (int) kSyncDivisions.size() - 1,
                                        (int) apvts.getRawParameterValue ("SYNC")->load());
    const double fallSeconds = kSyncDivisions[(size_t) syncIndex].quarters * 60.0 / bpm;
    const float  gravity     = ParticleSystem::gravityForFallTime (fallSeconds, controlRateHz,
                                                                   ParticleSystem::spawnHeight);

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const float inL = buffer.getSample (0, sample);
        const float inR = numChannels > 1 ? buffer.getSample (1, sample) : inL;

        // 1) Record the dry input into the delay line.
        delayBuffer.writeFrame (inL, inR);

        // 2) Onset detection -> queue a burst (released gradually by update()).
        if (transientDetector.processSample (inL, inR, threshold))
            particleSystem.triggerBurst ((int) particleCount, scatter);

        // 3) Advance the particle physics at the fixed control rate; each wall
        //    hit produces an EchoEvent that we turn into a grain.
        controlAccumulator += 1.0;
        while (controlAccumulator >= samplesPerControlTick)
        {
            controlAccumulator -= samplesPerControlTick;

            echoEvents.clear();
            particleSystem.update (gravity, bounce, decay, delayMinMs, delayMaxMs, echoEvents);

            for (const auto& e : echoEvents)
                triggerGrain (e);
        }

        // 4) Render every active grain into the wet bus.
        float wetL = 0.0f;
        float wetR = 0.0f;

        for (auto& g : activeGrains)
        {
            const float phase = (float) g.currentSample / (float) g.totalSamples;
            const float env   = std::sin (phase * juce::MathConstants<float>::pi); // Hann-ish window

            const float echoL = delayBuffer.readSample (0, g.delayMs);
            const float echoR = delayBuffer.readSample (1, g.delayMs);
            const float mono  = 0.5f * (echoL + echoR);

            // Brightness low-pass.
            g.lpState += g.lpCoeff * (mono - g.lpState);
            const float voiced = g.lpState * g.gain * env;

            wetL += voiced * g.leftGain;
            wetR += voiced * g.rightGain;

            if (++g.currentSample >= g.totalSamples)
                g.alive = false;
        }

        activeGrains.erase (std::remove_if (activeGrains.begin(), activeGrains.end(),
                                            [] (const EchoGrain& g) { return ! g.alive; }),
                            activeGrains.end());

        // Soft-clip the wet bus so dense echo clouds stay polite (dry stays clean).
        wetL = std::tanh (wetL * wetSafety);
        wetR = std::tanh (wetR * wetSafety);

        const float mix     = mixSmoothed.getNextValue();
        const float outGain = outputSmoothed.getNextValue();

        if (numChannels > 1)
        {
            const float outL = (inL * (1.0f - mix) + wetL * mix) * outGain;
            const float outR = (inR * (1.0f - mix) + wetR * mix) * outGain;
            buffer.setSample (0, sample, outL);
            buffer.setSample (1, sample, outR);
        }
        else
        {
            const float wetMono = 0.5f * (wetL + wetR);
            buffer.setSample (0, sample, (inL * (1.0f - mix) + wetMono * mix) * outGain);
        }
    }
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
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ParticleDelayAudioProcessor();
}
