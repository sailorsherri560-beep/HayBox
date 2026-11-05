#ifndef _CONFIG_ANALOGSTICKINPUT_HPP
#define _CONFIG_ANALOGSTICKINPUT_HPP

// Reads a two-potentiometer joystick on the Pico's ADC and stores it in the
// InputState "nunchuk" stick fields, using HayBox's convention for those fields
// (128 = centre, stored in an int8_t). N64ArcadeMode reads it from there.
//
// Written for HayBox v3.1.0.

#include "core/InputSource.hpp"
#include "core/state.hpp"

#include <hardware/adc.h>

// ---------------- Joystick tuning (adjust these after testing) ----------------

// GP26 = ADC input 0, GP27 = ADC input 1.
static constexpr uint JOY_X_GPIO = 26;
static constexpr uint JOY_Y_GPIO = 27;
static constexpr uint JOY_X_ADC = 0;
static constexpr uint JOY_Y_ADC = 1;

// Flip an axis if it moves the wrong way in game (e.g. pushing up looks down).
static constexpr bool JOY_INVERT_X = false;
static constexpr bool JOY_INVERT_Y = false;

// Full deflection sent to the console. An original N64 stick reads roughly
// +/-80, and games are tuned for that. Don't go above ~85.
static constexpr int JOY_OUTPUT_MAX = 80;

// Small zone around centre that reads as 0, so the stick doesn't drift.
// In output units (same scale as JOY_OUTPUT_MAX). Raise it if Mario creeps.
static constexpr int JOY_DEADZONE = 4;

// Starting guess for how many ADC counts (0-4095 scale) the pot moves from
// centre to full deflection. Each direction grows on its own as you push the
// stick further, so after one slow full circle the stick is calibrated.
static constexpr int JOY_INITIAL_RANGE = 300;

// --------------------------------------------------------------------------------

class AnalogStickInput : public InputSource {
  public:
    AnalogStickInput() : InputSource() {}

    // Call once at boot, with the stick at rest. Like a real N64 controller,
    // wherever the stick sits at power-on becomes centre.
    void Init() {
        adc_init();
        adc_gpio_init(JOY_X_GPIO);
        adc_gpio_init(JOY_Y_GPIO);
        Recenter();
    }

    void Recenter() {
        _center_x = ReadAveraged(JOY_X_ADC, 64);
        _center_y = ReadAveraged(JOY_Y_ADC, 64);
    }

    InputScanSpeed ScanSpeed() override { return InputScanSpeed::SLOW; }

    void UpdateInputs(InputState &inputs) override {
        // L + R + Start re-centres the stick, same as a real N64 controller.
        // (Uses the physical button positions from config.cpp.)
        bool recenter_combo = inputs.lf4 && inputs.rf5 && inputs.mb1;
        if (recenter_combo && !_recenter_was_held) {
            Recenter();
        }
        _recenter_was_held = recenter_combo;

        int x = Scale(ReadAveraged(JOY_X_ADC, 4), _center_x, _range_x_neg, _range_x_pos);
        int y = Scale(ReadAveraged(JOY_Y_ADC, 4), _center_y, _range_y_neg, _range_y_pos);
        if (JOY_INVERT_X) x = -x;
        if (JOY_INVERT_Y) y = -y;

        inputs.nunchuk_connected = true;
        inputs.nunchuk_x = static_cast<int8_t>(128 + x);
        inputs.nunchuk_y = static_cast<int8_t>(128 + y);
    }

  private:
    int _center_x = 2048;
    int _center_y = 2048;
    int _range_x_neg = JOY_INITIAL_RANGE;
    int _range_x_pos = JOY_INITIAL_RANGE;
    int _range_y_neg = JOY_INITIAL_RANGE;
    int _range_y_pos = JOY_INITIAL_RANGE;
    bool _recenter_was_held = false;

    static int ReadAveraged(uint adc_input, int samples) {
        adc_select_input(adc_input);
        int sum = 0;
        for (int i = 0; i < samples; i++) {
            sum += adc_read();
        }
        return sum / samples;
    }

    // Converts a raw ADC reading to -JOY_OUTPUT_MAX..+JOY_OUTPUT_MAX.
    static int Scale(int raw, int center, int &range_neg, int &range_pos) {
        int delta = raw - center;

        // Auto-range: remember the furthest the stick has gone each way.
        if (delta > range_pos) range_pos = delta;
        if (-delta > range_neg) range_neg = -delta;

        int value = (delta >= 0) ? (delta * JOY_OUTPUT_MAX) / range_pos
                                 : (delta * JOY_OUTPUT_MAX) / range_neg;

        // Deadzone, rescaled so movement starts smoothly just outside it.
        int magnitude = value < 0 ? -value : value;
        if (magnitude <= JOY_DEADZONE) {
            return 0;
        }
        magnitude = ((magnitude - JOY_DEADZONE) * JOY_OUTPUT_MAX) /
                    (JOY_OUTPUT_MAX - JOY_DEADZONE);
        if (magnitude > JOY_OUTPUT_MAX) magnitude = JOY_OUTPUT_MAX;
        return value < 0 ? -magnitude : magnitude;
    }
};

#endif
