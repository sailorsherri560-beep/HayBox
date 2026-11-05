#ifndef _CONFIG_N64ARCADEMODE_HPP
#define _CONFIG_N64ARCADEMODE_HPP

// Straight one-to-one N64 mode for the arcade panel. No Melee-style modifiers.
// Button positions match config.cpp. Written for HayBox v3.1.0.
//
// How HayBox's N64 backend reads the outputs:
//   outputs.a / b / start        -> A / B / Start
//   outputs.buttonR              -> Z
//   outputs.triggerLDigital      -> L
//   outputs.triggerRDigital      -> R
//   outputs.dpad*                -> D-Pad
//   right stick                  -> C-buttons
//   left stick                   -> analog stick

#include "core/ControllerMode.hpp"
#include "core/state.hpp"

class N64ArcadeMode : public ControllerMode {
  public:
    N64ArcadeMode() : ControllerMode() {}

  private:
    void UpdateDigitalOutputs(const InputState &inputs, OutputState &outputs) override {
        outputs.a = inputs.rt1;               // A
        outputs.b = inputs.rf1;               // B
        outputs.start = inputs.mb1;           // Start
        outputs.buttonR = inputs.rf3;         // Z
        outputs.triggerLDigital = inputs.lf4; // L
        outputs.triggerRDigital = inputs.rf5; // R

        outputs.dpadUp = inputs.rf4;
        outputs.dpadDown = inputs.lf2;
        outputs.dpadLeft = inputs.lf3;
        outputs.dpadRight = inputs.lf1;
    }

    void UpdateAnalogOutputs(const InputState &inputs, OutputState &outputs) override {
        // The left stick comes from the pots, so its four direction inputs are
        // all false here. The right stick carries the C-buttons.
        UpdateDirections(
            false,      // left stick: left
            false,      // left stick: right
            false,      // left stick: down
            false,      // left stick: up
            inputs.rt3, // C-Left
            inputs.rt5, // C-Right
            inputs.rt2, // C-Down
            inputs.rt4, // C-Up
            48,         // min     (128 - 80)
            128,        // neutral
            208,        // max     (128 + 80)
            outputs
        );

        // Analog stick from AnalogStickInput.
        if (inputs.nunchuk_connected) {
            outputs.leftStickX = static_cast<uint8_t>(inputs.nunchuk_x);
            outputs.leftStickY = static_cast<uint8_t>(inputs.nunchuk_y);
        }
    }
};

#endif
