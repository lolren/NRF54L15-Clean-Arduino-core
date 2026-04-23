#pragma once

// Minimal Matter core-stage shim for staged upstream support units that call
// ChipLogProgress() but do not need the full CHIP logging framework yet.

#define ChipLogProgress(MOD, MSG, ...) ((void)0)
