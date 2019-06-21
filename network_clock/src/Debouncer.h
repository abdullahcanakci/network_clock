#ifndef DEBOUNCER_H
#define DEBOUNCER_H

#include <Arduino.h>

class Debouncer{
    public:
    Debouncer(uint8_t buttonPin);
    void attach(std::function<void(void)> intRoutine);

    boolean isPressed();
    boolean isPressing();
    private:

};

#endif