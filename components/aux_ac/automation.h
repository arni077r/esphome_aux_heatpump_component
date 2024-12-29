#pragma once

#include "aux_heatpump.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"

namespace esphome {
namespace aux_heatpump {


    template <typename... Ts>
    class COPowerOffAction : public Action<Ts...> {
        public:
            explicit COPowerOffAction(HeatPump* heatpump) : heatpump_(heatpump) {}

            void play(Ts... x) override { this->heatpump_->coPowerOffSequence(); }

        protected:
            HeatPump* heatpump_;
    };

    template <typename... Ts>
    class COPowerOnAction : public Action<Ts...> {
        public:
            explicit COPowerOnAction(HeatPump* heatpump) : heatpump_(heatpump) {}

            void play(Ts... x) override { this->heatpump_->coPowerOnSequence(); }

        protected:
            HeatPump* heatpump_;
    };
    template <typename... Ts>
    class CWUPowerOffAction : public Action<Ts...> {
        public:
            explicit CWUPowerOffAction(HeatPump* heatpump) : heatpump_(heatpump) {}

            void play(Ts... x) override { this->heatpump_->cwuPowerOffSequence(); }

        protected:
            HeatPump* heatpump_;
    };

    template <typename... Ts>
        class CWUPowerOnAction : public Action<Ts...> {
        public:
            explicit CWUPowerOnAction(HeatPump* heatpump) : heatpump_(heatpump) {}

            void play(Ts... x) override { this->heatpump_->cwuPowerOnSequence(); }

        protected:
            HeatPump* heatpump_;
    };
    template <typename... Ts>
    class ECOOffAction : public Action<Ts...> {
        public:
            explicit ECOOffAction(HeatPump* heatpump) : heatpump_(heatpump) {}

            void play(Ts... x) override { this->heatpump_->ecoOffSequence(); }

        protected:
            HeatPump* heatpump_;
    };

    template <typename... Ts>
        class ECOOnAction : public Action<Ts...> {
        public:
            explicit ECOOnAction(HeatPump* heatpump) : heatpump_(heatpump) {}

            void play(Ts... x) override { this->heatpump_->ecoOnSequence(); }

        protected:
            HeatPump* heatpump_;
    };

    template <typename... Ts>
    class Fast_CWUOffAction : public Action<Ts...> {
        public:
            explicit Fast_CWUOffAction(HeatPump* heatpump) : heatpump_(heatpump) {}

            void play(Ts... x) override { this->heatpump_->fast_CWUOffSequence(); }

        protected:
            HeatPump* heatpump_;
    };

    template <typename... Ts>
        class Fast_CWUOnAction : public Action<Ts...> {
        public:
            explicit Fast_CWUOnAction(HeatPump* heatpump) : heatpump_(heatpump) {}

            void play(Ts... x) override { this->heatpump_->fast_CWUOnSequence(); }

        protected:
            HeatPump* heatpump_;
    };
    // **************************************** SEND TEST PACKET ACTION ****************************************
    template <typename... Ts>
    class AirConSendTestPacketAction : public Action<Ts...> {
       public:
        explicit AirConSendTestPacketAction(HeatPump *heatpump) : heatpump_(heatpump) {}
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
                this->heatpump_->sendTestPacket(this->data_static_);
            } else {
                auto val = this->data_func_(x...);
                this->heatpump_->sendTestPacket(val);
            }
        }

       protected:
        HeatPump *heatpump_;
        bool static_{false};
        std::function<std::vector<uint8_t>(Ts...)> data_func_{};
        std::vector<uint8_t> data_static_{};
    };

}  // namespace aux_heatpump
}  // namespace esphome
