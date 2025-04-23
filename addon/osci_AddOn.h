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

#include <juce_core/juce_core.h>

namespace osci
{

/**
 * @brief Base interface for osci-render addons
 * 
 * This class defines the interface that all osci-render addons must implement.
 * Implementations of this interface can be compiled into dynamic libraries
 * and loaded at runtime by the main application.
 */
class JUCE_API AddOn
{
public:
    /** Destructor */
    virtual ~AddOn() = default;

    /** Returns the name of the addon */
    virtual juce::String getName() const = 0;

    /** Returns the version of the addon */
    virtual juce::String getVersion() const = 0;

    /** Initialize the addon with any required setup */
    virtual bool initialize() = 0;

    /** Clean up any resources used by the addon */
    virtual void shutdown() = 0;

    /** Returns all the effects provided by this addon */
    virtual std::vector<std::shared_ptr<osci::Effect>> getEffects() const = 0;
};

using CreateFunc = AddOn* (*)();

} // namespace osci
