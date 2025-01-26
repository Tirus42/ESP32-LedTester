#include <stdint.h>
#include <string.h>

#include <LedStrip_Neopixel.h>
#include <AnimationManager.h>
#include <LedBufferStorage.h>

#include <NimBLEDevice.h>

#include "VirtualLedStrip.h"

#include <BLELedController.h>
#include <GUIDefinition.h>

static constexpr int LED0_PIN = 0;
static constexpr int LED1_PIN = 1;
static constexpr uint32_t LED_COUNT = 150;

struct Config {
	uint8_t brightness = 20;
	RGBW color = COLOR_ALL;
	bool alwaysUpdate = false;

	std::function<uint32_t(uint32_t)> activeAnimationFunction;
	uint32_t nextAnimationTriggerTime = 0;

	uint32_t ledStartOffset = 0;
	uint32_t ledCount = LED_COUNT;

	void foreachLed(const std::function<void(uint32_t ledIndex, uint32_t iterationIndex)> function) const {
		uint32_t count = std::min(ledCount, LED_COUNT - ledStartOffset);

		for (ledoffset_t i = 0; i < count; ++i) {
			function(i + ledStartOffset, i);
		}
	}
} Config;

template <typename T, typename RefType = T>
struct ValueHandler : public webgui::IDataHandler<T> {
	RefType& valueRef;
	std::function<void()> onChangeFunction;

	ValueHandler(RefType& valueRef, const std::function<void()>& onChangeFunction = nullptr) :
		valueRef(valueRef),
		onChangeFunction(onChangeFunction) {}

	static std::shared_ptr<ValueHandler<T, RefType>> Create(RefType& valueRef, const std::function<void()>& onChangeFunction = nullptr) {
		return std::make_shared<ValueHandler<T, RefType>>(valueRef, onChangeFunction);
	}

	virtual T getValue() const override {
		return T(valueRef);
	}

	virtual void setValue(const T& newValue) override {
		valueRef = RefType(newValue);

		if (onChangeFunction) {
			onChangeFunction();
		}
	}
};

class BrighnessFactorLedWrapper : public ILedStripWithStorage {
	private:
		LedBufferStorage storage;
		ILedStripWithStorage& ledStrip;
		uint8_t brightnessFactor;

		void setActualLed(ledoffset_t index, RGBW color, bool flush) {
			ledStrip.setLed(index, color * (float(brightnessFactor) / 255.f), flush);
		}

	public:
		BrighnessFactorLedWrapper(ILedStripWithStorage& ledStrip) :
			storage(ledStrip.getLedCount()),
			ledStrip(ledStrip),
			brightnessFactor(0xFF) {

			ledStrip.copyTo(storage);
		}

		virtual ledoffset_t getLedCount() const override {
			return storage.getLedCount();
		}

		virtual RGBW getLed(ledoffset_t index) const override {
			return storage.getLed(index);
		}

		virtual void setLed(ledoffset_t index, RGBW color, bool flush = false) override {
			storage.setLed(index, color, flush);
			setActualLed(index, color, flush);
		}

		virtual void updateLeds() override {
			ledStrip.updateLeds();
		}

		uint8_t getBrightness() const {
			return brightnessFactor;
		}

		void setBrightness(uint8_t newBrightness, bool flush = false) {
			brightnessFactor = newBrightness;

			for (ledoffset_t i = 0; i < storage.getLedCount(); ++i) {
				setActualLed(i, storage.getLed(i), false);
			}

			if (flush) {
				updateLeds();
			}
		}
};

class VirtualDuplicateLedWrapper : public ILedStripWithStorage {
	private:
		ILedStripWithStorage& ledStrip0;
		ILedStripWithStorage& ledStrip1;

	public:
		VirtualDuplicateLedWrapper(ILedStripWithStorage& ledStrip0, ILedStripWithStorage& ledStrip1) :
			ledStrip0(ledStrip0),
			ledStrip1(ledStrip1) {}

		virtual ledoffset_t getLedCount() const override {
			return ledStrip0.getLedCount();
		}

		virtual RGBW getLed(ledoffset_t index) const override {
			return ledStrip0.getLed(index);
		}

		virtual void setLed(ledoffset_t index, RGBW color, bool flush = false) override {
			ledStrip0.setLed(index, color, flush);
			ledStrip1.setLed(index, color, flush);
		}

		virtual void updateLeds() override {
			ledStrip0.updateLeds();
			ledStrip1.updateLeds();
		}
};

static void StartAnimation(const std::function<uint32_t(uint32_t)>& animationFunction) {
	Config.activeAnimationFunction = animationFunction;
	Config.nextAnimationTriggerTime = millis();
}

static std::function<void()> UpdatedBrightnessFunction = nullptr;

