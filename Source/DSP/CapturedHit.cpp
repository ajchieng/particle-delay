#include "CapturedHit.h"
#include <cmath>

void CapturedHitBank::prepare (double sampleRate)
{
    currentSampleRate = std::isfinite (sampleRate) && sampleRate > 0.0
        ? sampleRate
        : 44100.0;
    allocatedSamples = juce::jmax (
        1, (int) std::ceil (maximumCaptureMs * 0.001 * currentSampleRate));
    minimumSamples = juce::jmax (
        1, (int) std::round (minimumCaptureMs * 0.001 * currentSampleRate));
    silenceSamples = juce::jmax (
        1, (int) std::round (0.020 * currentSampleRate));

    for (auto& capture : captures)
        capture.audio.setSize (2, allocatedSamples, false, true, true);

    reset();
}

void CapturedHitBank::reset()
{
    for (auto& capture : captures)
    {
        capture.sourceId = 0;
        capture.startOrder = 0;
        capture.writtenSamples = 0;
        capture.finalLength = 0;
        capture.maximumSamples = 0;
        capture.belowThresholdSamples = 0;
        capture.peak = 0.0f;
        capture.active = false;
        capture.recording = false;
    }

    nextSourceId = 1;
    nextStartOrder = 1;
}

CapturedHitBank::StartResult CapturedHitBank::startCapture (float maximumMs)
{
    Capture* target = nullptr;
    for (auto& capture : captures)
    {
        if (! capture.active)
        {
            target = &capture;
            break;
        }
    }

    if (target == nullptr)
    {
        target = &captures.front();
        for (auto& capture : captures)
            if (capture.startOrder < target->startOrder)
                target = &capture;
    }

    StartResult result;
    result.retiredSourceId = target->active ? target->sourceId : 0;
    result.sourceId = nextSourceId++;
    if (result.sourceId == 0)
        result.sourceId = nextSourceId++;

    target->sourceId = result.sourceId;
    target->startOrder = nextStartOrder++;
    target->writtenSamples = 0;
    target->finalLength = 0;
    target->maximumSamples = juce::jlimit (
        minimumSamples, allocatedSamples,
        (int) std::round (juce::jlimit (minimumCaptureMs, maximumCaptureMs, maximumMs)
                          * 0.001 * currentSampleRate));
    target->belowThresholdSamples = 0;
    target->peak = 0.0f;
    target->active = true;
    target->recording = true;
    return result;
}

void CapturedHitBank::finishMatureCaptures()
{
    for (auto& capture : captures)
        if (capture.recording && capture.writtenSamples >= minimumSamples)
            finishCapture (capture, false);
}

void CapturedHitBank::processSample (float left, float right)
{
    for (auto& capture : captures)
    {
        if (! capture.recording)
            continue;

        if (capture.writtenSamples >= capture.maximumSamples)
        {
            finishCapture (capture, false);
            continue;
        }

        capture.audio.setSample (0, capture.writtenSamples, left);
        capture.audio.setSample (1, capture.writtenSamples, right);
        ++capture.writtenSamples;

        const float amplitude = juce::jmax (std::abs (left), std::abs (right));
        capture.peak = juce::jmax (capture.peak, amplitude);

        if (capture.writtenSamples >= minimumSamples && capture.peak > 0.0f)
        {
            if (amplitude <= capture.peak * 0.02f)
                ++capture.belowThresholdSamples;
            else
                capture.belowThresholdSamples = 0;

            if (capture.belowThresholdSamples >= silenceSamples)
            {
                finishCapture (capture, true);
                continue;
            }
        }

        if (capture.writtenSamples >= capture.maximumSamples)
            finishCapture (capture, false);
    }
}

bool CapturedHitBank::isActive (uint64_t sourceId) const
{
    return find (sourceId) != nullptr;
}

bool CapturedHitBank::isRecording (uint64_t sourceId) const
{
    if (const auto* capture = find (sourceId))
        return capture->recording;
    return false;
}

int CapturedHitBank::getAvailableSamples (uint64_t sourceId) const
{
    if (const auto* capture = find (sourceId))
        return capture->recording ? capture->writtenSamples : capture->finalLength;
    return 0;
}

