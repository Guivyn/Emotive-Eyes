#pragma once

class Face;

// Adapter between the power state machine and the face behavior.
void FacePower_EnterRelax(Face& face);
void FacePower_EnterIdle(Face& face);
void FacePower_EnterSleep(Face& face);
void FacePower_Wake(Face& face);
