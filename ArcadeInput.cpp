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

	struct JoystickSnapshot
	{
		bool connected = false;
		bool left = false;
		bool right = false;
		bool up = false;
		bool down = false;
		bool a = false;
		bool b = false;
		bool select = false;
		bool start = false;
	};

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
		bool controllerActivity = false;

		std::array<JoystickSnapshot, sf::Joystick::Count> joysticks{};
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

	const JoystickSnapshot& currentJoystick(unsigned int joystickIndex)
	{
		static const JoystickSnapshot empty{};

		if (joystickIndex >= sf::Joystick::Count)
			return empty;

		return currentInput.joysticks[joystickIndex];
	}

	const JoystickSnapshot& previousJoystick(unsigned int joystickIndex)
	{
		static const JoystickSnapshot empty{};

		if (joystickIndex >= sf::Joystick::Count)
			return empty;

		return previousInput.joysticks[joystickIndex];
	}
}

void ArcadeInput::consumePressedState()
{
	previousInput = currentInput;
}

void ArcadeInput::update()
{
	previousInput = currentInput;
	currentInput = {};

	sf::Joystick::update();

	for (unsigned int joystick = 0; joystick < sf::Joystick::Count; ++joystick)
	{
		JoystickSnapshot& snapshot = currentInput.joysticks[joystick];

		if (!sf::Joystick::isConnected(joystick))
		{
			snapshot = {};
			continue;
		}

		snapshot.connected = true;

		const float x = sf::Joystick::getAxisPosition(joystick, sf::Joystick::Axis::X);
		const float y = sf::Joystick::getAxisPosition(joystick, sf::Joystick::Axis::Y);

		snapshot.left = x < -AxisDeadZone;
		snapshot.right = x > AxisDeadZone;
		snapshot.up = y < -AxisDeadZone;
		snapshot.down = y > AxisDeadZone;

		snapshot.a = buttonPressed(joystick, ButtonA);
		snapshot.b = buttonPressed(joystick, ButtonB);
		snapshot.select = buttonPressed(joystick, ButtonSelect);
		snapshot.start = buttonPressed(joystick, ButtonStart);

		currentInput.controllerLeft = currentInput.controllerLeft || snapshot.left;
		currentInput.controllerRight = currentInput.controllerRight || snapshot.right;
		currentInput.controllerUp = currentInput.controllerUp || snapshot.up;
		currentInput.controllerDown = currentInput.controllerDown || snapshot.down;

		currentInput.controllerPrimary = currentInput.controllerPrimary || snapshot.a;
		currentInput.controllerSecondary = currentInput.controllerSecondary || snapshot.b;
		currentInput.controllerConfirm = currentInput.controllerConfirm || snapshot.start || snapshot.a;
		currentInput.restart = currentInput.restart || snapshot.start;
		currentInput.controllerBack = currentInput.controllerBack || snapshot.select;
		currentInput.controllerCancel = currentInput.controllerCancel || snapshot.select || snapshot.b;

		currentInput.controllerActivity = currentInput.controllerActivity ||
			snapshot.left || snapshot.right || snapshot.up || snapshot.down ||
			snapshot.a || snapshot.b || snapshot.select || snapshot.start;
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
	currentInput.restart = keyboardRestart() || currentInput.restart;
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

bool ArcadeInput::isKeyboardMoveLeftHeld() { return keyboardLeft(); }
bool ArcadeInput::isKeyboardMoveRightHeld() { return keyboardRight(); }
bool ArcadeInput::isKeyboardMoveUpHeld() { return keyboardUp(); }
bool ArcadeInput::isKeyboardMoveDownHeld() { return keyboardDown(); }
bool ArcadeInput::isKeyboardPrimaryHeld() { return keyboardPrimary(); }
bool ArcadeInput::isKeyboardSecondaryHeld() { return keyboardSecondary(); }
bool ArcadeInput::isKeyboardConfirmHeld() { return keyboardConfirm(); }

bool ArcadeInput::isJoystickConnected(unsigned int joystickIndex)
{
	return currentJoystick(joystickIndex).connected;
}

bool ArcadeInput::isJoystickMoveLeftHeld(unsigned int joystickIndex) { return currentJoystick(joystickIndex).left; }
bool ArcadeInput::isJoystickMoveRightHeld(unsigned int joystickIndex) { return currentJoystick(joystickIndex).right; }
bool ArcadeInput::isJoystickMoveUpHeld(unsigned int joystickIndex) { return currentJoystick(joystickIndex).up; }
bool ArcadeInput::isJoystickMoveDownHeld(unsigned int joystickIndex) { return currentJoystick(joystickIndex).down; }

bool ArcadeInput::isJoystickPrimaryHeld(unsigned int joystickIndex) { return currentJoystick(joystickIndex).a; }
bool ArcadeInput::isJoystickPrimaryPressed(unsigned int joystickIndex)
{
	return pressed(currentJoystick(joystickIndex).a, previousJoystick(joystickIndex).a);
}

bool ArcadeInput::isJoystickSecondaryHeld(unsigned int joystickIndex) { return currentJoystick(joystickIndex).b; }
bool ArcadeInput::isJoystickSecondaryPressed(unsigned int joystickIndex)
{
	return pressed(currentJoystick(joystickIndex).b, previousJoystick(joystickIndex).b);
}

bool ArcadeInput::isJoystickStartHeld(unsigned int joystickIndex) { return currentJoystick(joystickIndex).start; }
bool ArcadeInput::isJoystickStartPressed(unsigned int joystickIndex)
{
	return pressed(currentJoystick(joystickIndex).start, previousJoystick(joystickIndex).start);
}

bool ArcadeInput::isJoystickSelectHeld(unsigned int joystickIndex) { return currentJoystick(joystickIndex).select; }
bool ArcadeInput::isJoystickSelectPressed(unsigned int joystickIndex)
{
	return pressed(currentJoystick(joystickIndex).select, previousJoystick(joystickIndex).select);
}
