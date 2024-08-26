#include "channel_mapping.hh"
#include "src/controls.hh"
#include "src/flags.hh"
#include "src/params.hh"
#include "src/sampler.hh"
#include <cmath>
#include <optional>

namespace MetaModule
{

class SamplerChannel {

public:
	SamplerChannel(STSChanMapping mapping,
				   SamplerKit::Sdcard &sd,
				   SamplerKit::BankManager &banks,
				   SamplerKit::UserSettings &settings,
				   SamplerKit::CalibrationStorage &cal_storage)
		: mapping{mapping}
		, params{controls, flags, settings, banks, cal_storage}
		, sampler{params, flags, sd, banks, play_buff} {
		const auto slot_size = sizeof(memory[0]) & 0xFFFFF000;
		for (unsigned i = 0; i < SamplerKit::NumSamplesPerBank; i++) {
			play_buff[i].min = reinterpret_cast<uintptr_t>(&memory[i]);
			play_buff[i].max = play_buff[i].min + slot_size;
			play_buff[i].size = slot_size;

			play_buff[i].in = play_buff[i].min;
			play_buff[i].out = play_buff[i].min;

			play_buff[i].wrapping = 0;
		}
	}

	void fs_process(uint32_t tm) {
		sampler.update_fs_thread(tm);
	}

	void update(uint32_t tm) {
		if (++out_buf_pos >= outblock.size()) {
			out_buf_pos = 0;
			params.update(tm);
#ifndef METAMODULE
			sampler.recorder.record_audio_to_buffer(inblock);
#endif
			sampler.audio.update(inblock, outblock);
		}
	}

	void set_samplerate(float sr) {
		auto new_sample_rate = uint32_t(std::round(sr));
		if (new_sample_rate != sample_rate) {
			sample_rate = new_sample_rate;
			controls.set_samplerate(sample_rate);
		}
	}

	bool set_param(unsigned param_id, float val) {
		if (param_id == mapping.PitchKnob) {
			controls.pots[SamplerKit::PitchPot] = val * 4095;

		} else if (param_id == mapping.StartPosKnob) {
			controls.pots[SamplerKit::StartPot] = val * 4095;

		} else if (param_id == mapping.LengthKnob) {
			controls.pots[SamplerKit::LengthPot] = val * 4095;

		} else if (param_id == mapping.SampleKnob) {
			controls.pots[SamplerKit::SamplePot] = val * 4095;

		} else if (param_id == mapping.PlayButton) {
			controls.play_button.sideload_set(val > 0.5f);

		} else if (param_id == mapping.BankButton) {
			controls.bank_button.sideload_set(val > 0.5f);

		} else if (param_id == mapping.ReverseButton) {
			controls.rev_button.sideload_set(val > 0.5f);

		} else
			return false; //not handled

		return true; //handled
	}

	bool set_input(unsigned input_id, float volts) {
		auto bipolar_cv = [](float cv) {
			// (-5v, +5v] => [0, 4095]
			return std::clamp<int32_t>(std::round(cv * 409.6f + 2047.f), 0, 4095);
		};
		auto unipolar_cv = [](float cv) {
			// [0v, +5v] => [0, 4095]
			return std::clamp<int32_t>(std::round(cv * 819.f), 0, 4095);
		};

		if (input_id == mapping.PlayTrigIn) {
			controls.play_jack.register_state(volts > GateThreshold);

		} else if (input_id == mapping.ReverseTrigIn) {
			controls.rev_jack.register_state(volts > GateThreshold);

		} else if (input_id == mapping.VOctIn) {
			controls.cvs[SamplerKit::PitchCV] = bipolar_cv(volts);

		} else if (input_id == mapping.LengthCvIn) {
			controls.cvs[SamplerKit::LengthCV] = unipolar_cv(volts);

		} else if (input_id == mapping.StartPosCvIn) {
			controls.cvs[SamplerKit::StartCV] = unipolar_cv(volts);

		} else if (input_id == mapping.SampleCvIn) {
			controls.cvs[SamplerKit::SampleCV] = unipolar_cv(volts);

		} else if (input_id == mapping.RecIn) {
			//not used

		} else
			return false; //not handled

		return true; //handled
	}

	std::optional<float> get_output(unsigned output_id) const {
		static constexpr int32_t kMaxValue = MathTools::ipow(2, 23);
		static constexpr float kMaxValueVolts = kMaxValue / 10.f;

		if (output_id == mapping.OutL)
			return (float)outblock[out_buf_pos].chan[0] / kMaxValueVolts;

		else if (output_id == mapping.OutR)
			return (float)outblock[out_buf_pos].chan[1] / kMaxValueVolts;

		else if (output_id == mapping.EndOut)
			return controls.end_out.sideload_get() ? 8.f : 0.f;

		else
			return 0;
	}

	std::optional<float> get_led_brightness(unsigned led_id) const {
		if (led_id == mapping.PlayLight)
			return controls.playing_led.sideload_get() ? 1.f : 0.f;

		else if (led_id == mapping.PlayButR)
			return controls.play_led_data.r / 255.f;
		else if (led_id == mapping.PlayButG)
			return controls.play_led_data.g / 255.f;
		else if (led_id == mapping.PlayButB)
			return controls.play_led_data.b / 255.f;

		else if (led_id == mapping.BankButR)
			return controls.bank_led_data.r / 255.f;
		else if (led_id == mapping.BankButG)
			return controls.bank_led_data.g / 255.f;
		else if (led_id == mapping.BankButB)
			return controls.bank_led_data.b / 255.f;

		else if (led_id == mapping.RevButR)
			return controls.rev_led_data.r / 255.f;
		else if (led_id == mapping.RevButG)
			return controls.rev_led_data.g / 255.f;
		else if (led_id == mapping.RevButB)
			return controls.rev_led_data.b / 255.f;

		else
			return std::nullopt;
	}

	void reset() {
		sampler.state.reset();
	}

private:
	STSChanMapping mapping;

	SamplerKit::Controls controls{};
	SamplerKit::Params params;
	SamplerKit::Flags flags{};

	SamplerKit::Sampler sampler;

	// Do we need these? or just pass to params
	// SamplerKit::Sdcard &sd;
	// SamplerKit::BankManager &banks;
	// SamplerKit::UserSettings &settings;

	static constexpr float GateThreshold = 1.0f;

	float sample_rate = 48000;

	static constexpr uint32_t MemorySizeBytes = 5 * 1024 * 1024;
	alignas(64) std::array<std::array<char, MemorySizeBytes / SamplerKit::NumSamplesPerBank>,
						   SamplerKit::NumSamplesPerBank> memory;

	std::array<SamplerKit::CircularBuffer, SamplerKit::NumSamplesPerBank> play_buff;

	SamplerKit::AudioStreamConf::AudioInBlock inblock;
	SamplerKit::AudioStreamConf::AudioOutBlock outblock;
	uint32_t out_buf_pos = outblock.size() - 1;
};

} // namespace MetaModule
