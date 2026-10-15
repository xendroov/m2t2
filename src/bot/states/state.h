#pragma once

// Tüm bot state'lerinin uygulaması gereken arayüz
class IState {
public:
    virtual ~IState() = default;
    virtual void OnEnter() {}
    virtual void OnExit()  {}
    virtual void Update()  = 0;   // Her tick çağrılır (~100ms)
    virtual const char* Name() const = 0;
};
