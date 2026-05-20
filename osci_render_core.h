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

#ifndef OSCI_PROPRIETARY_BUILD
#define OSCI_PROPRIETARY_BUILD 0
#endif

#ifndef OSCI_RENDER_CORE_ENABLE_CHOWDSP_RESAMPLING
#define OSCI_RENDER_CORE_ENABLE_CHOWDSP_RESAMPLING 0
#endif

#if OSCI_PROPRIETARY_BUILD && OSCI_RENDER_CORE_ENABLE_CHOWDSP_RESAMPLING
#error "OSCI_RENDER_CORE_ENABLE_CHOWDSP_RESAMPLING cannot be enabled in OSCI_PROPRIETARY_BUILD."
#endif

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_core/juce_core.h>
#include <juce_dsp/juce_dsp.h>

#if OSCI_RENDER_CORE_ENABLE_CHOWDSP_RESAMPLING
#if !__has_include(<chowdsp_dsp_utils/chowdsp_dsp_utils.h>)
#error "OSCI_RENDER_CORE_ENABLE_CHOWDSP_RESAMPLING=1 requires the ChowDSP modules in the consuming project."
#endif
#include <chowdsp_dsp_utils/chowdsp_dsp_utils.h>
#endif

// Include settings helpers
#include "settings/osci_SettingsStore.h"

// Include effect headers
#include "effect/osci_Effect.h"
#include "effect/osci_SimpleEffect.h"
#include "effect/osci_EffectApplication.h"
#include "effect/osci_EffectParameter.h"
#include "effect/osci_SimpleEffect.h"

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
#include "effects/osci_SmoothEffect.h"
#include "effects/osci_StereoEffect.h"

namespace osci {
} // namespace osci
