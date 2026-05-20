#pragma once

#include "../osci_render_core_config.h"

#if OSCI_RENDER_CORE_ENABLE_CHOWDSP_RESAMPLING
#if !__has_include(<chowdsp_dsp_utils/chowdsp_dsp_utils.h>)
#error "OSCI_RENDER_CORE_ENABLE_CHOWDSP_RESAMPLING=1 requires the ChowDSP modules in the consuming project."
#endif
#include <chowdsp_dsp_utils/chowdsp_dsp_utils.h>
#endif
