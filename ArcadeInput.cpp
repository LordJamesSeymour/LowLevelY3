#include "ArcadeInput.h"

#include <array>
#include <cmath>

namespace
{
	constexpr float AxisDeadZone = 50.f;
	constexpr unsigned int ButtonB = 0;
	constexpr unsigned int ButtonA = 1;
	constexpr unsigned int ButtonSelect = 8;
	constexpr unsigned int ButtonStart = 9;

	struct InputSnapshot
	{
		bool left = false;
		bool right = false;
		bool up = false;
		bool down = false;

		bool primary = false;
		bool secondary = false;
		bool confirm = false;
		bool back = false;
		bool cancel = false;
		bool restart = false;

		bool controllerLeft = false;
		bool controllerRight = false;
		bool controllerUp = false;
		bool controllerDown = false;
		bool controllerPrimary = false;
		bool controllerSecondary = false;
		bool controllerConfirm = false;
		bool controllerBack = false;
		bool controllerCancel = false;
		bool controllerRestart = false;
		bool controllerActivity = false;
	};

	InputSnapshot currentInput;
	InputSnapshot previousInput;

	bool buttonPressed(unsigned int joystick, unsigned int button)
	{
		if (button >= sf::Joystick::getButtonCount(joystick))
			return false;

		return sf::Joystick::isButtonPressed(joystick, button);
	}

	bool keyboardLeft()
	{
		return sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A) ||
			sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Left);
	}

	bool keyboardRight()
	{
		return sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D) ||
			sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Right);
	}

	bool keyboardUp()
	{
		return sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W) ||
			sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Up);
	}

	bool keyboardDown()
	{
		return sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S) ||
			sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Down);
	}

	bool keyboardPrimary()
	{
		return sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Space);
	}

	bool keyboardSecondary()
	{
		return sf::Keyboard::isKeyPressed(sf::Keyboard::Key::E) ||
			sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift) ||
			sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RShift);
	}

	bool keyboardConfirm()
	{
		return sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Enter) ||
			sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Space);
	}

	bool keyboardBack()
	{
		return sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Escape);
	}

	bool keyboardRestart()
	{
		return sf::Keyboard::isKeyPressed(sf::Keyboard::Key::R);
	}

	bool pressed(bool now, bool before)
	{
		return now && !before;
	}
}

void ArcadeInput::update()
{
	previousInput = currentInput;
	currentInput = {};

	sf::Joystick::update();

	for (unsigned int joystick = 0; joystick < sf::Joystick::Count; ++joystick)
	{
		if (!sf::Joystick::isConnected(joystick))
			continue;

		const float x = sf::Joystick::getAxisPosition(joystick, sf::Joystick::Axis::X);
		const float y = sf::Joystick::getAxisPosition(joystick, sf::Joystick::Axis::Y);

		const bool left = x < -AxisDeadZone;
		const bool right = x > AxisDeadZone;
		const bool up = y < -AxisDeadZone;
		const bool down = y > AxisDeadZone;

		const bool a = buttonPressed(joystick, ButtonA);
		const bool b = buttonPressed(joystick, ButtonB);
		const bool select = buttonPressed(joystick, ButtonSelect);
		const bool start = buttonPressed(joystick, ButtonStart);

		currentInput.controllerLeft = currentInput.controllerLeft || left;
		currentInput.controllerRight = currentInput.controllerRight || right;
		currentInput.controllerUp = currentInput.controllerUp || up;
		currentInput.controllerDown = currentInput.controllerDown || down;

		currentInput.controllerPrimary = currentInput.controllerPrimary || a;
		currentInput.controllerSecondary = currentInput.controllerSecondary || b;
		currentInput.controllerConfirm = currentInput.controllerConfirm || start || a;
		currentInput.controllerBack = currentInput.controllerBack || select;
		currentInput.controllerCancel = currentInput.controllerCancel || select || b;
		currentInput.controllerRestart = currentInput.controllerRestart || start;

		currentInput.controllerActivity = currentInput.controllerActivity ||
			left || right || up || down || a || b || select || start;
	}

	currentInput.left = keyboardLeft() || currentInput.controllerLeft;
	currentInput.right = keyboardRight() || currentInput.controllerRight;
	currentInput.up = keyboardUp() || currentInput.controllerUp;
	currentInput.down = keyboardDown() || currentInput.controllerDown;

	currentInput.primary = keyboardPrimary() || currentInput.controllerPrimary;
	currentInput.secondary = keyboardSecondary() || currentInput.controllerSecondary;
	currentInput.confirm = keyboardConfirm() || currentInput.controllerConfirm;
	currentInput.back = keyboardBack() || currentInput.controllerBack;
	currentInput.cancel = keyboardBack() || currentInput.controllerCancel;
	currentInput.restart = keyboardRestart() || currentInput.controllerRestart;
}

