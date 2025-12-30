#include "CoreModules/CoreProcessor.hh"
#include "CoreModules/register_module.hh"
#include "helpers/4ms_elements.hh"
#include "midi/midi_in.hh"
#include "midi/midi_out.hh"
#include "util/math.hh"
#include <cmath>

namespace MetaModule
{

struct MidiTestInfo : ModuleInfoBase {
	static constexpr std::string_view slug{"MidiTest"};
	static constexpr std::string_view description{"MidiTest"};
	static constexpr uint32_t width_hp = 4;
	static constexpr std::string_view png_filename{"4ms/fp/Source.png"};

	using enum Coords;

	static constexpr std::array<Element, 4> Elements{{
		Knob9mm{{to_mm<72>(28.8), to_mm<72>(46.72), Center, "CC23 Tx", ""}, 0.5f, -10.0, 10.0, "v"},
		Knob9mm{{to_mm<72>(28.8), to_mm<72>(94.96), Center, "CC24 Tx", ""}, 0.5f, -10.0, 10.0, "v"},
		AnalogJackOutput4ms{{to_mm<72>(28.8), to_mm<72>(265.11), Center, "CC21 Rx", ""}},
		AnalogJackOutput4ms{{to_mm<72>(28.8), to_mm<72>(313.21), Center, "CC22 Rx", ""}},
	}};

	enum {
		CC23TxKnob,
		CC24TxKnob,
	};

	enum {
		CC21RxOut,
		CC22RxOut,
	};
};

class MidiTestCore : public CoreProcessor {
	using Info = MidiTestInfo;
	using ThisCore = MidiTestCore;

public:
	MidiTestCore() = default;

	void update(void) override {
		// MIDI CC21 and CC22 => CV out
		if (auto msg = midi_in.pop_message()) {
			if (msg->is_command<MidiCommand::ControlChange>()) {
				auto ccnum = msg->ccnum();
				if (ccnum == 21) {
					output1 = MathTools::map_value(msg->ccval(), 0, 127, 0.f, 10.f);
				} else if (ccnum == 22) {
					output2 = MathTools::map_value(msg->ccval(), 0, 127, 0.f, 10.f);
				}
			}
		}

		// Knob -> CC23 and CC24 out
		if (knob1_changed) {
			knob1_changed = false;
			MidiMessage msg{0xB0, 23, knob1};
			midi_out.push_message(msg);
		}
		if (knob2_changed) {
			knob2_changed = false;
			MidiMessage msg{0xB0, 24, knob2};
			midi_out.push_message(msg);
		}
	}

	void set_param(int const param_id, const float val) override {
		if (param_id == Info::CC23TxKnob) {
			auto t = MathTools::map_value(val, 0.0f, 1.0f, 0, 127);
			if (t != knob1) {
				knob1 = t;
				knob1_changed = true;
			}
		} else if (param_id == Info::CC24TxKnob) {
			auto t = MathTools::map_value(val, 0.0f, 1.0f, 0, 127);
			if (t != knob2) {
				knob2 = t;
				knob2_changed = true;
			}
		}
	}

	float get_param(int const param_id) const override {
		if (param_id == Info::CC23TxKnob)
			return MathTools::map_value(knob1, 0, 127, 0.f, 1.f);

		if (param_id == Info::CC24TxKnob)
			return MathTools::map_value(knob2, 0, 127, 0.f, 1.f);

		return 0;
	}

	void set_samplerate(const float sr) override {
	}

	void set_input(const int input_id, const float val) override {
	}

	float get_output(const int output_id) const override {
		if (output_id == Info::CC21RxOut)
			return output1;

		if (output_id == Info::CC22RxOut)
			return output2;

		return 0;
	}

	float get_led_brightness(int led_id) const override {
		return 0.f;
	}

	// Boilerplate to auto-register in ModuleFactory
	static inline bool s_registered = register_module<ThisCore, Info>("4msCompany");

private:
	float output1{};
	float output2{};

	uint8_t knob1{};
	uint8_t knob2{};
	bool knob1_changed{};
	bool knob2_changed{};

	MidiInput midi_in;
	MidiOutput midi_out;
};

} // namespace MetaModule
