#pragma once

// Pure wheel policy shared by the video surface and the timeline. It knows
// nothing about widgets, stored preferences, or playback commands.
int wheelNotches(int deltaY);
double volumeAfterWheel(double current, int deltaY, int step);
