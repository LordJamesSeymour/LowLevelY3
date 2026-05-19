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

	// Keyboard OR controller gameplay input.
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

	static bool isRestartHeld();      // Keyboard R
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
};
