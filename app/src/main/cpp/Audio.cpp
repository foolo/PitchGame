#include "Audio.h"
#include <cmath>
#include <algorithm>

// Global state for voice control
static float gLastRms = 0.0f;
static float gLastPitch = 0.0f;

// Voice range constants
const float kVoicePitchMin = 180.0f;
const float kVoicePitchMax = 230.0f;

float Audio_getLastRms() { return gLastRms; }
float Audio_getLastPitch() { return gLastPitch; }

extern "C" JNIEXPORT void JNICALL
Java_com_example_mygame1_MainActivity_analyzeAudio(JNIEnv *env, jobject thiz, jshortArray jbuffer, jint size) {
    jshort *buffer = env->GetShortArrayElements(jbuffer, nullptr);

    float kVoiceMid = std::sqrt(kVoicePitchMin * kVoicePitchMax);
    float detectionPitchMax = kVoiceMid * std::sqrt(2.0f);
    float detectionPitchMin = kVoiceMid / std::sqrt(2.0f);

    // Calculate RMS
    double sum_sq = 0;
    for (int i = 0; i < size; ++i) {
        sum_sq += (double)buffer[i] * buffer[i];
    }
    auto rms = (float)std::sqrt(sum_sq / (double)size);
    gLastRms = rms;

    float pitch = 0.0f;
    if (rms > 500.0f) {
        // Autocorrelation-based pitch detection, restricted to kVoicePitchMin-kVoicePitchMax
        int sampleRate = 44100;
        int minPeriod = (int)((float)sampleRate / detectionPitchMax);
        int maxPeriod = (int)((float)sampleRate / detectionPitchMin);

        // Limit periods to buffer size
        if (maxPeriod >= size) maxPeriod = size - 1;
        if (minPeriod < 1) minPeriod = 1;

        double maxCorr = -1.0;
        int bestPeriod = -1;

        if (minPeriod <= maxPeriod) {
            for (int period = minPeriod; period <= maxPeriod; ++period) {
                double corr = 0;
                for (int i = 0; i < size - period; ++i) {
                    corr += (double)buffer[i] * (double)buffer[i + period];
                }
                if (corr > maxCorr) {
                    maxCorr = corr;
                    bestPeriod = period;
                }
            }
        }

        if (bestPeriod != -1) {
            pitch = (float)sampleRate / (float)bestPeriod;

            // Strictly enforce range
            if (pitch < kVoicePitchMin || pitch > kVoicePitchMax) {
                pitch = 0.0f;
            }
        }
    }

    gLastPitch = pitch;

    env->ReleaseShortArrayElements(jbuffer, buffer, JNI_ABORT);
}
