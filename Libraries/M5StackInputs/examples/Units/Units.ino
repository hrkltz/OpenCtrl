#include <M5StackInputs.hpp>

// XIAO ESP32S3 GPIO numbers. See the library README for Grove wire colors.
M5Stack::Key key(7, 8);
M5Stack::Fader fader(1, 2);
M5Stack::Angle angle(3);
M5Stack::Encoder encoder(Wire, {0x40, 0, 0x70});
M5Stack::Scroll scroll(Wire, {0x40, 1, 0x70});
M5Stack::Joystick2 joystick(Wire, {0x63, 2, 0x70});

bool keyReady = false, faderReady = false, angleReady = false, encoderReady = false;

void setup() {
    Serial.begin(115200);
    Wire.begin(SDA, SCL);
    Wire.setClock(100000);
    Wire.setTimeOut(20);
    keyReady = key.Begin();
    faderReady = fader.Begin();
    angleReady = angle.Begin();
    encoderReady = encoder.Begin();
    Serial.printf("Setup: Key=%d Fader=%d Angle=%d Encoder=%d\n", keyReady, faderReady, angleReady, encoderReady);
    if (keyReady) { key.LEDs().Fill({0, 0, 8}); if (!key.LEDs().Show()) Serial.println("Key LED failed"); }
    if (faderReady) { fader.LEDs().Fill({0, 8, 0}); if (!fader.LEDs().Show()) Serial.println("Fader LEDs failed"); }
    if (encoderReady && (!encoder.LEDs().SetColor(0, {8, 0, 0}) || !encoder.LEDs().SetColor(1, {0, 8, 0})))
        Serial.println("Encoder LEDs failed");
    if (!scroll.LEDs().SetColor(0, {0, 0, 8})) Serial.println("Scroll LED failed");
    if (!joystick.SetColor({0, 8, 0})) Serial.println("Joystick LED failed");
}

void loop() {
    // Poll the mechanical key frequently so debounce and short presses stay useful.
    M5Stack::ButtonState button;
    if (keyReady && key.Read(button) && (button.pressedEdge || button.releasedEdge))
        Serial.printf("Key pressed=%d\n", button.pressed);

    static uint32_t lastRead = 0;
    if (uint32_t(millis() - lastRead) >= 100) {
        lastRead = millis();
        uint16_t raw;
        if (faderReady && fader.ReadRaw(raw))
            Serial.printf("Fader: %u mV, axis %.3f\n", raw, fader.Calibration().Normalize(raw));
        if (angleReady && angle.ReadRaw(raw))
            Serial.printf("Angle: %u mV, axis %.3f\n", raw, angle.Calibration().Normalize(raw));
        M5Stack::EncoderState rotation;
        if (encoderReady && encoder.Read(rotation))
            Serial.printf("Encoder: %ld delta=%ld button=%d\n", long(rotation.counter), long(rotation.delta), rotation.pressed);
        else Serial.println("Encoder read failed");
        if (scroll.Read(rotation))
            Serial.printf("Scroll: %ld delta=%ld button=%d\n", long(rotation.counter), long(rotation.delta), rotation.pressed);
        else Serial.println("Scroll read failed");
        M5Stack::JoystickState stick;
        if (joystick.Read(stick)) Serial.printf("Joystick: %.3f, %.3f button=%d\n", stick.x, stick.y, stick.pressed);
        else Serial.println("Joystick read failed");
    }
    delay(1);
}
