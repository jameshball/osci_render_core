/*
  ==============================================================================

   This file is part of the osci-render Addon module
   Copyright (c) 2025 James H Ball

  ==============================================================================
*/

#pragma once

/*******************************************************************************
 The block below describes the properties of this module, and is read by
 the Projucer to automatically generate project code that uses it.
 For details about the syntax and how to create or use a module, see the
 JUCE Module Format.txt file.

 BEGIN_JUCE_MODULE_DECLARATION

  ID:                osci_render_core
  vendor:            jameshball
  version:           1.0.0
  name:              osci-render core
  description:       Core module for osci-render
  website:           https://osci-render.com
  license:           GPLv3
  minimumCppStandard: 20

  dependencies:      juce_core, juce_audio_processors, juce_dsp

 END_JUCE_MODULE_DECLARATION

*******************************************************************************/

#include "osci_render_core_config.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_core/juce_core.h>
#include <juce_dsp/juce_dsp.h>

// Include settings helpers
#include "settings/osci_SettingsStore.h"

// Include effect headers
#include "effect/osci_Effect.h"
#include "effect/osci_SimpleEffect.h"
#include "effect/osci_EffectApplication.h"
#include "effect/osci_EffectParameter.h"

// Include shape headers
#include "shape/osci_CircleArc.h"
#include "shape/osci_CubicBezierCurve.h"
#include "shape/osci_Line.h"
#include "shape/osci_Point.h"
#include "shape/osci_QuadraticBezierCurve.h"
#include "shape/osci_Shape.h"

// Include midi headers
#include "midi/osci_MidiCCManager.h"

// Include transport headers
#include "transport/osci_DawPosition.h"

// Include concurrency headers
#include "concurrency/osci_AudioBackgroundThread.h"
#include "concurrency/osci_AudioBackgroundThreadManager.h"
#include "concurrency/osci_BlockingQueue.h"
#include "concurrency/osci_BufferConsumer.h"
#include "concurrency/osci_WriteProcess.h"

// Include DSP headers
#include "dsp/osci_IntegerRatioSampleRateAdapter.h"

// Include visualiser support headers
#include "effects/osci_BounceEffect.h"
#include "effects/osci_DelayEffect.h"
#include "effects/osci_DistortEffect.h"
#include "effects/osci_RippleEffect.h"
#include "effects/osci_RotateEffect.h"
#include "effects/osci_ScaleEffect.h"
#include "effects/osci_SkewEffect.h"
#include "effects/osci_SmoothEffect.h"
#include "effects/osci_StereoEffect.h"
#include "effects/osci_SwirlEffect.h"
#include "effects/osci_TranslateEffect.h"
#include "effects/osci_UnfoldEffect.h"
#include "effects/osci_VectorCancellingEffect.h"

namespace osci {
} // namespace osci
