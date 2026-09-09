# Keyboard Mapper

A lightweight Windows keyboard remapping application built with C++ and Win32 API.

## Features

- Remap one keyboard key to another
- Global keyboard mapping
- Enable / disable keyboard mapping
- Dark Mode and Light Mode
- Follow Windows system theme
- English and Indonesian language
- System tray support
- Run automatically on Windows startup
- Export and import keyboard mapping presets
- Prevent multiple instances of the application

## Screenshot

![Keyboard Mapper](assets/Screenshot.png)

## Requirements

- Windows 10 / Windows 11
- C++ compiler with Windows SDK

## Installation

1. Download the latest `KeyboardMapper.exe` from Releases.
2. Run the application.
3. Select the source key.
4. Select the target key.
5. Click **Save**.
6. Click **START** to enable the mapping.

## Example

For example:

`Caps Lock → Escape`

After activating the mapping, pressing Caps Lock will behave like Escape.

## Build

This project uses the Windows Win32 API.

Compile `keyboardmapping.cpp` using a C++ compiler with the Windows SDK.

Example using MinGW:

```bash
g++ keyboardmapping.cpp -o KeyboardMapper.exe \
    -mwindows \
    -lcomctl32 \
    -lshell32 \
    -ldwmapi \
    -lgdi32 \
    -luxtheme \
    -ladvapi32 \
    -lcomdlg32