#ifndef MYGAME1_AUDIO_H
#define MYGAME1_AUDIO_H

#include <jni.h>

// Voice range constants
extern const float kVoicePitchMin;
extern const float kVoicePitchMax;

float Audio_getLastRms();
float Audio_getLastPitch();

#endif //MYGAME1_AUDIO_H
