# Auto Hold

A ZMK behavior that automatically holds a key after a specified timeout period.

## Overview

> [!CAUTION]
> LLM-assisted README (overseen). 

The auto hold behavior transforms a normal key press into an automatic hold after a configurable timeout. If you release the key before the timeout, it behaves normally. If you hold it past the timeout, it becomes "auto-held" until you press any key.

## How it Works

1. **Key Press**: The wrapped behavior is triggered immediately
2. **Timeout Timer**: A timer starts for the configured duration
3. **Normal Release**: If released before timeout, the wrapped behavior releases normally
4. **Auto Hold**: If held past timeout, the key becomes auto-held
5. **Auto Release**: Pressing any key automatically releases the auto-held key

## Configuration

The auto hold behavior uses the `zmk,behavior-auto-hold` compatible binding:

### Device Tree Binding

```dts
#include <behaviors/auto_hold.dtsi>

/ {
    behaviors {
        ah_auto_hold: auto_hold {
            compatible = "zmk,behavior-auto-hold";
            #binding-cells = <2>;
            bindings = <&kp>;
            timeout-ms = <500>;
        };
    };
};
```

### Configuration Options

- **bindings**: The behavior to wrap (required)
- **timeout-ms**: Time before auto-hold activates
- **CONFIG_ZMK_BEHAVIOR_AUTO_HOLD_MAX_BEHAVIORS**: Maximum number of behaviors (default: 4)

## Usage Examples

### Basic Auto Hold Key
```dts
// In keymap
bindings = <
    &ah_auto_hold A 0  // Auto-hold 'A' key
    &ah_auto_hold B 0  // Auto-hold 'B' key
>;
```

### Modifier Auto Hold
```dts
behaviors {
    ah_mod: auto_hold_mod {
        compatible = "zmk,behavior-auto-hold";
        #binding-cells = <2>;
        bindings = <&kp>;
        timeout-ms = <300>;
    };
};

// In keymap
bindings = <
    &ah_mod LCTRL 0  // Auto-hold left Ctrl
    &ah_mod LSHIFT 0 // Auto-hold left Shift
>;
```

### Layer Auto Hold
```dts
behaviors {
    ah_layer: auto_hold_layer {
        compatible = "zmk,behavior-auto-hold";
        #binding-cells = <2>;
        bindings = <&mo>;
        timeout-ms = <750>;
    };
};

// In keymap
bindings = <
    &ah_layer 1 0  // Auto-hold layer 1
    &ah_layer 2 0  // Auto-hold layer 2
>;
```

