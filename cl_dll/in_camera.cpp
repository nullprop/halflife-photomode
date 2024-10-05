//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#include "hud.h"
#include "cl_util.h"
#include "camera.h"
#include "kbutton.h"
#include "cvardef.h"
#include "usercmd.h"
#include "const.h"
#include "camera.h"
#include "in_defs.h"
#include "Exports.h"

#include "SDL2/SDL_mouse.h"

float CL_KeyState(kbutton_t* key);

extern cl_enginefunc_t gEngfuncs;

//-------------------------------------------------- Constants

#define CAM_DIST_DELTA 1.0
#define CAM_ANGLE_DELTA 2.5
#define CAM_ANGLE_SPEED 2.5
#define CAM_PM_MOVE_SPEED 1.0
#define CAM_MIN_DIST 30.0

enum ECAM_Command
{
	CAM_COMMAND_NONE = 0,
	CAM_COMMAND_TOTHIRDPERSON = 1,
	CAM_COMMAND_TOFIRSTPERSON = 2,
	CAM_COMMAND_TOPHOTOMODE = 3
};

//-------------------------------------------------- Global Variables

cvar_t* cam_command;
cvar_t* cam_snapto;
cvar_t* cam_idealyaw;
cvar_t* cam_idealpitch;
cvar_t* cam_idealroll;
cvar_t* cam_idealdist;
cvar_t* cam_contain;

cvar_t* c_maxpitch;
cvar_t* c_minpitch;
cvar_t* c_maxyaw;
cvar_t* c_minyaw;
cvar_t* c_maxroll;
cvar_t* c_minroll;
cvar_t* c_maxdistance;
cvar_t* c_mindistance;

int prev_hud_draw;
int prev_crosshair;
int prev_showpause;

// pitch, yaw, dist/roll
Vector cam_ofs;
// photomode pos
Vector cam_pm_ofs;

// In third person
bool cam_thirdperson;
bool cam_photomode;
bool cam_mousemove; //true if we are moving the cam with the mouse, False if not
bool iMouseInUse = false;
bool cam_distancemove;
Point cam_mouse;
//-------------------------------------------------- Local Variables

static kbutton_t cam_pitchup, cam_pitchdown, cam_yawleft, cam_yawright;
static kbutton_t cam_in, cam_out, cam_move;
static kbutton_t cam_pm_forward;
static kbutton_t cam_pm_back;
static kbutton_t cam_pm_left;
static kbutton_t cam_pm_right;
static kbutton_t cam_pm_up;
static kbutton_t cam_pm_down;

//-------------------------------------------------- Prototypes

void CAM_ToThirdPerson();
void CAM_ToFirstPerson();
void CAM_ToPhotoMode();
void CAM_StartDistance();
void CAM_EndDistance();

void SDL_GetMouseMovement(Point* p)
{
    SDL_GetRelativeMouseState(&p->x, &p->y);
}

//-------------------------------------------------- Local Functions

float MoveToward(float cur, float goal, float maxspeed)
{
	if (cur != goal)
	{
		if (fabs(cur - goal) > 180.0)
		{
			if (cur < goal)
				cur += 360.0;
			else
				cur -= 360.0;
		}

		if (cur < goal)
		{
			if (cur < goal - 1.0)
				cur += (goal - cur) / 4.0;
			else
				cur = goal;
		}
		else
		{
			if (cur > goal + 1.0)
				cur -= (cur - goal) / 4.0;
			else
				cur = goal;
		}
	}


	// bring cur back into range
	if (cur < 0)
		cur += 360.0;
	else if (cur >= 360)
		cur -= 360;

	return cur;
}


//-------------------------------------------------- Gobal Functions

typedef struct
{
	Vector boxmins, boxmaxs; // enclose the test object along entire move
	float *mins, *maxs;		 // size of the moving object
	Vector mins2, maxs2;	 // size when clipping against mosnters
	float *start, *end;
	trace_t trace;
	int type;
	edict_t* passedict;
	qboolean monsterclip;
} moveclip_t;

