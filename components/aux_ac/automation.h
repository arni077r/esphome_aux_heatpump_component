#pragma once

#include "aux_ac.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"

namespace esphome {
namespace aux_ac {

// **************************************** DISPLAY ACTIONS ****************************************
template <typename... Ts>
class AirConDisplayOffAction : public Action<Ts...> {
   public:
    explicit AirConDisplayOffAction(AirCon *ac) : ac_(ac) {}

    void play(Ts... x) override { this->ac_->displayOffSequence(); }

   protected:
    AirCon *ac_;
};

template <typename... Ts>
class AirConDisplayOnAction : public Action<Ts...> {
   public:
    explicit AirConDisplayOnAction(AirCon *ac) : ac_(ac) {}

    void play(Ts... x) override { this->ac_->displayOnSequence(); }

   protected:
    AirCon *ac_;
};

// **************************************** SEND TEST PACKET ACTION ****************************************
template <typename... Ts>
class AirConSendTestPacketAction : public Action<Ts...> {
   public:
    explicit AirConSendTestPacketAction(AirCon *ac) : ac_(ac) {}
    void set_data_template(std::function<std::vector<uint8_t>(Ts...)> func) {
        this->data_func_ = func;
        this->static_ = false;
    }
    void set_data_static(const std::vector<uint8_t> &data) {
        this->data_static_ = data;
        this->static_ = true;
    }

    void play(Ts... x) override {
        if (this->static_) {
            this->ac_->sendTestPacket(this->data_static_);
        } else {
            auto val = this->data_func_(x...);
            this->ac_->sendTestPacket(val);
        }
    }

   protected:
    AirCon *ac_;
    bool static_{false};
    std::function<std::vector<uint8_t>(Ts...)> data_func_{};
    std::vector<uint8_t> data_static_{};
};

// **************************************** POWER LIMITATION ACTIONS ****************************************
template <typename... Ts>
class AirConPowerLimitationOffAction : public Action<Ts...> {
   public:
    explicit AirConPowerLimitationOffAction(AirCon *ac) : ac_(ac) {}

    void play(Ts... x) override { this->ac_->powerLimitationOffSequence(); }

   protected:
    AirCon *ac_;
};

template <typename... Ts>
class AirConPowerLimitationOnAction : public Action<Ts...> {
   public:
    AirConPowerLimitationOnAction(AirCon *ac) : ac_(ac) {}
    TEMPLATABLE_VALUE(uint8_t, value);

    void play(Ts... x) {
        this->pwr_lim_ = this->value_.value(x...);
        this->ac_->powerLimitationOnSequence(this->pwr_lim_);
    }

   protected:
    AirCon *ac_;
    uint8_t pwr_lim_;
};

}  // namespace aux_ac
}  // namespace esphome
