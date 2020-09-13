IMPORTANT: This installer only includes sample applications and the libraries and source code needed to compile applications.  If you are running a pre-compiled Unity, Unreal, or other demo this will not update the version of RenderManager it is linked against; you must either obtain an updated demo or (if you're lucky) copy the RenderManager.dll file from this installer into appropriate location inside the game folder.

IMPORTANT: This installer is not built with the NDA libraries that implement DirectMode.

Step 0: If you are going to be using an Oculus DK2 and/or a Microsoft Xbox controller to fly around, run the following shortcut first:
  vrpn_server_Oculus_xbox: When run, it will power up the Oculus DK2 and talk to an Xbox controller plugged in via USB.

Step 1: Run an OSVR server (obtained from an OSVR Core install) with a compatible configuration file.
  (If you do not have this installed, you can run one of the servers using the installed shortcuts;
   these are older-format files run against an older server, but should still work.)
  osvr_server_nondirectmode_window: Displays in a window without motion.
  osvr_server_nondirectmode_window_lookabout: Displays stored looking-around motions.
  osvr_server_nondirectmode_Oculus_xbox: Assumed Oculus DK2 is placed to the left of the main display and rotated 90 degrees to the left.
  osvr_server_window_Oculus_xbox: Displays on a side-by-side horizontal window, tracked using DK2 and/or Xbox controller.
  osvr_server_mono_xbox: Displays on a mono horizontal window, tracked using DK2 and/or xbox controller.

Step 2: Run one of the example programs
  RenderManager*: Various graphics libraries and modes of operation.
  RenderManagerOpenGLHeadSpaceExample: Displays a small cube in head space to debug backwards eyes.
  SolidColor provides a slowly-varying solid color and works with all graphics APIs.
  SpinCubeD3D provides a smoothly-rotating cube to test update consistency.
  SpinCubeOpenGL provides a smoothly-rotating cube with frame ID displayed to test update consistency.  Timing info expects 60Hz DirectMode display device that blocks the app for vsync.

An osvr_server.exe must be running to open a display (this is where it gets information about the distortion correction and other system parameters).  The osvr_server.exe must use a configuration that defines /me/head (implicitly or explicitly) for head tracking to work.  There are shortcuts to run the server with various configuration files.  You can leave the server running and run multiple different clients one after the other.  All clients should work with all servers.

Source code for RenderManager, including example programs, is available at:
  https://github.com/sensics/OSVR-RenderManager

Version notes:
2.0.00:
    Adds example programs for both textured display of text and flying around using a joystick.
1.1.00:
    Adds "lookabout" shortcuts that replay recorded head-tracker data.
1.0.00:
    Initial release
