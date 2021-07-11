/*
  ==============================================================================

    Constants.h
    Created: 4 Jul 2021 9:09:50pm
    Author:  ihorv

  ==============================================================================
*/

#pragma once

class Parameters {
public:
	static const char* k_drive;
    static const char* k_bass;
    static const char* k_mid;
    static const char* k_treble;
    static const char* k_volume;
    static const char* k_output_level;
    static const char* k_input_level;
    static const char* k_low_pass_freq;
    static const char* k_high_shelf_freq;
    static const char* k_high_shelf_gain;
    static const char* k_high_shelf_q;
};

const char* Parameters::k_drive = "Drive";
const char* Parameters::k_bass = "Bass";
const char* Parameters::k_mid = "Middle";
const char* Parameters::k_treble = "Treble";
const char* Parameters::k_volume = "Volume";
const char* Parameters::k_output_level = "Output Level";
const char* Parameters::k_input_level = "Input Level";
const char* Parameters::k_low_pass_freq = "Post dist low pass frequency";
const char* Parameters::k_high_shelf_freq = "Post dist high shelf frequency";
const char* Parameters::k_high_shelf_gain = "Post dist high shelf gain";
const char* Parameters::k_high_shelf_q = "Post dist high shelf q";

// Tone Stack Values. Reference https://ccrma.stanford.edu/~dtyeh/papers/yeh06_dafx.pdf
// C1 = 0.25nF
// C2 = C3 = 20nF
// R1 = 250k
// R2 = 1M
// R3 = 25k
// R4 = 56k
constexpr double C1 = (250 * 1.0E-12);
constexpr double C2 = (20 * 1.0E-9);
constexpr double C3 = C2;
constexpr double R1 = (250.0 * 1.0E3);
constexpr double R2 = (1.0E6);
constexpr double R3 = (25 * 1.0E3);
constexpr double R4 = (56 * 1.0E3);
