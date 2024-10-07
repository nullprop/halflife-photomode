# Half-Life Photo Mode

A basic photo mode for Half-Life.

Releases: https://github.com/laurirasanen/halflife-photomode/releases

## Install

Move this folder to the root directory of your Half-Life install.

So you end up with a structure like:

Half-Life
  photomode
    cl_dlls
    dlls
  valve
  valve_hd

Then add '-game photomode' to your game launch settings.

## Settings

See keyboard tab in settings for controls.

Default:
  TAB: photomode
  ARROW KEYS: move horizontally
  PG UP/PG DOWN: move vertically

Alternatively bind keys to:
  photomode
  +cam_pm_forward
  +cam_pm_back
  +cam_pm_right
  +cam_pm_left
  +cam_pm_up
  +cam_pm_down

## Known Issues

### No raw input

You should enable raw input under mouse settings.
Otherwise while in photo mode, the mouse will "collide" with edges of the window,
and not let you turn any further.
Gordon will also snap to the angle you are looking when exiting photo mode.

### Unpausing during photo mode

Photo mode toggles pause when entering and exiting.
If you toggle pause via any other means while in photo mode,
it will still be toggled again when exiting.
e.g. hitting ESC to open and close the menu while in photo mode
will result in the game being unpaused while in photo mode,
and then paused again when exiting. Hit escape again afterwards to resolve.

