#!/usr/bin/env python3
"""
OpenCtrl / Receiver / Mouse Logger

A macOS mouse event logger using CGEventTap.
This script captures and logs mouse events in real-time.
It can optionally suppress events to prevent them from reaching the OS.

Features:
- Logs mouse movements, clicks, and scroll events
- Suppression mode: Use --catch to block events from reaching the OS
"""

import argparse
import sys
import time

import Quartz
import CoreFoundation

# Dictionary mapping Quartz event constants to readable names
EVENT_TYPE_NAMES = {
    Quartz.kCGEventMouseMoved:        "MouseMoved       ",
    Quartz.kCGEventLeftMouseDown:     "LeftMouseDown    ",
    Quartz.kCGEventLeftMouseUp:       "LeftMouseUp      ",
    Quartz.kCGEventRightMouseDown:    "RightMouseDown   ",
    Quartz.kCGEventRightMouseUp:      "RightMouseUp     ",
    Quartz.kCGEventOtherMouseDown:    "OtherMouseDown   ",
    Quartz.kCGEventOtherMouseUp:      "OtherMouseUp     ",
    Quartz.kCGEventScrollWheel:       "ScrollWheel      ",
    Quartz.kCGEventLeftMouseDragged:  "LeftMouseDragged ",
    Quartz.kCGEventRightMouseDragged: "RightMouseDragged",
    Quartz.kCGEventOtherMouseDragged: "OtherMouseDragged",
}


def get_event_type_name(event_type: int) -> str:
    """Convert a Quartz event type constant to a human-readable string."""
    return EVENT_TYPE_NAMES.get(event_type, f"EventType({event_type})")


def get_mouse_location(event) -> str:
    """Extract and format mouse coordinates from an event."""
    location = Quartz.CGEventGetLocation(event)
    return f"x={int(location.x)} y={int(location.y)}"


def get_scroll_delta(event) -> str:
    """Extract and format scroll wheel deltas from an event."""
    delta_y = Quartz.CGEventGetIntegerValueField(event, Quartz.kCGScrollWheelEventDeltaAxis1)
    delta_x = Quartz.CGEventGetIntegerValueField(event, Quartz.kCGScrollWheelEventDeltaAxis2)
    return f"dx={delta_x} dy={delta_y}"


def event_callback(proxy, event_type, event, refcon):
    """
    Callback function called for each intercepted mouse event.

    Args:
        proxy: The event tap proxy
        event_type: Type of the event (mouse movement, click, etc.)
        event: The CGEvent object
        refcon: Reference constant (used for suppression flag)

    Returns:
        The event (to pass it through) or None (to suppress it)
    """
    suppress_events = bool(refcon)

    # Handle tap timeout - re-enable if needed
    if event_type == Quartz.kCGEventTapDisabledByTimeout:
        Quartz.CGEventTapEnable(proxy, True)
        return event

    event_name = get_event_type_name(event_type)

    # Handle mouse movement and click events
    if event_type in (
        Quartz.kCGEventMouseMoved,
        Quartz.kCGEventLeftMouseDragged,
        Quartz.kCGEventRightMouseDragged,
        Quartz.kCGEventOtherMouseDragged,
        Quartz.kCGEventLeftMouseDown,
        Quartz.kCGEventLeftMouseUp,
        Quartz.kCGEventRightMouseDown,
        Quartz.kCGEventRightMouseUp,
        Quartz.kCGEventOtherMouseDown,
        Quartz.kCGEventOtherMouseUp,
    ):
        print(f"[Mouse][{event_name}] {get_mouse_location(event)}")

    # Handle scroll wheel events
    elif event_type == Quartz.kCGEventScrollWheel:
        print(f"[Mouse][{event_name}] {get_scroll_delta(event)}")

    # Return None to suppress the event, or the event to pass it through
    return None if suppress_events else event


def create_event_mask() -> int:
    """Create a bitmask for the types of events we want to intercept."""
    events_to_capture = [
        Quartz.kCGEventMouseMoved,
        Quartz.kCGEventLeftMouseDown,
        Quartz.kCGEventLeftMouseUp,
        Quartz.kCGEventRightMouseDown,
        Quartz.kCGEventRightMouseUp,
        Quartz.kCGEventOtherMouseDown,
        Quartz.kCGEventOtherMouseUp,
        Quartz.kCGEventLeftMouseDragged,
        Quartz.kCGEventRightMouseDragged,
        Quartz.kCGEventOtherMouseDragged,
        Quartz.kCGEventScrollWheel,
    ]

    mask = 0
    for event_type in events_to_capture:
        mask |= Quartz.CGEventMaskBit(event_type)
    return mask


def main() -> int:
    """Main function - parse arguments and start the mouse event logger."""
    parser = argparse.ArgumentParser(
        description="macOS mouse event logger using CGEventTap",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python mouse_logger.py              # Log mouse events without suppressing them
  python mouse_logger.py --catch      # Log and suppress mouse events (blocks OS input)
        """
    )
    parser.add_argument(
        "--catch",
        action="store_true",
        help="Suppress events (prevent them from reaching the OS)",
    )
    args = parser.parse_args()

    suppress_events = args.catch

    # Create the event tap to intercept mouse events
    event_mask = create_event_mask()
    event_tap = Quartz.CGEventTapCreate(
        Quartz.kCGHIDEventTap,        # Tap at HID level
        Quartz.kCGHeadInsertEventTap, # Insert at head of event stream
        Quartz.kCGEventTapOptionDefault,
        event_mask,
        event_callback,
        suppress_events,
    )

    if not event_tap:
        print(
            "Error: Failed to create event tap.\n"
            "Please grant Accessibility permissions in System Settings > Privacy & Security.",
            file=sys.stderr,
        )
        return 1

    # Set up the run loop to process events
    run_loop_source = Quartz.CFMachPortCreateRunLoopSource(None, event_tap, 0)
    CoreFoundation.CFRunLoopAddSource(
        CoreFoundation.CFRunLoopGetCurrent(),
        run_loop_source,
        CoreFoundation.kCFRunLoopCommonModes,
    )
    Quartz.CGEventTapEnable(event_tap, True)

    print("Mouse Event Logger started.")
    print(f"Event suppression: {'ON' if suppress_events else 'OFF'}")
    print("Press Ctrl+C to stop logging.")

    # Main event loop
    try:
        while True:
            CoreFoundation.CFRunLoopRunInMode(
                CoreFoundation.kCFRunLoopDefaultMode, 0.25, False
            )
            time.sleep(0.01)
    except KeyboardInterrupt:
        print("\nLogger stopped by interrupt.")
        return 0

    print("Logger stopped.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())