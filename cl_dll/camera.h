//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

// Camera.h  --  defines and such for a 3rd person camera
// NOTE: must include quakedef.h first

#pragma once

// pitch, yaw, dist/roll
extern Vector cam_ofs;
// photomode pos
extern Vector cam_pm_ofs;
// Using third person camera
extern bool cam_thirdperson;
extern bool cam_photomode;

void CAM_Init();
void CAM_ClearStates();
void CAM_StartMouseMove();
void CAM_EndMouseMove();