void _main() {
	Serial.begin(115200);

	LedStrip_Neopixel leds1(LED_COUNT, LED0_PIN);
	LedStrip_Neopixel leds2(LED_COUNT, LED1_PIN, NEO_RGB + NEO_KHZ800);

	VirtualDuplicateLedWrapper mirrorLeds(leds1, leds2);
	BrighnessFactorLedWrapper leds(mirrorLeds);
	leds.setBrightness(Config.brightness);

	UpdatedBrightnessFunction = [&]() {
		leds.setBrightness(Config.brightness, true);
	};

	AnimationManager animationManager;

	BLELedController ble("LED tester", "LED tester", 3);

	auto animRainbowFunction = [&](uint32_t startTime) {
		uint8_t c = 0xFF;

		RGBW colors[4] = {
			RGBW(c, 0, 0, 0),
			RGBW(0, c, 0, 0),
			RGBW(0, 0, c, 0),
			RGBW(0, 0, 0, c),
		};

		ledoffset_t lastLed = std::min<ledoffset_t>(leds.getLedCount(), Config.ledStartOffset + Config.ledCount);

		for (ledoffset_t i = Config.ledStartOffset; i < lastLed; ++i) {
			animationManager.addAnimation(new FadeFromExistingAnimation(startTime +   0, 250, leds, i, colors[(i + 0) % 4]));
			animationManager.addAnimation(new FadeFromExistingAnimation(startTime + 250, 250, leds, i, colors[(i + 1) % 4]));
			animationManager.addAnimation(new FadeFromExistingAnimation(startTime + 500, 250, leds, i, colors[(i + 2) % 4]));
			animationManager.addAnimation(new FadeFromExistingAnimation(startTime + 750, 250, leds, i, colors[(i + 3) % 4]));
		}

		return 1000;
	};

	auto waveAnimationFunction = [&](uint32_t startTime) {
		Config.foreachLed([&](uint32_t ledIndex, uint32_t i) {
			uint32_t ledTime = startTime + i * 250;

			animationManager.addAnimation(new FadeFromExistingAnimation(ledTime, 250, leds, ledIndex, Config.color));
			animationManager.addAnimation(new FadeFromExistingAnimation(ledTime + 250, 400, leds, ledIndex, COLOR_OFF));
		});

		return 1000;
	};

	auto ManualColorFunction = [&]() {
		Config.activeAnimationFunction = nullptr;
		animationManager.clear();

		ledoffset_t lastLed = std::min<ledoffset_t>(leds.getLedCount(), Config.ledStartOffset + Config.ledCount);

		leds.setAll(COLOR_OFF);

		Config.foreachLed([&](uint32_t index, uint32_t /*i*/) {
			leds.setLed(index, Config.color, false);
		});

		leds.updateLeds();
	};

	/*ble.addRGBWCharacteristic("All", [&](RGBW newColor) {
		manualInput = true;
	    Config.activeAnimationFunction = nullptr;
		leds.setAll(newColor, true);
	});*/

	using namespace webgui;

	std::shared_ptr<RootElement> root = std::make_shared<RootElement>();

	root->addRange("Brightness", 0, 255, ValueHandler<int32_t, uint8_t>::Create(Config.brightness, UpdatedBrightnessFunction))->endRange();
	root->addRange("Red", 0, 255, ValueHandler<int32_t, uint8_t>::Create(Config.color.r, ManualColorFunction));
	root->addRange("Green", 0, 255, ValueHandler<int32_t, uint8_t>::Create(Config.color.g, ManualColorFunction));
	root->addRange("Blue", 0, 255, ValueHandler<int32_t, uint8_t>::Create(Config.color.b, ManualColorFunction));
	root->addRange("White", 0, 255, ValueHandler<int32_t, uint8_t>::Create(Config.color.w, ManualColorFunction));

	root->addGroup("Animation")
	->addButton("Rainbow", FunctionTrigger::Create([&] {StartAnimation(animRainbowFunction);}))->endButton()
	->addButton("Wave", FunctionTrigger::Create([&] {StartAnimation(waveAnimationFunction);}))->endButton();

	root->addGroup("Config")
	->addCheckbox("Permanent update", ValueHandler<bool>::Create(Config.alwaysUpdate))->endCheckbox()
	->addNumberFieldInt32("First LED Index", ValueHandler<int32_t, uint32_t>::Create(Config.ledStartOffset))->endNumberField()
	->addNumberFieldInt32("LED count", ValueHandler<int32_t, uint32_t>::Create(Config.ledCount))->endNumberField();

	ble.setGUI(root);

	ble.begin();

	StartAnimation(animRainbowFunction);

	while (true) {
		uint32_t currentTime = millis();

		if (Config.activeAnimationFunction) {
			if (currentTime >= Config.nextAnimationTriggerTime) {
				uint32_t animationTime = Config.activeAnimationFunction(currentTime);

				Config.nextAnimationTriggerTime = currentTime + animationTime;
			}

			animationManager.update();
		}

		if (Config.alwaysUpdate) {
			leds.updateLeds();
		} else {
			delay(10);
		}
	}
}

void setup() {
	_main();
}

void loop() {}
