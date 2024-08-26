#include "../../../../../src/medium/debug_raw.h"
#include "CoreModules/CoreHelper.hh"
#include "CoreModules/async_thread.hh"
#include "CoreModules/moduleFactory.hh"
#include "info/STS_info.hh"
#include "sampler/channel_mapping.hh"
#include "sampler/sampler_channel.hh"
#include "sampler/src/sts_filesystem.hh"
#include "sdcard.hh"
#include <atomic>
#include <chrono>

namespace MetaModule
{

class STSCore : public CoreProcessor {
public:
	using Info = STSInfo;
	using ThisCore = STSCore;
	using enum Info::Elem;

private:
	// This runs in the low-pri thread:
	AsyncThread fs_thread{[this]() {
		if (!index_is_loaded) {
			printf("Loading samples from '%s'\n", root_dir.data());

			sd.reload_disk(root_dir);

			index_loader.load_all_banks();
			chanL.reset();
			chanR.reset();

			index_is_loaded = true;
		}

		if (tm - last_tm >= 1) {
			last_tm = tm;

			chanL.fs_process(tm);
			chanR.fs_process(tm);

			// For now, disable saving the index
			// index_loader.handle_events();
		}
	}};

public:
	void update() override {
		tm = std::chrono::steady_clock::now().time_since_epoch().count() / 1'000'000LL;

		if (index_is_loaded) {
			chanL.update(tm);
			chanR.update(tm);
		} else {
			//Leds leds{index_flags, controls};
			//leds.animate_startup()
		}

		if (!started_fs_thread && id > 0) {
			fs_thread.start(id);
			started_fs_thread = true;
		}
	}

	void set_param(int param_id, float val) override {
		if (param_id == CoreHelper<Info>::param_index<AltParamStereoMode>()) {
			settings.stereo_mode = val < 0.5f;

		} else if (param_id == CoreHelper<Info>::param_index<AltParamSampleDir>()) {
			auto new_index_file = root_name(val);
			if (new_index_file != root_dir) {
				root_dir = new_index_file;
				// Disable live changing root
				// index_is_loaded = false;
			}
		}

		else if (chanL.set_param(param_id, val))
			return;
		else
			chanR.set_param(param_id, val);
	}

	void set_input(int input_id, float val) override {
		if (chanL.set_input(input_id, val))
			return;
		else
			chanR.set_input(input_id, val);
	}

	float get_output(int output_id) const override {
		//TODO: if chanR is not patched, feed mono to chan L

		if (output_id == OutL) {
			if (settings.stereo_mode)
				return 0.5f * (chanL.get_output(OutL).value_or(0.f) + chanR.get_output(OutL).value_or(0.f));
			else
				return chanL.get_output(OutL).value_or(0.f);

		} else if (output_id == OutR) {
			if (settings.stereo_mode)
				return 0.5f * (chanL.get_output(OutR).value_or(0.f) + chanR.get_output(OutR).value_or(0.f));
			else
				return chanR.get_output(OutL).value_or(0.f);

		} else if (auto found = chanL.get_output(output_id)) {
			return *found;

		} else if (auto found = chanR.get_output(output_id)) {
			return *found;
		}
		return 0.f;
	}

	void set_samplerate(float sr) override {
		chanL.set_samplerate(sr);
		chanR.set_samplerate(sr);
		ms_per_update = 1000.f / sr;
	}

	float get_led_brightness(int led_id) const override {
		if (auto found = chanL.get_led_brightness(led_id); found.has_value())
			return *found;

		else if (auto found = chanR.get_led_brightness(led_id); found.has_value())
			return *found;

		else if (led_id == CoreHelper<Info>::first_light_index<BusyLight>())
			return index_is_loaded ? 0 : 1.f;

		else
			return 0.f;
	}

	std::string_view root_name(float val) {
		unsigned index = std::clamp<unsigned>(std::round(val * 4.f), 0, 3);
		return sample_root_dirs[index];
	}

	// Boilerplate to auto-register in ModuleFactory
	// clang-format off
	static std::unique_ptr<CoreProcessor> create() { return std::make_unique<ThisCore>(); }
	static inline bool s_registered = ModuleFactory::registerModuleType(Info::slug, create, ModuleInfoView::makeView<Info>(), Info::png_filename);
	// clang-format on

private:
	SamplerKit::Sdcard sd;
	SamplerKit::SampleList samples;
	SamplerKit::BankManager banks{samples};
	SamplerKit::UserSettings settings{}; //TODO: load from file
	SamplerKit::CalibrationStorage cal_storage;

	uint32_t tm = 0;
	uint32_t last_tm = 0;
	float ms_per_update = 1000.f / 48000.f;
	bool started_fs_thread = false;

	constexpr static uint8_t OutL = CoreHelper<STSInfo>::output_index<OutLOut>();
	constexpr static uint8_t OutR = CoreHelper<STSInfo>::output_index<OutROut>();