extern trace_t SV_ClipMoveToEntity(edict_t* ent, Vector start, Vector mins, Vector maxs, Vector end);

void DLLEXPORT CAM_Think()
{
	//	RecClCamThink();

	Vector origin;
	Vector ext, pnt, camForward, camRight, camUp, camMove;
	moveclip_t clip;
	float dist;
	Vector camAngles;
	Vector viewangles;
	float flSensitivity = 0.022f * CVAR_GET_FLOAT("sensitivity");

	switch ((int)cam_command->value)
	{
	case CAM_COMMAND_TOTHIRDPERSON:
		CAM_ToThirdPerson();
		break;

	case CAM_COMMAND_TOFIRSTPERSON:
		CAM_ToFirstPerson();
		break;

    case CAM_COMMAND_TOPHOTOMODE:
        CAM_ToPhotoMode();
        break;

	case CAM_COMMAND_NONE:
	default:
		break;
	}

	if (!cam_thirdperson && !cam_photomode)
		return;

    camAngles[PITCH] = cam_idealpitch->value;
    camAngles[YAW] = cam_idealyaw->value;
    camAngles[ROLL] = cam_idealroll->value;
    dist = cam_idealdist->value;

    SDL_GetMouseMovement(&cam_mouse);

    if (cam_thirdperson)
    {
        //
        //movement of the camera with the mouse
        //
        if (cam_mousemove)
        {
            //check for X delta values and adjust accordingly
            //eventually adjust YAW based on amount of movement
            //don't do any movement of the cam using YAW/PITCH if we are zooming in/out the camera
            if (!cam_distancemove)
            {
                //keep the camera within certain limits around the player (ie avoid certain bad viewing angles)
                if (cam_mouse.x != 0)
                {
                    camAngles[YAW] -= flSensitivity * cam_mouse.x;
                    if (camAngles[YAW] > c_maxyaw->value)
                    {
                        camAngles[YAW] = c_maxyaw->value;
                    }
                    if (camAngles[YAW] < c_minyaw->value)
                    {
                        camAngles[YAW] = c_minyaw->value;
                    }
                }

                //check for y delta values and adjust accordingly
                //eventually adjust PITCH based on amount of movement
                //also make sure camera is within bounds
                if (cam_mouse.y != 0)
                {
                    camAngles[PITCH] += flSensitivity * cam_mouse.y;
                    if (camAngles[PITCH] > c_maxpitch->value)
                    {
                        camAngles[PITCH] = c_maxpitch->value;
                    }
                    if (camAngles[PITCH] < c_minpitch->value)
                    {
                        camAngles[PITCH] = c_minpitch->value;
                    }
                }
            }
        }

        //Nathan code here
        if (0 != CL_KeyState(&cam_pitchup))
            camAngles[PITCH] += CAM_ANGLE_DELTA;
        else if (0 != CL_KeyState(&cam_pitchdown))
            camAngles[PITCH] -= CAM_ANGLE_DELTA;

        if (0 != CL_KeyState(&cam_yawleft))
            camAngles[YAW] -= CAM_ANGLE_DELTA;
        else if (0 != CL_KeyState(&cam_yawright))
            camAngles[YAW] += CAM_ANGLE_DELTA;

        if (0 != CL_KeyState(&cam_in))
        {
            dist -= CAM_DIST_DELTA;
            if (dist < CAM_MIN_DIST)
            {
                // If we go back into first person, reset the angle
                camAngles[PITCH] = 0;
                camAngles[YAW] = 0;
                dist = CAM_MIN_DIST;
            }
        }
        else if (0 != CL_KeyState(&cam_out))
            dist += CAM_DIST_DELTA;

        if (cam_distancemove)
        {
            if (cam_mouse.y != 0)
            {
                dist += CAM_DIST_DELTA * flSensitivity * cam_mouse.y;
                if (dist > c_maxdistance->value)
                {
                    dist = c_maxdistance->value;
                }
                if (dist < c_mindistance->value)
                {
                    dist = c_mindistance->value;
                }
            }
        }
    }

    if (cam_photomode)
    {
        cam_ofs[YAW] -= flSensitivity * cam_mouse.x;
        cam_ofs[PITCH] += flSensitivity * cam_mouse.y;
        cam_ofs[ROLL] = 0;

        AngleVectors(cam_ofs, camForward, camRight, NULL);

        camMove[0] = CL_KeyState(&cam_pm_forward);
        camMove[0] -= CL_KeyState(&cam_pm_back);
        camMove[1] = CL_KeyState(&cam_pm_right);
        camMove[1] -= CL_KeyState(&cam_pm_left);
        camMove[2] = CL_KeyState(&cam_pm_up);
        camMove[2] -= CL_KeyState(&cam_pm_down);

        for (int i = 0; i < 3; i++)
        {
            cam_pm_ofs[i] += camMove[0] * camForward[i] * CAM_PM_MOVE_SPEED;
            cam_pm_ofs[i] += camMove[1] * camRight[i] * CAM_PM_MOVE_SPEED;
        }
        cam_pm_ofs[2] += camMove[2] * CAM_PM_MOVE_SPEED;
    }

    if (cam_thirdperson)
    {
        // update ideal
        cam_idealpitch->value = camAngles[PITCH];
        cam_idealyaw->value = camAngles[YAW];
        cam_idealroll->value = camAngles[ROLL];
        cam_idealdist->value = dist;

        // Move towards ideal
        VectorCopy(cam_ofs, camAngles);

        gEngfuncs.GetViewAngles((float*)viewangles);

        if (0 != cam_snapto->value)
        {
            camAngles[YAW] = cam_idealyaw->value + viewangles[YAW];
            camAngles[PITCH] = cam_idealpitch->value + viewangles[PITCH];
            camAngles[ROLL] = cam_idealdist->value;
        }
        else
        {
            if (camAngles[YAW] - viewangles[YAW] != cam_idealyaw->value)
                camAngles[YAW] = MoveToward(camAngles[YAW], cam_idealyaw->value + viewangles[YAW], CAM_ANGLE_SPEED);

            if (camAngles[PITCH] - viewangles[PITCH] != cam_idealpitch->value)
                camAngles[PITCH] = MoveToward(camAngles[PITCH], cam_idealpitch->value + viewangles[PITCH], CAM_ANGLE_SPEED);

            if (fabs(camAngles[ROLL] - cam_idealdist->value) < 2.0)
            {
                camAngles[ROLL] = cam_idealdist->value;
            }
            else
            {
                camAngles[ROLL] += (cam_idealdist->value - camAngles[2]) / 4.0;
            }
        }
        cam_ofs[0] = camAngles[0];
        cam_ofs[1] = camAngles[1];
        cam_ofs[2] = dist;
    }
}

