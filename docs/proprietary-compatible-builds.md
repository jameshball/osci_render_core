# Proprietary-Compatible Builds

`osci_render_core` is publicly distributed under GPLv3.

This module can also contain code paths intended for private commercial use by James H Ball or by proprietary products he controls. Those paths are governed by the root policy in `../../../docs/proprietary-compatible-builds.md`.

This is an engineering policy document, not legal advice.

## Build Mode

Set `OSCI_PROPRIETARY_BUILD=1` when compiling a proprietary consumer or verifying that this module avoids incompatible dependencies.

This is a build-mode boundary only. It does not replace ownership records, private licensing terms, third-party notices, or commercial licenses for framework dependencies.

In this mode, `osci_render_core` code should:

- compile without GPL-only or unresolved-provenance dependencies
- keep reusable APIs independent of osci-render application-only assets
- prefer owned code, permissively licensed code, or separately licensed dependencies
- provide bypasses or replacements for features whose implementation depends on incompatible dependencies

When `OSCI_PROPRIETARY_BUILD` is unset or `0`, GPL osci-render builds may continue to use GPL-compatible dependencies.

## Optional Features

ChowDSP-backed sample-rate conversion is controlled by `OSCI_RENDER_CORE_ENABLE_CHOWDSP_RESAMPLING`.

- When the flag is `0`, the adapter compiles without ChowDSP and only allows bypass ratio `1.0`.
- When the flag is `1`, the consuming project must include the required ChowDSP modules.

Flag defaults and proprietary-mode conflicts are centralized in `../osci_render_core_config.h`. ChowDSP dependency checks live in the DSP support headers that require those types.

The flag must remain off in proprietary-compatible builds unless that dependency is separately cleared for proprietary use and the guard is intentionally updated.

Permissive third-party notices for reusable code in this module are recorded in `../THIRD_PARTY_NOTICES.md`.

## Contribution Policy

Do not accept third-party contributions into proprietary-compatible paths unless the contribution is under terms compatible with private commercial use. Otherwise, keep the contribution outside `OSCI_PROPRIETARY_BUILD` builds or reimplement it independently.

## References

- Root policy: `../../../docs/proprietary-compatible-builds.md`
