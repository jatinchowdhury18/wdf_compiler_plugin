#pragma once

struct Params {
    static constexpr float C1_value = 1e-06;
    static constexpr float R1_value = 1e+05;
    static constexpr float R2_value = 1e+03;
    static constexpr float C2_value = 1e-06;
};

struct Impedances {
    float S1_R;
    float S1_G;
    float C1_R;
    float C1_G;
    float P1_R;
    float P1_G;
    float R1_R;
    float R1_G;
    float S2_R;
    float S2_G;
    float R2_R;
    float R2_G;
    float C2_R;
    float C2_G;
    float S2_pr;
    float P1_pr;
    float S1_pr;
};

struct State {
    float C1_z;
    float C2_z;
};

void calc_impedances(Impedances& impedances, float fs, Params params = {}) {
    [[maybe_unused]] const auto T = 1.0f / fs;

    // Computing impedance for: C1;
    impedances.C1_G = 2.0f * params.C1_value * fs;
    impedances.C1_R = 1.0f / impedances.C1_G;

    // Computing impedance for: R1;
    impedances.R1_R = params.R1_value;
    impedances.R1_G = 1.0f / impedances.R1_R;

    // Computing impedance for: R2;
    impedances.R2_R = params.R2_value;
    impedances.R2_G = 1.0f / impedances.R2_R;

    // Computing impedance for: C2;
    impedances.C2_G = 2.0f * params.C2_value * fs;
    impedances.C2_R = 1.0f / impedances.C2_G;

    // Computing impedance for: S2;
    impedances.S2_R = impedances.R2_R + impedances.C2_R;
    impedances.S2_G = 1.0f / impedances.S2_R;
    impedances.S2_pr = impedances.R2_R * impedances.S2_G;

    // Computing impedance for: P1;
    impedances.P1_G = impedances.R1_G + impedances.S2_G;
    impedances.P1_R = 1.0f / impedances.P1_G;
    impedances.P1_pr = impedances.R1_G * impedances.P1_R;

    // Computing impedance for: S1;
    impedances.S1_R = impedances.C1_R + impedances.P1_R;
    impedances.S1_G = 1.0f / impedances.S1_R;
    impedances.S1_pr = impedances.C1_R * impedances.S1_G;

}

float process(State& state, const Impedances& impedances, float Vin) {
    const auto C2_b = state.C2_z; // C2 reflected
    const auto R2_b = 0; // R2 reflected
    const auto S2_b = -(R2_b + C2_b); // S2 reflected
    const auto R1_b = 0; // R1 reflected
    const auto P1_b = S2_b - impedances.P1_pr * (S2_b - R1_b); // P1 reflected
    const auto C1_b = state.C1_z; // C1 reflected
    const auto S1_b = -(C1_b + P1_b); // S1 reflected
    const auto Vin_a = S1_b; // Vin incident
    const auto Vin_b = -Vin_a + 2 * Vin; // Vin reflected
    const auto S1_a = Vin_b; // S1 incident
    const auto C1_a = C1_b - impedances.S1_pr * (S1_a + C1_b + P1_b); // C1 incident
    const auto P1_a = -(S1_a + C1_a); // P1 incident
    state.C1_z = C1_a; // C1 state update
    const auto R1_a = P1_b - R1_b + P1_a; // R1 incident
    const auto S2_a = P1_b - S2_b + P1_a; // S2 incident
    const auto R2_a = R2_b - impedances.S2_pr * (S2_a + R2_b + C2_b); // R2 incident
    const auto C2_a = -(S2_a + R2_a); // C2 incident
    state.C2_z = C2_a; // C2 state update
    
    return (C2_a + C2_b) * 0.5f; // C2 voltage
}

