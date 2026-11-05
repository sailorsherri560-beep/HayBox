// N64 arcade control panel config for HayBox v3.1.0 (Raspberry Pi Pico).
//
// Buttons are mapped to HayBox's standard B0XX positions for each function
// (A = RT1, B = RF1, Start = MB1, Z = RF3, etc.), so the built-in boot holds
// line up with the labels below.
//
// Boot holds (hold while plugging in):
//   C-Down -> Pico bootloader, for flashing a new .uf2
//   Start  -> HayBox web configurator
//   Z      -> DInput (PC). Don't hold Z or Start when powering on the N64.

#include "comms/backend_init.hpp"
#include "config_defaults.hpp"
#include "core/CommunicationBackend.hpp"
#include "core/KeyboardMode.hpp"
#include "core/Persistence.hpp"
#include "core/mode_selection.hpp"
#include "core/pinout.hpp"
#include "core/state.hpp"
#include "input/DebouncedGpioButtonInput.hpp"
#include "reboot.hpp"
#include "stdlib.hpp"

#include "AnalogStickInput.hpp"
#include "N64ArcadeMode.hpp"

#include <config.pb.h>

Config config = default_config;

GpioButtonMapping button_mappings[] = {
    { BTN_RT1, 2  }, // GP2  (pin 4)  -> A           30mm Blue
    { BTN_RF1, 3  }, // GP3  (pin 5)  -> B           30mm Green
    { BTN_MB1, 4  }, // GP4  (pin 6)  -> Start       24mm Red
    { BTN_RF3, 5  }, // GP5  (pin 7)  -> Z-Trigger   30mm Gray
    { BTN_LF4, 6  }, // GP6  (pin 9)  -> L-Shoulder  24mm Gray
    { BTN_RF5, 7  }, // GP7  (pin 10) -> R-Shoulder  30mm Gray

    { BTN_RT4, 8  }, // GP8  (pin 11) -> C-Up        24mm Yellow
    { BTN_RT2, 9  }, // GP9  (pin 12) -> C-Down      24mm Yellow
    { BTN_RT3, 10 }, // GP10 (pin 14) -> C-Left      24mm Yellow
    { BTN_RT5, 11 }, // GP11 (pin 15) -> C-Right     24mm Yellow

    { BTN_RF4, 12 }, // GP12 (pin 16) -> D-Pad Up    24mm Gray
    { BTN_LF2, 13 }, // GP13 (pin 17) -> D-Pad Down  24mm Gray
    { BTN_LF3, 14 }, // GP14 (pin 19) -> D-Pad Left  24mm Gray
    { BTN_LF1, 15 }, // GP15 (pin 20) -> D-Pad Right 24mm Gray
};
const size_t button_count = sizeof(button_mappings) / sizeof(GpioButtonMapping);

DebouncedGpioButtonInput<button_count> gpio_input(button_mappings);

// Joystick pots: X wiper on GP26 (pin 31), Y wiper on GP27 (pin 32).
AnalogStickInput analog_stick;

N64ArcadeMode n64_mode;

const Pinout pinout = {
    .joybus_data = 0, // GP0 (pin 1) -> N64 data line
    .nes_data = -1,
    .nes_clock = -1,
    .nes_latch = -1,
    .mux = -1,
    .nunchuk_detect = -1,
    .nunchuk_sda = -1,
    .nunchuk_scl = -1,
};

CommunicationBackend **backends = nullptr;
size_t backend_count;
KeyboardMode *current_kb_mode = nullptr;

void setup() {
    static InputState inputs;

    // Create GPIO input source and use it to read button states for checking button holds.
    gpio_input.UpdateInputs(inputs);

    // Check bootsel button hold as early as possible for safety. (RT2 = C-Down)
    if (inputs.rt2) {
        reboot_bootloader();
    }

    // Turn on LED to indicate firmware booted.
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    gpio_put(PICO_DEFAULT_LED_PIN, 1);

    // Set up the ADC and capture the stick's resting position as centre.
    analog_stick.Init();

    // Attempt to load config, or write default config to flash if failed to load config.
    if (!persistence.LoadConfig(config)) {
        persistence.SaveConfig(config);
    }

    // Buttons and the stick are both read on core1 (see loop1), so no input
    // sources are handed to the backends here.
    static InputSource *input_sources[] = {};
    size_t input_source_count = sizeof(input_sources) / sizeof(InputSource *);

    backend_count =
        initialize_backends(backends, inputs, input_sources, input_source_count, config, pinout);

    // Always use the N64 arcade mode. The stock mode switching (select_mode) is
    // left out on purpose so a button combo can't swap you into a Melee mode.
    for (size_t i = 0; i < backend_count; i++) {
        set_mode(backends[i], &n64_mode);
    }
}

void loop() {
    for (size_t i = 0; i < backend_count; i++) {
        backends[i]->SendReport();
    }

    if (current_kb_mode != nullptr) {
        current_kb_mode->SendReport(backends[0]->GetInputs());
    }
}

/* Button and stick inputs are read from the second core */

void setup1() {
    while (backends == nullptr) {
        tight_loop_contents();
    }
}

void loop1() {
    if (backends != nullptr) {
        gpio_input.UpdateInputs(backends[0]->GetInputs());
        analog_stick.UpdateInputs(backends[0]->GetInputs());
    }
}
