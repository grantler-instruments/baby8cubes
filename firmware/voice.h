#pragma once
#include <Audio.h>
#include "AudioSampleSnare.h"   // http://www.freesound.org/people/KEVOY/sounds/82583/
#include "AudioSampleTomtom.h"  // http://www.freesound.org/people/zgump/sounds/86334/
#include "AudioSampleHihat.h"   // http://www.freesound.org/people/mhc/sounds/102790/
#include "AudioSampleKick.h"    // http://www.freesound.org/people/DWSD/sounds/171104/

// Class to handle triggering an audio file with an envelope
class Voice {
public:
  Voice()
    : _playMemToEnv(_playMem, 0, _env, 0),
      _envToAmp(_env, 0, _amp, 0) {

    _env.attack(1.2);
    _env.hold(2.1);
    _env.decay(31.4);
    _env.sustain(0.6);
    _env.release(84.5);

    _amp.gain(1);
  }

  // Trigger the audio sample
  void noteOn(byte note, byte velocity = 127) {
    AudioNoInterrupts();
    switch (note) {
      case 60:
        {
          _playMem.play(AudioSampleKick);
          break;
        }
      case 61:
        {
          _playMem.play(AudioSampleSnare);
          break;
        }
      case 62:
        {
          _playMem.play(AudioSampleHihat);
          break;
        }
      case 63:
        {
          _playMem.play(AudioSampleTomtom);
          break;
        }
    }
    _env.noteOn();  // Apply the envelope
    AudioInterrupts();
  }

  // Stop the audio playback
  void noteOff() {
    AudioNoInterrupts();
    _env.noteOff();   // Apply envelope release
    _playMem.stop();  // Stop the playback (optional, depending on your use case)
    AudioInterrupts();
  }

  // Audio components
  AudioPlayMemory _playMem;  // For audio sample playback
  AudioEffectEnvelope _env;  // For shaping the playback with an envelope
  AudioAmplifier _amp;       // Final amplifiero
  AudioConnection _playMemToEnv;
  AudioConnection _envToAmp;
};
