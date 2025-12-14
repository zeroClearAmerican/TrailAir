# TrailAir - Wireless Air Pressure Control System

A dual-ESP32 system for precise air pressure control with wireless remote operation.

## Overview

**TrailAir** consists of two cooperating devices:

- **Control Board**: Manages compressor/vent actuators, reads pressure sensor, enforces safety logic
- **Remote**: Wireless user interface with buttons and OLED display, battery-powered with sleep mode

Communication uses ESP-NOW protocol for low-latency, reliable wireless control.

## Quick Start

### Hardware

- **Board**: Seeed XIAO ESP32-C3 (both devices)
- **Control Board**: Pressure sensor (analog), compressor relay, vent valve, optional OLED
- **Remote**: 4 buttons, battery (LiPo), OLED display

### Build & Flash

Using VS Code PlatformIO tasks:

- `PIO: Build Control Board` / `PIO: Build Remote`
- Upload via USB-C

Or CLI:

```bash
# Control Board
cd pio/control_board
platformio run -t upload

# Remote
cd pio/remote
platformio run -t upload
```

### First Run

1. **Pair devices**: On remote, press Right button when disconnected to initiate pairing
2. **Set target pressure**: Use Up/Down buttons in Idle view
3. **Start seeking**: Press Right button to automatically reach target pressure
4. **Manual control**: Press Left to enter manual mode, use Up (inflate) / Down (deflate) buttons

## Architecture

### Control Board

- **TA_Controller**: Adaptive pressure seeking with learned inflation/deflation rates
- **TA_StateBoard**: UI state machine managing Idle/Manual/Seeking/Error views
- **TA_Actuators**: Hardware abstraction for compressor/vent with safety interlocks
- **TA_CommsBoard**: ESP-NOW server for receiving remote commands

### Remote (Thin Client)

- **TA_State**: Minimal state controller - forwards button events to board
- **TA_Comms**: ESP-NOW client for sending commands and receiving status
- **TA_Battery**: LiPo monitoring with critical battery protection
- **Sleep/Wake**: Auto-sleep after 5 minutes, GPIO wake on button press

### Shared Libraries (`pioLib/`)

- **TA_Protocol**: 2-byte binary protocol (no heap allocation)
- **TA_UI**: Shared UI state machine used by both devices
- **TA_Display**: OLED rendering with non-blocking animations
- **TA_Input**: Button manager with debouncing via SmartButton
- **TA_Config**: Centralized configuration structs
- **TA_Errors**: Error code definitions and descriptions
- **TA_Time**: Overflow-safe time utilities

## Project Structure

```
TrailAir/
├── pio/
│   ├── control_board/          # Control board firmware
│   │   ├── platformio.ini
│   │   ├── lib/                # Board-specific libraries
│   │   └── src/TrailAir-ControlBoard.ino
│   └── remote/                 # Remote firmware
│       ├── platformio.ini
│       ├── lib/                # Remote-specific libraries
│       └── src/TrailAir-Remote.ino
├── pioLib/                     # Shared libraries
│   ├── TA_Protocol/
│   ├── TA_UI/
│   ├── TA_Controller/
│   ├── TA_Display/
│   └── ...
├── AGENTS.md                   # Detailed architecture guide for developers
└── CODING_STANDARDS.md         # C++ style guide
```

## Development

### Testing

Run unit tests:

```bash
# Remote tests
platformio test -e native_test -d pio/remote

# Control board tests
platformio test -e native_test -d pio/control_board
```

### Code Style

Follow `CODING_STANDARDS.md`:

- PascalCase for classes/enums
- camelCase for methods/variables
- Private members with trailing underscore
- Avoid Hungarian notation

### Architecture Details

See `AGENTS.md` for comprehensive documentation on:

- Protocol message format
- UI state machine transitions
- Controller adaptive learning algorithm
- Communication layer implementation
- Testing patterns

## Key Features

- **Adaptive Learning**: Controller learns inflation/deflation rates for optimal burst timing
- **Thin Client Architecture**: Remote forwards raw button events, board runs master state machine
- **Safety Interlocks**: Compressor/vent mutually exclusive, debounced, error detection
- **Non-blocking UI**: Logo animations and display rendering don't block main loop
- **Battery Protection**: Critical battery auto-sleep, voltage monitoring
- **Auto-pairing**: One-time pairing with persistent peer storage
- **Robust Communication**: Connection tracking, reconnection logic, timeout handling

## License

Proprietary - All rights reserved
