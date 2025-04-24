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

#include "osci_render_core.h"

// Include effect implementations
#include "effect/osci_Effect.cpp"
#include "effect/osci_EffectApplication.cpp"

// Include shape implementations
#include "shape/osci_Shape.cpp"
#include "shape/osci_Point.cpp"
#include "shape/osci_Line.cpp"
#include "shape/osci_CircleArc.cpp"
#include "shape/osci_CubicBezierCurve.cpp"
#include "shape/osci_QuadraticBezierCurve.cpp"

// Include concurrency implementations
#include "concurrency/osci_AudioBackgroundThread.cpp"
#include "concurrency/osci_AudioBackgroundThreadManager.cpp"

namespace osci
{
    // The base class is pure virtual, so no implementation is needed here
} // namespace osci
