/*
  ==============================================================================

   This file is part of the osci-render Addon module
   Copyright (c) 2025 James H Ball

   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in all
   copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
   SOFTWARE.

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

  dependencies:      juce_core, juce_audio_processors

 END_JUCE_MODULE_DECLARATION

*******************************************************************************/

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_core/juce_core.h>

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

// Include concurrency headers
#include "concurrency/osci_AudioBackgroundThread.h"
#include "concurrency/osci_AudioBackgroundThreadManager.h"
#include "concurrency/osci_BlockingQueue.h"
#include "concurrency/osci_BufferConsumer.h"
#include "concurrency/osci_WriteProcess.h"

namespace osci {
} // namespace osci
