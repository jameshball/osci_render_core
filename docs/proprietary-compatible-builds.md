# Proprietary-Compatible Builds

`osci_render_core` is publicly distributed under GPLv3.

This module can also contain code paths intended for private commercial use by James H Ball or by proprietary products he controls. Those paths are governed by the root policy in `../../../docs/proprietary-compatible-builds.md`.

This is an engineering policy document, not legal advice.

## Build Mode

Set `OSCI_PROPRIETARY_BUILD=1` when compiling a proprietary consumer or verifying that this module avoids incompatible dependencies.

This is a build-mode boundary only. It does not replace ownership records, private licensing terms, contributor relicensing records, third-party notices, or commercial licenses for framework dependencies.

In this mode, `osci_render_core` code should:

- compile without GPL-only or unresolved-provenance dependencies
- keep reusable APIs independent of osci-render application-only assets
- prefer owned code, permissively licensed code, or separately licensed dependencies
- allow contributor code only when it is owned, permissively licensed, or covered by a written inbound rights record compatible with proprietary use
- provide bypasses or replacements for features whose implementation depends on incompatible dependencies

When `OSCI_PROPRIETARY_BUILD` is unset or `0`, GPL osci-render builds may continue to use GPL-compatible dependencies.

## Optional Features

Optional dependencies should be guarded by explicit feature flags only when the core module actually needs them.

The integer-ratio sample-rate adapter uses JUCE-only bypass and upsampling modes. It supports `1.0`, `2.0`, `4.0`, and `8.0`, and does not require a core ChowDSP feature flag.

`OSCI_RENDER_CORE_ENABLE_MIDI_CC_LEARN` controls MIDI CC learn processing. It defaults to `OSCI_PREMIUM` for current osci-render products, but reusable consumers can set it directly.

Permissive third-party notices for reusable code in this module are recorded in `../THIRD_PARTY_NOTICES.md`.

Reusable effect implementations can live under `effects/` when they do not depend on application-only UI, assets, or unresolved dependencies. Product icons and other app assets should be attached by the consuming app through the effect builder/API rather than embedded into core effect classes.

## Contribution Policy

Do not accept third-party contributions into proprietary-compatible paths unless the contribution is under terms compatible with private commercial use or is covered by a written inbound rights record. Otherwise, keep the contribution outside `OSCI_PROPRIETARY_BUILD` builds or reimplement it independently.

Keep private relicensing or assignment evidence outside the public module when it contains private correspondence; document public third-party notices separately when redistribution requires them.

## References

- Root policy: `../../../docs/proprietary-compatible-builds.md`