bool ArcadeInput::isMoveLeftHeld() { return currentInput.left; }
bool ArcadeInput::isMoveRightHeld() { return currentInput.right; }
bool ArcadeInput::isMoveUpHeld() { return currentInput.up; }
bool ArcadeInput::isMoveDownHeld() { return currentInput.down; }

bool ArcadeInput::isMoveLeftPressed() { return pressed(currentInput.left, previousInput.left); }
bool ArcadeInput::isMoveRightPressed() { return pressed(currentInput.right, previousInput.right); }
bool ArcadeInput::isMoveUpPressed() { return pressed(currentInput.up, previousInput.up); }
bool ArcadeInput::isMoveDownPressed() { return pressed(currentInput.down, previousInput.down); }

bool ArcadeInput::isPrimaryHeld() { return currentInput.primary; }
bool ArcadeInput::isPrimaryPressed() { return pressed(currentInput.primary, previousInput.primary); }

bool ArcadeInput::isSecondaryHeld() { return currentInput.secondary; }
bool ArcadeInput::isSecondaryPressed() { return pressed(currentInput.secondary, previousInput.secondary); }

bool ArcadeInput::isConfirmHeld() { return currentInput.confirm; }
bool ArcadeInput::isConfirmPressed() { return pressed(currentInput.confirm, previousInput.confirm); }

bool ArcadeInput::isBackHeld() { return currentInput.back; }
bool ArcadeInput::isBackPressed() { return pressed(currentInput.back, previousInput.back); }

bool ArcadeInput::isCancelHeld() { return currentInput.cancel; }
bool ArcadeInput::isCancelPressed() { return pressed(currentInput.cancel, previousInput.cancel); }

bool ArcadeInput::isRestartHeld() { return currentInput.restart; }
bool ArcadeInput::isRestartPressed() { return pressed(currentInput.restart, previousInput.restart); }

bool ArcadeInput::isControllerMoveLeftPressed() { return pressed(currentInput.controllerLeft, previousInput.controllerLeft); }
bool ArcadeInput::isControllerMoveRightPressed() { return pressed(currentInput.controllerRight, previousInput.controllerRight); }
bool ArcadeInput::isControllerMoveUpPressed() { return pressed(currentInput.controllerUp, previousInput.controllerUp); }
bool ArcadeInput::isControllerMoveDownPressed() { return pressed(currentInput.controllerDown, previousInput.controllerDown); }
bool ArcadeInput::isControllerConfirmPressed() { return pressed(currentInput.controllerConfirm, previousInput.controllerConfirm); }
bool ArcadeInput::isControllerBackPressed() { return pressed(currentInput.controllerBack, previousInput.controllerBack); }
bool ArcadeInput::isControllerCancelPressed() { return pressed(currentInput.controllerCancel, previousInput.controllerCancel); }
bool ArcadeInput::isControllerPrimaryPressed() { return pressed(currentInput.controllerPrimary, previousInput.controllerPrimary); }
bool ArcadeInput::isControllerSecondaryPressed() { return pressed(currentInput.controllerSecondary, previousInput.controllerSecondary); }
bool ArcadeInput::hasControllerActivity() { return currentInput.controllerActivity; }