float CapturedHitBank::getSample (uint64_t sourceId, int channel, int sampleIndex) const
{
    if (const auto* capture = find (sourceId))
    {
        const int available = capture->recording
            ? capture->writtenSamples
            : capture->finalLength;
        if (sampleIndex >= 0 && sampleIndex < available)
            return capture->audio.getSample (juce::jlimit (0, 1, channel), sampleIndex);
    }
    return 0.0f;
}

int CapturedHitBank::getSlotForSource (uint64_t sourceId) const
{
    for (int slot = 0; slot < maxCaptures; ++slot)
        if (captures[(size_t) slot].active
            && captures[(size_t) slot].sourceId == sourceId)
            return slot;
    return -1;
}

bool CapturedHitBank::isHandleActive (int slot, uint64_t sourceId) const
{
    return slot >= 0 && slot < maxCaptures
        && captures[(size_t) slot].active
        && captures[(size_t) slot].sourceId == sourceId;
}

bool CapturedHitBank::isHandleRecording (int slot, uint64_t sourceId) const
{
    return isHandleActive (slot, sourceId)
        && captures[(size_t) slot].recording;
}

int CapturedHitBank::getHandleAvailableSamples (int slot, uint64_t sourceId) const
{
    if (! isHandleActive (slot, sourceId))
        return 0;

    const auto& capture = captures[(size_t) slot];
    return capture.recording ? capture.writtenSamples : capture.finalLength;
}

float CapturedHitBank::getHandleSample (int slot,
                                        uint64_t sourceId,
                                        int channel,
                                        int sampleIndex) const
{
    const int available = getHandleAvailableSamples (slot, sourceId);
    if (sampleIndex < 0 || sampleIndex >= available)
        return 0.0f;

    return captures[(size_t) slot].audio.getSample (
        juce::jlimit (0, 1, channel), sampleIndex);
}

bool CapturedHitBank::getHandleFrame (int slot,
                                      uint64_t sourceId,
                                      int sampleIndex,
                                      float& left,
                                      float& right,
                                      int& availableSamples,
                                      bool& recording) const
{
    left = 0.0f;
    right = 0.0f;
    availableSamples = 0;
    recording = false;

    if (! isHandleActive (slot, sourceId))
        return false;

    const auto& capture = captures[(size_t) slot];
    recording = capture.recording;
    availableSamples = recording ? capture.writtenSamples : capture.finalLength;

    if (sampleIndex >= 0 && sampleIndex < availableSamples)
    {
        left = capture.audio.getSample (0, sampleIndex);
        right = capture.audio.getSample (1, sampleIndex);
    }

    return true;
}

CapturedHitBank::Capture* CapturedHitBank::find (uint64_t sourceId)
{
    for (auto& capture : captures)
        if (capture.active && capture.sourceId == sourceId)
            return &capture;
    return nullptr;
}

const CapturedHitBank::Capture* CapturedHitBank::find (uint64_t sourceId) const
{
    for (const auto& capture : captures)
        if (capture.active && capture.sourceId == sourceId)
            return &capture;
    return nullptr;
}

void CapturedHitBank::finishCapture (Capture& capture, bool trimSilence)
{
    capture.recording = false;
    capture.finalLength = capture.writtenSamples;
    if (trimSilence)
        capture.finalLength = juce::jmax (
            minimumSamples, capture.writtenSamples - capture.belowThresholdSamples);
}

void StereoSafetyLimiter::prepare (double sampleRate)
{
    const double safeRate = std::isfinite (sampleRate) && sampleRate > 0.0
        ? sampleRate
        : 44100.0;
    releaseCoeff = (float) std::exp (-1.0 / (0.050 * safeRate));
    reset();
}

void StereoSafetyLimiter::reset()
{
    gain = 1.0f;
}

void StereoSafetyLimiter::process (float& left, float& right)
{
    const float peak = juce::jmax (std::abs (left), std::abs (right));
    const float target = peak > ceiling ? ceiling / peak : 1.0f;

    if (target < gain)
        gain = target;
    else
        gain = target + releaseCoeff * (gain - target);

    left *= gain;
    right *= gain;
}