	constexpr static STSChanMapping MappingL{
		.PitchKnob = CoreHelper<STSInfo>::param_index<PitchLKnob>(),
		.SampleKnob = CoreHelper<STSInfo>::param_index<SampleLKnob>(),
		.StartPosKnob = CoreHelper<STSInfo>::param_index<StartPos_LKnob>(),
		.LengthKnob = CoreHelper<STSInfo>::param_index<LengthLKnob>(),
		.PlayButton = CoreHelper<STSInfo>::param_index<PlayLButton>(),
		.BankButton = CoreHelper<STSInfo>::param_index<BankLButton>(),
		.ReverseButton = CoreHelper<STSInfo>::param_index<ReverseLButton>(),
		.PlayTrigIn = CoreHelper<STSInfo>::input_index<PlayTrigLIn>(),
		.VOctIn = CoreHelper<STSInfo>::input_index<_1V_OctLIn>(),
		.ReverseTrigIn = CoreHelper<STSInfo>::input_index<ReverseTrigLIn>(),
		.LengthCvIn = CoreHelper<STSInfo>::input_index<LengthCvLIn>(),
		.StartPosCvIn = CoreHelper<STSInfo>::input_index<StartPosCvLIn>(),
		.SampleCvIn = CoreHelper<STSInfo>::input_index<SampleCvLIn>(),
		.RecIn = CoreHelper<STSInfo>::input_index<LeftRecIn>(),
		.OutL = OutL,
		.OutR = OutR,
		.EndOut = CoreHelper<STSInfo>::output_index<EndOutLOut>(),
		.PlayLight = CoreHelper<STSInfo>::first_light_index<PlayLLight>(),
		.PlayButR = CoreHelper<STSInfo>::first_light_index<PlayLButton>() + 0,
		.PlayButG = CoreHelper<STSInfo>::first_light_index<PlayLButton>() + 1,
		.PlayButB = CoreHelper<STSInfo>::first_light_index<PlayLButton>() + 2,
		.RevButR = CoreHelper<STSInfo>::first_light_index<ReverseLButton>() + 0,
		.RevButG = CoreHelper<STSInfo>::first_light_index<ReverseLButton>() + 1,
		.RevButB = CoreHelper<STSInfo>::first_light_index<ReverseLButton>() + 2,
		.BankButR = CoreHelper<STSInfo>::first_light_index<BankLButton>() + 0,
		.BankButG = CoreHelper<STSInfo>::first_light_index<BankLButton>() + 1,
		.BankButB = CoreHelper<STSInfo>::first_light_index<BankLButton>() + 2,
	};

	constexpr static STSChanMapping MappingR{
		.PitchKnob = CoreHelper<STSInfo>::param_index<PitchRKnob>(),
		.SampleKnob = CoreHelper<STSInfo>::param_index<SampleRKnob>(),
		.StartPosKnob = CoreHelper<STSInfo>::param_index<StartPos_RKnob>(),
		.LengthKnob = CoreHelper<STSInfo>::param_index<LengthRKnob>(),
		.PlayButton = CoreHelper<STSInfo>::param_index<PlayRButton>(),
		.BankButton = CoreHelper<STSInfo>::param_index<BankRButton>(),
		.ReverseButton = CoreHelper<STSInfo>::param_index<ReverseRButton>(),
		.PlayTrigIn = CoreHelper<STSInfo>::input_index<PlayTrigRIn>(),
		.VOctIn = CoreHelper<STSInfo>::input_index<_1V_OctRIn>(),
		.ReverseTrigIn = CoreHelper<STSInfo>::input_index<ReverseTrigRIn>(),
		.LengthCvIn = CoreHelper<STSInfo>::input_index<LengthCvRIn>(),
		.StartPosCvIn = CoreHelper<STSInfo>::input_index<StartPosCvRIn>(),
		.SampleCvIn = CoreHelper<STSInfo>::input_index<SampleCvRIn>(),
		.RecIn = CoreHelper<STSInfo>::input_index<RightRecIn>(),
		.OutL = OutL,
		.OutR = OutR,
		.EndOut = CoreHelper<STSInfo>::output_index<EndOutROut>(),
		.PlayLight = CoreHelper<STSInfo>::first_light_index<PlayRLight>(),
		.PlayButR = CoreHelper<STSInfo>::first_light_index<PlayRButton>() + 0,
		.PlayButG = CoreHelper<STSInfo>::first_light_index<PlayRButton>() + 1,
		.PlayButB = CoreHelper<STSInfo>::first_light_index<PlayRButton>() + 2,
		.RevButR = CoreHelper<STSInfo>::first_light_index<ReverseRButton>() + 0,
		.RevButG = CoreHelper<STSInfo>::first_light_index<ReverseRButton>() + 1,
		.RevButB = CoreHelper<STSInfo>::first_light_index<ReverseRButton>() + 2,
		.BankButR = CoreHelper<STSInfo>::first_light_index<BankRButton>() + 0,
		.BankButG = CoreHelper<STSInfo>::first_light_index<BankRButton>() + 1,
		.BankButB = CoreHelper<STSInfo>::first_light_index<BankRButton>() + 2,
	};

	SamplerChannel chanL{MappingL, sd, banks, settings, cal_storage};
	SamplerChannel chanR{MappingR, sd, banks, settings, cal_storage};

	SamplerKit::Flags index_flags;
	std::atomic<bool> index_is_loaded = false;
	SamplerKit::SampleIndexLoader index_loader{sd, samples, banks, index_flags};

	static constexpr std::array<std::string_view, 4> sample_root_dirs = {
		"",
		"Samples-1/",
		"Samples-2/",
		"Samples-3/",
	};
	std::string_view root_dir = sample_root_dirs[0];
};

} // namespace MetaModule