extern void KeyDown(kbutton_t* b); // HACK
extern void KeyUp(kbutton_t* b);   // HACK

void CAM_PitchUpDown() { KeyDown(&cam_pitchup); }
void CAM_PitchUpUp() { KeyUp(&cam_pitchup); }
void CAM_PitchDownDown() { KeyDown(&cam_pitchdown); }
void CAM_PitchDownUp() { KeyUp(&cam_pitchdown); }
void CAM_YawLeftDown() { KeyDown(&cam_yawleft); }
void CAM_YawLeftUp() { KeyUp(&cam_yawleft); }
void CAM_YawRightDown() { KeyDown(&cam_yawright); }
void CAM_YawRightUp() { KeyUp(&cam_yawright); }
void CAM_InDown() { KeyDown(&cam_in); }
void CAM_InUp() { KeyUp(&cam_in); }
void CAM_OutDown() { KeyDown(&cam_out); }
void CAM_OutUp() { KeyUp(&cam_out); }
void CAM_PM_ForwardDown() { KeyDown(&cam_pm_forward); }
void CAM_PM_ForwardUp() { KeyUp(&cam_pm_forward); }
void CAM_PM_BackDown() { KeyDown(&cam_pm_back); }
void CAM_PM_BackUp() { KeyUp(&cam_pm_back); }
void CAM_PM_LeftDown() { KeyDown(&cam_pm_left); }
void CAM_PM_LeftUp() { KeyUp(&cam_pm_left); }
void CAM_PM_RightDown() { KeyDown(&cam_pm_right); }
void CAM_PM_RightUp() { KeyUp(&cam_pm_right); }
void CAM_PM_UpDown() { KeyDown(&cam_pm_up); }
void CAM_PM_UpUp() { KeyUp(&cam_pm_up); }
void CAM_PM_DownDown() { KeyDown(&cam_pm_down); }
void CAM_PM_DownUp() { KeyUp(&cam_pm_down); }

