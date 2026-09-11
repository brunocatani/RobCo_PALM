#pragma once
struct ImFont;
namespace wheel {
void prepareFonts(); // Startup only, before either renderer can consume the bytes.
// Uses preloaded immutable font bytes in the current ImGui context; no file I/O.
ImFont* addPreparedFont(float size);
void installFonts();
}
