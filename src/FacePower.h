#pragma once

class Face;

// Implemented in main.cpp where the complete Face class lives.
void FacePower_EnterRelax(Face& face);
void FacePower_EnterIdle(Face& face);
void FacePower_EnterSleep(Face& face);
void FacePower_Wake(Face& face);