void CAM_ExitPhotoMode()
{
    if (cam_photomode)
    {
        cam_photomode = false;
        iMouseInUse = cam_mousemove;
        gEngfuncs.Cvar_SetValue("hud_draw", prev_hud_draw);
        gEngfuncs.Cvar_SetValue("crosshair", prev_crosshair);
        gEngfuncs.Cvar_SetValue("showpause", prev_showpause);
        gEngfuncs.pfnClientCmd("pause");
    }
}

void CAM_ToThirdPerson()
{
	Vector viewangles;

#if !defined(_DEBUG)
	if (gEngfuncs.GetMaxClients() > 1)
	{
		// no thirdperson in multiplayer.
		return;
	}
#endif

	gEngfuncs.GetViewAngles((float*)viewangles);

	if (!cam_thirdperson)
	{
        CAM_ExitPhotoMode();
		cam_thirdperson = true;

		cam_ofs[YAW] = viewangles[YAW];
		cam_ofs[PITCH] = viewangles[PITCH];
		cam_ofs[ROLL] = CAM_MIN_DIST;
	}

	gEngfuncs.Cvar_SetValue("cam_command", 0);
}

void CAM_ToFirstPerson()
{
	cam_thirdperson = false;
    CAM_ExitPhotoMode();
	gEngfuncs.Cvar_SetValue("cam_command", 0);
}

void CAM_ToPhotoMode()
{
	Vector viewangles;

#if !defined(_DEBUG)
	if (gEngfuncs.GetMaxClients() > 1)
	{
		// no photo mode in multiplayer.
		return;
	}
#endif

    gEngfuncs.GetViewAngles((float*)viewangles);

    if (!cam_photomode)
	{
        cam_thirdperson = false;
        cam_photomode = true;
		iMouseInUse = true;
        cam_mousemove = false;
        cam_distancemove = false; 

		cam_ofs[YAW] = viewangles[YAW];
		cam_ofs[PITCH] = viewangles[PITCH];
		cam_ofs[ROLL] = viewangles[ROLL];

        cam_pm_ofs[0] = 0;
        cam_pm_ofs[1] = 0;
        cam_pm_ofs[2] = 0;

        prev_hud_draw = (int)CVAR_GET_FLOAT("hud_draw");
        prev_crosshair = (int)CVAR_GET_FLOAT("crosshair");
        prev_showpause = (int)CVAR_GET_FLOAT("showpause");
        gEngfuncs.Cvar_SetValue("hud_draw", 0);
        gEngfuncs.Cvar_SetValue("crosshair", 0);
        gEngfuncs.Cvar_SetValue("showpause", 0);
        gEngfuncs.pfnClientCmd("pause");
	}
    else
    {
        CAM_ExitPhotoMode();
    }

    gEngfuncs.Cvar_SetValue("cam_command", 0);
}

void CAM_ToggleSnapto()
{
	cam_snapto->value = 0 != cam_snapto->value ? 0 : 1;
}

