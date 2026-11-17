# Included sources

- Gurux DLMS C: copied from `/home/hazim/github/GuruxDLMS_test/development`
  (local checkout HEAD `1cd045c5bfc468eebc719f1bd0e37a0c42d79b35`). Local source
  contents are preserved; this is not a claim that the checkout was pristine.
  Source copyright notices and upstream GPL v2 text are included under
  `vendor/gurux`. Upstream: https://github.com/Gurux/GuruxDLMS.c .
- Transport sources: derived from `dlms_bridge_project`'s connection,
  communication and Gurux callback files, retaining their Gurux notices.
  Includes were made relative to CMake include directories; TCP receive timeout
  rounding was adjusted to preserve fractional milliseconds.
- cJSON v1.7.19: `cJSON.c`, `cJSON.h`, and MIT license copied from
  https://github.com/DaveGamble/cJSON/tree/v1.7.19 .
- SBKR headers: supplied by the user; copyright and other notices preserved in
  `vendor/sbkr`. Matching implementation is not included.
- Configuration and Word reference document: supplied by the user. The original
  JSON is retained alongside the enriched examples.

This file records dependency provenance; it does not relicense third-party code.
