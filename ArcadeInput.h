#pragma once

#include <SFML/Window/Joystick.hpp>
#include <SFML/Window/Keyboard.hpp>

// Central input helper for the arcade project.
// Keyboard still works as before, while the TRIXES NES-style USB controller
// is mapped through SFML joystick input.
//
// Tested TRIXES mapping:
// D-pad  = Joystick axis X/Y
// B      = button 0
// A      = button 1
// Select = button 8
// Start  = button 9
class ArcadeInput
{
public:
	static void update();

	// Call this after a menu selection consumes the current button press.
	// It prevents the same held A/Start press from also triggering gameplay
	// on the first frame after changing screens.
	static void consumePressedState();

	// Keyboard OR controller gameplay input (aggregate over every connected
	// joystick).  This is the original single-player API used by menus and
	// games other than Surfers Quest co-op.
	static bool isMoveLeftHeld();
	static bool isMoveRightHeld();
	static bool isMoveUpHeld();
	static bool isMoveDownHeld();

	static bool isMoveLeftPressed();
	static bool isMoveRightPressed();
	static bool isMoveUpPressed();
	static bool isMoveDownPressed();

	static bool isPrimaryHeld();      // Keyboard Space / controller A
	static bool isPrimaryPressed();

	static bool isSecondaryHeld();    // Keyboard E or Shift / controller B
	static bool isSecondaryPressed();

	static bool isConfirmHeld();      // Keyboard Enter / controller Start or A
	static bool isConfirmPressed();

	static bool isBackHeld();         // Keyboard Escape / controller Select
	static bool isBackPressed();

	static bool isCancelHeld();       // Keyboard Escape / controller Select or B
	static bool isCancelPressed();

	static bool isRestartHeld();      // Keyboard R / controller Start
	static bool isRestartPressed();

	// Controller-only menu helpers.
	// These avoid double-triggering keyboard controls that are already handled
	// through SFML key events in main.cpp.
	static bool isControllerMoveLeftPressed();
	static bool isControllerMoveRightPressed();
	static bool isControllerMoveUpPressed();
	static bool isControllerMoveDownPressed();
	static bool isControllerConfirmPressed();
	static bool isControllerBackPressed();
	static bool isControllerCancelPressed();
	static bool isControllerPrimaryPressed();
	static bool isControllerSecondaryPressed();
	static bool hasControllerActivity();

	// Keyboard-only queries.  Used so per-player co-op input profiles can
	// combine "keyboard + a specific joystick" without picking up other
	// joysticks at the same time.
	static bool isKeyboardMoveLeftHeld();
	static bool isKeyboardMoveRightHeld();
	static bool isKeyboardMoveUpHeld();
	static bool isKeyboardMoveDownHeld();
	static bool isKeyboardPrimaryHeld();
	static bool isKeyboardSecondaryHeld();
	static bool isKeyboardConfirmHeld();

	// Per-joystick queries (joystickIndex is the SFML joystick index, 0..7).
	// Returns false for an out-of-range or disconnected joystick.
	static bool isJoystickConnected(unsigned int joystickIndex);

	static bool isJoystickMoveLeftHeld(unsigned int joystickIndex);
	static bool isJoystickMoveRightHeld(unsigned int joystickIndex);
	static bool isJoystickMoveUpHeld(unsigned int joystickIndex);
	static bool isJoystickMoveDownHeld(unsigned int joystickIndex);

	static bool isJoystickPrimaryHeld(unsigned int joystickIndex);
	static bool isJoystickPrimaryPressed(unsigned int joystickIndex);
	static bool isJoystickSecondaryHeld(unsigned int joystickIndex);
	static bool isJoystickSecondaryPressed(unsigned int joystickIndex);

	static bool isJoystickStartHeld(unsigned int joystickIndex);
	static bool isJoystickStartPressed(unsigned int joystickIndex);
	static bool isJoystickSelectHeld(unsigned int joystickIndex);
	static bool isJoystickSelectPressed(unsigned int joystickIndex);
};