void CAM_Init()
{
	gEngfuncs.pfnAddCommand("+campitchup", CAM_PitchUpDown);
	gEngfuncs.pfnAddCommand("-campitchup", CAM_PitchUpUp);
	gEngfuncs.pfnAddCommand("+campitchdown", CAM_PitchDownDown);
	gEngfuncs.pfnAddCommand("-campitchdown", CAM_PitchDownUp);
	gEngfuncs.pfnAddCommand("+camyawleft", CAM_YawLeftDown);
	gEngfuncs.pfnAddCommand("-camyawleft", CAM_YawLeftUp);
	gEngfuncs.pfnAddCommand("+camyawright", CAM_YawRightDown);
	gEngfuncs.pfnAddCommand("-camyawright", CAM_YawRightUp);
	gEngfuncs.pfnAddCommand("+camin", CAM_InDown);
	gEngfuncs.pfnAddCommand("-camin", CAM_InUp);
	gEngfuncs.pfnAddCommand("+camout", CAM_OutDown);
	gEngfuncs.pfnAddCommand("-camout", CAM_OutUp);
	gEngfuncs.pfnAddCommand("thirdperson", CAM_ToThirdPerson);
	gEngfuncs.pfnAddCommand("firstperson", CAM_ToFirstPerson);
	gEngfuncs.pfnAddCommand("photomode", CAM_ToPhotoMode);
	gEngfuncs.pfnAddCommand("+cammousemove", CAM_StartMouseMove);
	gEngfuncs.pfnAddCommand("-cammousemove", CAM_EndMouseMove);
	gEngfuncs.pfnAddCommand("+camdistance", CAM_StartDistance);
	gEngfuncs.pfnAddCommand("-camdistance", CAM_EndDistance);
	gEngfuncs.pfnAddCommand("snapto", CAM_ToggleSnapto);
	gEngfuncs.pfnAddCommand("+cam_pm_forward", CAM_PM_ForwardDown);
	gEngfuncs.pfnAddCommand("-cam_pm_forward", CAM_PM_ForwardUp);
	gEngfuncs.pfnAddCommand("+cam_pm_back", CAM_PM_BackDown);
	gEngfuncs.pfnAddCommand("-cam_pm_back", CAM_PM_BackUp);
	gEngfuncs.pfnAddCommand("+cam_pm_left", CAM_PM_LeftDown);
	gEngfuncs.pfnAddCommand("-cam_pm_left", CAM_PM_LeftUp);
	gEngfuncs.pfnAddCommand("+cam_pm_right", CAM_PM_RightDown);
	gEngfuncs.pfnAddCommand("-cam_pm_right", CAM_PM_RightUp);
	gEngfuncs.pfnAddCommand("+cam_pm_up", CAM_PM_UpDown);
	gEngfuncs.pfnAddCommand("-cam_pm_up", CAM_PM_UpUp);
	gEngfuncs.pfnAddCommand("+cam_pm_down", CAM_PM_DownDown);
	gEngfuncs.pfnAddCommand("-cam_pm_down", CAM_PM_DownUp);

	cam_command = gEngfuncs.pfnRegisterVariable("cam_command", "0", 0);		  // tells camera to go to thirdperson
	cam_snapto = gEngfuncs.pfnRegisterVariable("cam_snapto", "0", 0);		  // snap to thirdperson view
	cam_idealyaw = gEngfuncs.pfnRegisterVariable("cam_idealyaw", "90", 0);	  // thirdperson yaw
	cam_idealpitch = gEngfuncs.pfnRegisterVariable("cam_idealpitch", "0", 0); // thirperson pitch
	cam_idealroll = gEngfuncs.pfnRegisterVariable("cam_idealroll", "0", 0); // photomode roll
	cam_idealdist = gEngfuncs.pfnRegisterVariable("cam_idealdist", "64", 0);  // thirdperson distance
	cam_contain = gEngfuncs.pfnRegisterVariable("cam_contain", "0", 0);		  // contain camera to world

	c_maxpitch = gEngfuncs.pfnRegisterVariable("c_maxpitch", "90.0", 0);
	c_minpitch = gEngfuncs.pfnRegisterVariable("c_minpitch", "-90.0", 0);
	c_maxyaw = gEngfuncs.pfnRegisterVariable("c_maxyaw", "180.0", 0);
	c_minyaw = gEngfuncs.pfnRegisterVariable("c_minyaw", "-180.0", 0);
	c_maxroll = gEngfuncs.pfnRegisterVariable("c_maxroll", "180.0", 0);
	c_minroll = gEngfuncs.pfnRegisterVariable("c_minroll", "-180.0", 0);
	c_maxdistance = gEngfuncs.pfnRegisterVariable("c_maxdistance", "200.0", 0);
	c_mindistance = gEngfuncs.pfnRegisterVariable("c_mindistance", "30.0", 0);
}

