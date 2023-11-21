#include "vl53l1x_sensor.h"
#include "VL53L1X.h"
#include "esphome/core/log.h"

namespace esphome {
namespace vl53l1x {

static const char* TAG = "vl53l1x";

VL53L1XSensor::VL53L1XSensor() { vl53l1x_ = new VL53L1X(); }

void VL53L1XSensor::dump_config() {
  LOG_SENSOR("", "VL53L1X", this);
  LOG_UPDATE_INTERVAL(this);
  LOG_I2C_DEVICE(this);
}

void VL53L1XSensor::setup() {
  if (!vl53l1x_->init()) {
    ESP_LOGW(TAG, "'%s' - device not found", this->name_.c_str());
  }
  vl53l1x_->set_distance_mode(static_cast<VL53L1X::DistanceMode>(distance_mode_));

  switch (distance_mode_) {
    case DistanceMode::SHORT:
      if (timing_budget_ < 20000)
        timing_budget_ = 20000;
      break;

    case DistanceMode::MEDIUM:
    case DistanceMode::LONG:
      if (timing_budget_ < 33000)
        timing_budget_ = 33000;
      break;
  }
  vl53l1x_->set_measurement_timing_budget(timing_budget_);
}

void VL53L1XSensor::update() {
  // initiate non-blocking single shot measurement
  (void) vl53l1x_->read_single(false);
  retry_count_ = retry_budget_;
}

void VL53L1XSensor::loop() {
  if (vl53l1x_->data_ready()) {
    uint16_t range_mm = vl53l1x_->read(false);  // non-blocking read

    // check measurement result
    if (vl53l1x_->ranging_data.range_status == VL53L1X::RANGE_VALID) {
      float range_m = static_cast<float>(range_mm) / 1000.0;
      ESP_LOGD(TAG, "'%s' - Got distance %.3f m", this->name_.c_str(), range_m);
      this->publish_state(range_m);
    } else if (retry_count_ > 0) {
      ESP_LOGW(TAG, "'%s' - %s --> retrying %d", this->name_.c_str(),
               VL53L1X::range_status_to_string(vl53l1x_->ranging_data.range_status), retry_count_);
      (void) vl53l1x_->read_single(false);
      retry_count_--;
    } else {
      ESP_LOGW(TAG, "'%s' - %s", this->name_.c_str(),
               VL53L1X::range_status_to_string(vl53l1x_->ranging_data.range_status));
      this->publish_state(NAN);
    }
  }
}

// Dirty hack which provides access to the TwoWire instance within I2CCompoment.
// This hack is used to minimize changes in the Pololu VL53L1X library.
class I2CComponentDummy : public i2c::I2CComponent {
 public:
  TwoWire* get_wire() const { return this->wire_; }
};

void VL53L1XSensor::set_i2c_parent(i2c::I2CComponent* parent) {
  vl53l1x_->set_bus(static_cast<I2CComponentDummy*>(parent)->get_wire());
}

void VL53L1XSensor::set_i2c_address(uint8_t address) {
  if (vl53l1x_->get_address() != address) {
    vl53l1x_->set_address(address);
  }
}

}  // namespace vl53l1x
}  // namespace esphome