void CAM_ClearStates()
{
	Vector viewangles;

	gEngfuncs.GetViewAngles((float*)viewangles);

	cam_pitchup.state = 0;
	cam_pitchdown.state = 0;
	cam_yawleft.state = 0;
	cam_yawright.state = 0;
	cam_in.state = 0;
	cam_out.state = 0;

    cam_pm_forward.state = 0;
    cam_pm_back.state = 0;
    cam_pm_right.state = 0;
    cam_pm_left.state = 0;
    cam_pm_up.state = 0;
    cam_pm_down.state = 0;

	cam_thirdperson = false;
	cam_photomode = false;
	cam_command->value = 0;
	cam_mousemove = false;

	cam_snapto->value = 0;
	cam_distancemove = false;

	cam_ofs[0] = 0.0;
	cam_ofs[1] = 0.0;
	cam_ofs[2] = CAM_MIN_DIST;

    cam_pm_ofs[0] = 0.0;
    cam_pm_ofs[1] = 0.0;
    cam_pm_ofs[2] = 0.0;

	cam_idealpitch->value = viewangles[PITCH];
	cam_idealyaw->value = viewangles[YAW];
	cam_idealroll->value = viewangles[ROLL];
	cam_idealdist->value = CAM_MIN_DIST;

    prev_hud_draw = (int)CVAR_GET_FLOAT("hud_draw");
    prev_crosshair = (int)CVAR_GET_FLOAT("crosshair");
    prev_showpause = (int)CVAR_GET_FLOAT("showpause");
}

void CAM_StartMouseMove()
{
	//only move the cam with mouse if we are in third person.
	if (cam_thirdperson)
	{
		//set appropriate flags and initialize the old mouse position
		//variables for mouse camera movement
		if (!cam_mousemove)
		{
			cam_mousemove = true;
			iMouseInUse = true;
		}
	}
	//we are not in 3rd person view..therefore do not allow camera movement
	else
	{
		cam_mousemove = false;
		iMouseInUse = false;
	}
}

//the key has been released for camera movement
//tell the engine that mouse camera movement is off
void CAM_EndMouseMove()
{
	cam_mousemove = false;
	iMouseInUse = false;
}


//----------------------------------------------------------
//routines to start the process of moving the cam in or out
//using the mouse
//----------------------------------------------------------
void CAM_StartDistance()
{
	//only move the cam with mouse if we are in third person.
	if (cam_thirdperson)
	{
		//set appropriate flags and initialize the old mouse position
		//variables for mouse camera movement
		if (!cam_distancemove)
		{
			cam_distancemove = true;
			cam_mousemove = true;
			iMouseInUse = true;
		}
	}
	//we are not in 3rd person view..therefore do not allow camera movement
	else
	{
		cam_distancemove = false;
		cam_mousemove = false;
		iMouseInUse = false;
	}
}

//the key has been released for camera movement
//tell the engine that mouse camera movement is off
void CAM_EndDistance()
{
	cam_distancemove = false;
	cam_mousemove = false;
	iMouseInUse = false;
}

int DLLEXPORT CL_IsThirdPerson()
{
	//	RecClCL_IsThirdPerson();

    // Return true for photo mode so engine draws gordon
	return static_cast<int>(cam_thirdperson || cam_photomode || (0 != g_iUser1 && (g_iUser2 == gEngfuncs.GetLocalPlayer()->index)));
}

void DLLEXPORT CL_CameraOffset(float* ofs)
{
	//	RecClCL_GetCameraOffsets(ofs);

	VectorCopy(cam_ofs, ofs);
}

int DLLEXPORT CL_IsPhotoMode()
{
	return static_cast<int>(cam_photomode || (0 != g_iUser1 && (g_iUser2 == gEngfuncs.GetLocalPlayer()->index)));
}

void DLLEXPORT CL_CameraPhotoModeOffset(float* ofs)
{
	VectorCopy(cam_pm_ofs, ofs);
}
