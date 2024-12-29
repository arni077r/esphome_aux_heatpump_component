// Custom ESPHome component for AUX-based heat pumps
// Need some soldering skills
// Source code and detailed instructions are available on github: https://github.com/arni077r/esphome_aux_heatpump_component
// reworked version of the GrKoR AUX_AC component https://github.com/GrKoR/esphome_aux_heatpump_component
#pragma once

#include <stdarg.h>
//commit
#include <cinttypes>
#ifndef F
#define F(string_literal) (string_literal)  
#endif
//
#include "esphome.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
//#commit
using String = std::string;
namespace esphome
{
    namespace aux_heatpump
    {

        static const char *const TAG = "HeatPump";


// ****************************************************************************************************************************************************
// **************************************************** Packet logger configuration *******************************************************************
// ****************************************************************************************************************************************************

// The HOLMES_WORKS directive allows you to enable (true) or disable (false) the output of packets to the log.
// Moreover, disabling the output of packets will not affect the output of other data.
#define HOLMES_WORKS true

// The HOLMES_BYTE_FORMAT directive specifies the format for outputting each byte of the packet to the log in sprintf format.
// To output in hexadecimal with two digits, specify "%02X".
// To output in decimal form with three digits, specify "%03d".
#define HOLMES_BYTE_FORMAT "%02X"

// The HOLMES_FILTER_LEN directive provides filtering of packet output to the log.
// All valid packets whose body length is shorter than HOLMES_FILTER_LEN will be ignored.
// All valid packets with body length HOLMES_FILTER_LEN or more will be logged.
// All data that is not a valid packet will be logged anyway. This is for debugging purposes.
// The protocol contains packets with a body of the following lengths: 0, 1, 2, 4, 8, 15, 23
#define HOLMES_FILTER_LEN 0

// The HOLMES_DELIMITER directive allows you to specify a byte separator when outputting to the log
// For "classic" output, set " "
// For "Excel" output, specify ";"
#define HOLMES_DELIMITER " "

// The HOLMES_x_BRACKET_OPEN and HOLMES_x_BRACKET_CLOSE directives define the opening and
// closing brackets for the header and CRC.
// If you specify "" instead of brackets, there will be no brackets in the log.
#define HOLMES_HEADER_BRACKET_OPEN "["
#define HOLMES_HEADER_BRACKET_CLOSE "]"
#define HOLMES_CRC_BRACKET_OPEN "["
#define HOLMES_CRC_BRACKET_CLOSE "]"

        // ****************************************************************************************************************************************************
        // ************************************************* Constants for ESPHome integration ****************************************************************
        // ****************************************************************************************************************************************************
        class Constants
        {
        public:
            static const std::string HEATPUMP_FIRMWARE_VERSION;

            // minimum and maximum temperature in degrees Celsius, limitations of the air conditioner itself
            static const float HEATPUMP_CO_MIN_TEMPERATURE;
            static const float HEATPUMP_CO_MAX_TEMPERATURE;
            static const float HEATPUMP_CWU_MIN_TEMPERATURE;
            static const float HEATPUMP_CWU_MAX_TEMPERATURE;
            // step change of target temperature, degrees Celsius
            static const float HEATPUMP_TEMPERATURE_STEP;

            // minimum and maximum value of inverter power when setting limits
            static const uint8_t HEATPUMP_MIN_INVERTER_POWER_LIMIT;
            static const uint8_t HEATPUMP_MAX_INVERTER_POWER_LIMIT;

            // frequency of polling the air conditioner for changes in state
            // changes in parameters from the remote control are not reported to UART, so you need to request the status to stay informed
            // value in milliseconds
            static const uint32_t HEATPUMP_STATES_REQUEST_INTERVAL;

            // limits of the acceptable range of packet download timeout
            // boot timeout - after this number of milliseconds the state machine will go from
            // HPSM_RECEIVING_PACKET state to HPSM_IDLE if the packet will not be downloaded
            static const uint32_t HEATPUMP_PACKET_TIMEOUT_MAX;
            static const uint32_t HEATPUMP_PACKET_TIMEOUT_MIN;
        };

        // AUX_HEATPUMP_FIRMWARE_VERSION will be defined by the ESPHome code generator at compile time
        const std::string Constants::HEATPUMP_FIRMWARE_VERSION = AUX_HEATPUMP_FIRMWARE_VERSION;

        // params
        const float Constants::HEATPUMP_CO_MIN_TEMPERATURE = 25.0;
        const float Constants::HEATPUMP_CO_MAX_TEMPERATURE = 55.0;
        const float Constants::HEATPUMP_CWU_MIN_TEMPERATURE = 25.0;
        const float Constants::HEATPUMP_CWU_MAX_TEMPERATURE = 60.0;
        const float Constants::HEATPUMP_TEMPERATURE_STEP = 1.0;
        // AUX_HEATPUMP_MIN_INVERTER_POWER_LIMIT and AUX_HEATPUMP_MAX_INVERTER_POWER_LIMIT will be defined by the ESPHome code generator at compile time
        const uint8_t Constants::HEATPUMP_MIN_INVERTER_POWER_LIMIT = AUX_HEATPUMP_MIN_INVERTER_POWER_LIMIT;
        const uint8_t Constants::HEATPUMP_MAX_INVERTER_POWER_LIMIT = AUX_HEATPUMP_MAX_INVERTER_POWER_LIMIT;
        // package download timeout
        // According to calculations it turns out:
        // - receiving and processing character by character should not take longer than 600 ms.
        // - receiving and processing entire packets should not take longer than 150 ms.
        // We will process in batches, so 150.
        // It is not possible to stretch the reception of packets by a command queue, since the air conditioner sometimes sends
        // unsolicited information packets. Such packets will disrupt the command sequence,
        // commands will be lost. We are not protected from such a collision in any case. But the shorter the timeout,
        // the less chance of a collision.
        // Based on these considerations, the range boundaries (_MIN and _MAX values) were chosen.
        // AUX_HEATPUMP_PACKET_TIMEOUT_MAX and AUX_HEATPUMP_PACKET_TIMEOUT_MIN will be defined by the ESPHome code generator at compile time
        const uint32_t Constants::HEATPUMP_PACKET_TIMEOUT_MAX = AUX_HEATPUMP_PACKET_TIMEOUT_MAX;
        const uint32_t Constants::HEATPUMP_PACKET_TIMEOUT_MIN = AUX_HEATPUMP_PACKET_TIMEOUT_MIN;

        // ****************************************************************************************************************************************************
        // ********************************************************** MAIN STRUCTURES **************************************************************************
        // ****************************************************************************************************************************************************
        class HeatPump;

        // state machine states of the component
        enum hpsm_state : uint8_t
        {
            HPSM_IDLE = 0,         // we do nothing, we wait for something to react to
            HPSM_RECEIVING_PACKET, // we are in the process of receiving a package, no sending is possible in this state
            HPSM_PARSING_PACKET,   // we analyze the received package
            HPSM_SENDING_PACKET,   // we send the package to split
        };

// The package structure is described here:
// https://github.com/GrKoR/AUX_AC_Protocol#packet_structure
#define HEATPUMP_HEADER_SIZE 8

// the standard packet length is no more than 34 bytes
// that's why the buffer is increased
#define HEATPUMP_BUFFER_SIZE 35

// types of packages
// https://github.com/GrKoR/AUX_AC_Protocol#packet_types
#define HEATPUMP_PTYPE_PING 0x01 // ping packet
#define HEATPUMP_PTYPE_CMD 0x06  // split command
#define HEATPUMP_PTYPE_INFO 0x07 // information package
#define HEATPUMP_PTYPE_INIT 0x09 // initiating packet
#define HEATPUMP_PTYPE_UNKN 0x0b // some strange package

// types of commands
// see here: https://github.com/GrKoR/AUX_AC_Protocol#packet_type_cmd
#define HEATPUMP_CMD_SET_PARAMS 0x01   // air conditioner parameter setting command
#define HEATPUMP_CMD_STATUS_SMALL 0x11 // small air conditioner status package
#define HEATPUMP_CMD_STATUS_BIG 0x21   // Big package of conditioner status`
// TODO: Need to look at where HEATPUMP_CMD_STATUS_PERIODIC is used and change the logic.
// Today it is already known that commands in the range 0x20..0x2F are periodically sent
#define HEATPUMP_CMD_STATUS_PERIODIC 0x2C // sometimes occurs

// byte values ​​in packets
#define HEATPUMP_PACKET_START_BYTE 0xBB // The starting byte of any packet is 0xBB, I have not encountered any others
#define HEATPUMP_PACKET_ANSWER 0x80     // wifi module response indicator

        // packet header
        // https://github.com/GrKoR/AUX_AC_Protocol#packet_header
        struct packet_header_t
        {
            uint8_t start_byte = HEATPUMP_PACKET_START_BYTE;
            uint8_t _unknown1;
            uint8_t packet_type;
            uint8_t wifi;
            uint8_t ping_answer_01;
            uint8_t _unknown2;
            uint8_t body_length;
            uint8_t _unknown3;
        };

        // CRC of the packet
        // https://github.com/GrKoR/AUX_AC_Protocol#packet_crc
        union packet_crc_t
        {
            uint16_t crc16;
            uint8_t crc[2];
        };

        // stack structure[)
        struct packet_t
        {
            uint32_t msec; // millis value at the time of determining the correctness of the packet
            packet_header_t *header;
            packet_crc_t *crc;
            uint8_t *body;       // pointer to the first byte of the body; individual bits can be accessed as fields of the corresponding structure by casting pointer types
            uint8_t bytesLoaded; // number of bytes loaded into the packet, including CRC
            uint8_t data[HEATPUMP_BUFFER_SIZE];
        };

        // ping response body
        // https://github.com/GrKoR/AUX_AC_Protocol#packet_type_ping
        struct packet_ping_answer_body_t
        {
            uint8_t byte_1C = 0x00;
            uint8_t byte_27 = 0x00;
            uint8_t zero1 = 0;
            uint8_t zero2 = 0;
            uint8_t zero3 = 0;
            uint8_t zero4 = 0;
            uint8_t zero5 = 0;
            uint8_t zero6 = 0;
        };
        // body of a large information packet
        //01 21 05 00 00 01 00 5F 61 42 60 41 69 2E 00 00 00 00 00 00 00 00 [F6 CB]
        //      01 3A wyl zewn
        //      01 07 wyl zew
        //[BB 00 07 00 00 00 16 00] 01 21 01 00 56 00 1D 42 3C 41 42 53 44 20 00 00 00 44 00 00 00 00 [EF A2] 
        struct packet_big_info_body_t
        {
            // byte 0 of body (byte 8 of packet)
            uint8_t byte_01 = 0x01;//01

            // byte 1 of body (byte 9 of packet)
            uint8_t cmd_answer;//21

            // byte 2 of the body (byte 10 of the packet)
            uint8_t bi_byte2;

            // byte 3 of the body (byte 11 of the packet)
            uint8_t bi_byte3;
            // byte 4 of the body (byte 12 of the packet)
            uint8_t valve_pump_and_heatingElements_status;
            
            // byte 5 of the body (byte 13 of the packet)
            uint8_t bi_byte5;

            // byte 6 of the body (byte 14 of the packet)

            uint8_t flow_rate; // Fan PWM
            // byte 7 of the body (byte 15 of the packet) 5f-63 IBH W-OUT
            uint8_t ibhw_out_temperature;

            // byte 8 of the body (byte 16 of the packet) 61 65 Twej wody
            uint8_t temp_water_incoming; //

            // byte 9 of the body (byte 17 of the packet) 42 -34 TWejFreon
            uint8_t temp_freon_incoming; //

            // byte 10 of the body (byte 18 of the packet)60 -64 -twyjwody
            uint8_t temp_water_outgoing; //

            // byte 11 of the body (byte 19 of the packet)41 -33 -wyjFreon
            uint8_t temp_freon_outgoing; //

            // byte 12 of the body (byte 20 of the packet) 69 -73 -tWody
            uint8_t temp_dhw; // TWody; formula T - 0x20

            // byte 13 of the body (byte 21 of the packet) 2E -14 -wym/zew
            uint8_t outside_temperature_int; // T - 0x20

            // byte 14 of the body (byte 22 of the packet)
            uint8_t bi_byte14;

            // byte 15 of the body (byte 23 of the packet)
            uint8_t bi_byte15;

            // byte 16 of the body (byte 24 of the packet)
            uint8_t bi_byte16;

            // byte 17 of the body (byte 25 of the packet)
            uint8_t compressor_frequency;

            // byte 18 of the body (byte 26 of the packet)
            uint8_t bi_byte18;

            // byte 19 of the body (byte 27 of the packet)
            uint8_t bi_byte19;

            // byte 20 of the body (byte 28 of the packet)
            uint8_t bi_byte20;

            // byte 21 of the body (byte 29 of the packet)
            uint8_t bi_byte21;
        };

        // small information packet body
        // https://github.com/GrKoR/AUX_AC_Protocol#packet_cmd_11
        struct packet_small_info_body_t
        {
            // byte 8 of the packet: https://github.com/GrKoR/AUX_AC_Protocol#packet_cmd_11_b08
            uint8_t byte_01;

            // byte 9 of the packet: https://github.com/GrKoR/AUX_AC_Protocol#packet_cmd_11_b09
            uint8_t cmd_answer;

            // byte 10 of the packet: https://github.com/GrKoR/AUX_AC_Protocol#packet_cmd_11_b10

            //bool status_co : 1;
            //uint8_t something : 6;
            //bool status_dhw : 1;
            uint8_t status_1;
            // byte 11 of the packet: 
            uint8_t target_temp_co_h;
            uint8_t target_temp_co_l;
            // byte 14 of the packet: 
            uint8_t target_temp_dhw_h;
            uint8_t target_temp_dhw_l;

            // byte 15 of the packet: 
            uint8_t eco_mode; //TODO
            // byte 16 of the packet: 
            uint8_t fast_cwu; //TODO

            // byte 17 of packet: 
            uint8_t crc1; //?

            // byte 18 of the packet: 
            uint8_t crc2;//?

        };

// ****************************************************************************************************************************************************
// ******************************************************* AIR CONDITIONER OPERATING PARAMETERS **********************************************************************
// ****************************************************************************************************************************************************
// for all parameters below the option X_UNTOUCHED = 0xFF means that this command parameter should remain the one that is already set

// main operating modes of the air conditioner
#define HEATPUMP_MODE_MASK 0b11100000
        enum heatpump_mode : uint8_t
        {
            HEATPUMP_MODE_AUTO = 0x00,
            HEATPUMP_MODE_COOL = 0x20,
            HEATPUMP_MODE_DRY = 0x40,
            HEATPUMP_MODE_UNKNOWN = 0x60,
            HEATPUMP_MODE_HEAT = 0x80,
            HEATPUMP_MODE_UNKNOWN_A = 0xA0,
            HEATPUMP_MODE_FAN = 0xC0,
            HEATPUMP_MODE_UNTOUCHED = 0xFF
        };

// Turning the "Power Limit" function on and off.
#define HEATPUMP_POWLIMSTAT_MASK 0b10000000
        enum heatpump_powLim_state : uint8_t
        {
            HEATPUMP_POWLIMSTAT_OFF = 0x00,
            HEATPUMP_POWLIMSTAT_ON = 0x80,
            HEATPUMP_POWLIMSTAT_UNTOUCHED = 0xFF
        };

// power limiting masks for inverter air conditioner
#define HEATPUMP_POWLIMVAL_MASK 0b01111111
#define HEATPUMP_POWLIMVAL_UNTOUCHED 0xFF

// polecenie dla klimatyzatora
//
// WAŻNY! W kodzie zastosowano kopiowanie poleceń poprzez proste przypisanie.
// Jeśli do struktury zostaną wprowadzone wskaźniki, należy zmienić sposób kopiowania!
//

// *****************************************************************************
// structure for saving settings, specially put into a macro to be used in several places
// Brokly was made to make the wifi module behave like an IR remote control (each mode had its own temperature settings and other things)
#define HEATPUMP_COMMAND_BASE         \
    uint8_t temp_target_co;       \
    uint8_t temp_target_cwu;      \
    heatpump_mode mode;               \
    bool heatpump_turn_on_co;         \
    bool heatpump_turn_on_cwu;         \
    bool temp_target_co_matter;  \
    bool temp_target_cwu_matter

// the net size of this structure is 20 bytes, most likely due to alignment it will be larger
// Because of this technique, you need to control the size of the copied data manually
#define HEATPUMP_COMMAND_BASE_SIZE 20

        // *****************************************************************************

        struct heatpump_command_t
        {
            HEATPUMP_COMMAND_BASE;
            int8_t temp_dhw;
            int8_t temp_ibhw;
            int8_t temp_outside;
            int8_t temp_freon_outgoing;
            int8_t temp_freon_incoming;
            int8_t temp_water_incoming;
            int8_t temp_water_outgoing;
            uint8_t compressor_frequency;
            float flow_rate;
            uint8_t valve_pump_and_heatingElements_status;
            bool   cwu_on;
            bool   co_on;
            bool   eco_mode;
            bool   fast_cwu;
            uint8_t inverter_power;                  // inverter power
            bool defrost;                            // external unit defrosting mode (heat accumulation + evaporator heating)
            heatpump_powLim_state power_lim_state;         // inverter power limit status
            uint8_t  power_lim_value;                // inverter power limit value
            uint8_t  other_mode;                // inverter power limit value
            uint8_t  power_state;                // inverter power limit value
            uint8_t  all_eco_mode;                // inverter power limit value
            uint8_t bi_byte2;
            uint8_t bi_byte3;
            uint8_t bi_byte5;
            uint8_t bi_byte14;
            uint8_t bi_byte15;
            uint8_t bi_byte16;
            uint8_t bi_byte18;
            uint8_t bi_byte19;
            uint8_t bi_byte20;
            uint8_t bi_byte21;
        };

        typedef heatpump_command_t heatpump_state_t; // the current state of the condenser parameters can be stored in the same format as commands

        // Structure for storing the latest information packets received from the split in raw form
        // It is necessary until all the functionality is analyzed into the status structure.
        // We use it to check the split's response to commands (this is how we catch different versions of the protocol for communication between the wifi module and the air conditioner)
        // Each packet has an msec field. If it is zero, then the packets have not been received yet. You can also use this field to see how long ago
        // information was received from the air conditioner, a conclusion was made about the failure and an error was reported.
        struct heatpump_last_raw_data
        {
            packet_t last_small_info_packet;
            packet_t last_big_info_packet;
        };

// ****************************************************************************************************************************************************
// **************************************************** END OF AIR CONDITIONER OPERATING PARAMETERS ********************************************************************
// ****************************************************************************************************************************************************

/*********************************************************************************************************************************************
* structures and types for command sequence
*************************************************************************************************************************************************************
* Command sequence allows to execute several consecutive commands with control of received packets in response.
* If required, the value of any bytes in received packets can be controlled.
* For an incoming packet, the byte whose value is not checked must be set to HEATPUMP_SEQUENCE_ANY_BYTE.
* Control is possible only for incoming packets, outgoing ones are sent "as is".
*
* For outgoing packets, the CRC values ​​may not be calculated, the checksum will be calculated automatically.
* For incoming packets, the CRC value can also be omitted by setting the CRC bytes to HEATPUMP_SEQUENCE_ANY_BYTE,
* since the CRC check for received packets is performed automatically upon receipt.
*
* A timeout can be specified for incoming packets in a sequence. If the timeout is 0, the HEATPUMP_SEQUENCE_DEFAULT_TIMEOUT value is used.
* If no suitable packet is received within the specified time, the sequence is terminated with an error.
* Ping packets in the sequence are ignored.
*
* The pause in the sequence is specified by the timeout value of the HEATPUMP_DELAY element. No other parameters of such an element can be filled in.
*
**/
// maximum sequence length; more seemed not to be required
#define HEATPUMP_SEQUENCE_MAX_LEN 0x0F

// default incoming packet timeout in milliseconds
// If a timeout of 0 is specified for an incoming packet in a sequence, the default value is used
// If the required packet is not received within the specified time, the sequence is terminated with an error
#define HEATPUMP_SEQUENCE_DEFAULT_TIMEOUT 580 // Brokly: had to increase from 500 to 580

        enum sequence_item_type_t : uint8_t
        {
            HEATPUMP_SIT_NONE = 0x00,  // empty sequence element
            HEATPUMP_SIT_DELAY = 0x01, // pause in the sequence for the desired number of milliseconds
            HEATPUMP_SIT_FUNC = 0x02   // work item sequence
        };

        // packet type in sequence array
        // informs about what kind of packet is in the packet field of the sequence element
        enum sequence_packet_type_t : uint8_t
        {
            HEATPUMP_SPT_CLEAR = 0x00,           // empty package
            HEATPUMP_SPT_RECEIVED_PACKET = 0x01, // received package
            HEATPUMP_SPT_SENT_PACKET = 0x02      // sent package
        };

        // sequence element
        //The item_type, func, timeout and cmd fields are set manually and define the parameters for executing the sequence step.
        //The msec, packet_type and packet fields are filled by the engine when processing the sequence.
        //
        struct sequence_item_t
        {
            sequence_item_type_t item_type; // sequence element type
            bool (HeatPump::*func)();         // pointer to a function that executes a step of the sequence
            uint16_t timeout;               // acceptable timeout while waiting for a packet (applies only to incoming packets)
            heatpump_command_t cmd;               // new split state, needed to transmit commands to the air conditioner
            // ******* fields below are filled by sequence processing functions ***********
            uint32_t msec;                      // start time of the current step of the sequence (for the incoming packet and pause)
            sequence_packet_type_t packet_type; // packet type (incoming, outgoing, or not a packet at all)
            packet_t packet;                    // package data
        };
        /*****************************************************************************************************************************************************/

        class HeatPump : public esphome::Component
        {
        private:

            // time of last status request from air conditioner
            uint32_t _dataMillis;
            // frequency of updating the air conditioner status, by default HEATPUMP_STATES_REQUEST_INTERVAL

            // air conditioner type flag. inverter - true, ON/OFF - false, initial setting false
            // in this mode, the accuracy and speed of determining the real state of the system for the inverter,
            // will work but will be lower, variable is set when first receiving large packet;
            // if this variable is set, the operating mode of non-inverter air conditioner will be recognized
            // as "idle"
            bool _is_inverter = false;


            // state of a finite state machine
            hpsm_state _heatpump_state = HPSM_IDLE;

            // current state of user-defined system parameters
            heatpump_state_t _current_heatpump_state;
            // UART connection flag
            bool _hw_initialized = false;
            // pointer to UART, through which we communicate with the air conditioner
            esphome::uart::UARTComponent *_heatpump_serial;
            // UART wrappers: peek
            int peek()
            {
                uint8_t data;
                if (!_heatpump_serial->peek_byte(&data))
                    return -1;
                return data;
            }

            // UART wrappers: read
            int read()
            {
                uint8_t data;
                if (!_heatpump_serial->read_byte(&data))
                    return -1;
                return data;
            }

            // air conditioner packet exchange flag (if pings pass, then there is a connection)
            bool _has_connection = false;

            // incoming and outgoing packets
            packet_t _inPacket;
            packet_t _outPacket;

            // package for testing all sorts of crap
            packet_t _outTestPacket;

            // package download timeout, default is minimal
            uint32_t _packet_timeout = Constants::HEATPUMP_PACKET_TIMEOUT_MIN;

            // raw data of the last received large and small information packets
            heatpump_last_raw_data _last_raw_data;

            // normalization of temperature readings, bringing them into range
            int _temp_target_co_normalise(int temp)
            {
                if (temp < Constants::HEATPUMP_CO_MIN_TEMPERATURE)
                    temp = Constants::HEATPUMP_CO_MIN_TEMPERATURE;
                if (temp > Constants::HEATPUMP_CO_MAX_TEMPERATURE)
                    temp = Constants::HEATPUMP_CO_MAX_TEMPERATURE;
                return temp;
            }
            // normalization of temperature readings, bringing them into range
            int _temp_target_cwu_normalise(int temp)
            {
                if (temp < Constants::HEATPUMP_CWU_MIN_TEMPERATURE)
                    temp = Constants::HEATPUMP_CWU_MIN_TEMPERATURE;
                if (temp > Constants::HEATPUMP_CWU_MAX_TEMPERATURE)
                    temp = Constants::HEATPUMP_CWU_MAX_TEMPERATURE;
                return temp;
            }

            // normalization of inverter power limitation limit, bringing it into range
            uint8_t _power_limitation_value_normalise(uint8_t power_limitation_value)
            {
                if (power_limitation_value < Constants::HEATPUMP_MIN_INVERTER_POWER_LIMIT)
                    power_limitation_value = Constants::HEATPUMP_MIN_INVERTER_POWER_LIMIT;
                if (power_limitation_value > Constants::HEATPUMP_MAX_INVERTER_POWER_LIMIT)
                    power_limitation_value = Constants::HEATPUMP_MAX_INVERTER_POWER_LIMIT;
                return power_limitation_value;
            }

            // packet sequence current step in sequence
            sequence_item_t _sequence[HEATPUMP_SEQUENCE_MAX_LEN];
            uint8_t _sequence_current_step;

            // flag of successful execution of the initial command sequence
            bool _startupSequenceComplete = false;

            // clearing the command sequence
            void _clearSequence()
            {
                for (uint8_t i = 0; i < HEATPUMP_SEQUENCE_MAX_LEN; i++)
                {
                    _sequence[i].item_type = HEATPUMP_SIT_NONE;
                    _sequence[i].func = nullptr;
                    _sequence[i].timeout = 0;
                    _sequence[i].msec = 0;
                    _sequence[i].packet_type = HEATPUMP_SPT_CLEAR;
                    _clearPacket(&_sequence[i].packet);
                    _clearCommand(&_sequence[i].cmd);
                }
                _sequence_current_step = 0;
            }

            // checks if there are any free steps in the command sequence
            bool _hasFreeSequenceStep()
            {
                return (_getNextFreeSequenceStep() < HEATPUMP_SEQUENCE_MAX_LEN);
            }

            // returns the index of the first empty step in the command sequence
            uint8_t _getNextFreeSequenceStep()
            {
                for (size_t i = 0; i < HEATPUMP_SEQUENCE_MAX_LEN; i++)
                {
                    if (_sequence[i].item_type == HEATPUMP_SIT_NONE)
                    {
                        return i;
                    }
                }
                // if there are no free slots, then we return a value outside the range
                return HEATPUMP_SEQUENCE_MAX_LEN;
            }

            // returns the number of free steps in the sequence
            uint8_t _getFreeSequenceSpace()
            {
                return (HEATPUMP_SEQUENCE_MAX_LEN - _getNextFreeSequenceStep());
            }

            // adds a step to the command sequence
            // returns false if there is no space for the step
            bool _addSequenceStep(const sequence_item_type_t item_type, bool (HeatPump::*func)() = nullptr, heatpump_command_t *cmd = nullptr, uint16_t timeout = HEATPUMP_SEQUENCE_DEFAULT_TIMEOUT)
            {
                if (!_hasFreeSequenceStep())
                    return false; // if there is no space, then we leave
                if (item_type == HEATPUMP_SIT_NONE)
                    return false; // this is some kind of stupidity, let's leave
                if ((item_type == HEATPUMP_SIT_FUNC) && (func == nullptr))
                    return false; // a function must be passed for this type of step
                if ((item_type != HEATPUMP_SIT_DELAY) && (item_type != HEATPUMP_SIT_FUNC))
                {
                    // some unknown guy
                    _debugMsg(F("_addSequenceStep: unknown sequence item type = %u"), ESPHOME_LOG_LEVEL_DEBUG, __LINE__, item_type);
                    return false;
                }

                uint8_t step = _getNextFreeSequenceStep();

                _sequence[step].item_type = item_type;

                // if the delay is zero, then we assign the default delay
                if (timeout == 0)
                    timeout = HEATPUMP_SEQUENCE_DEFAULT_TIMEOUT;
                _sequence[step].timeout = timeout;

                _sequence[step].func = func;
                if (cmd != nullptr)
                    _sequence[step].cmd = *cmd; // since the command structure only contains simple types, you can assign like this

                return true;
            }

            // adds a step with a delay to the sequence
            bool _addSequenceDelayStep(uint16_t timeout)
            {
                return this->_addSequenceStep(HEATPUMP_SIT_DELAY, nullptr, nullptr, timeout);
            }

            // adds a functional step to the sequence
            bool _addSequenceFuncStep(bool (HeatPump::*func)(), heatpump_command_t *cmd = nullptr, uint16_t timeout = HEATPUMP_SEQUENCE_DEFAULT_TIMEOUT)
            {
                return this->_addSequenceStep(HEATPUMP_SIT_FUNC, func, cmd, timeout);
            }

            // executes all the logic of the next step of the command sequence
            void _doSequence()
            {
                if (!hasSequence())
                    return;

                // if the step is already the maximum possible
                if (_sequence_current_step >= HEATPUMP_SEQUENCE_MAX_LEN)
                {
                    // then the sequence is over, it needs to be cleared
                    // when clearing the sequence, _sequence_current_step will also be reset
                    _debugMsg(F("Sequence [step %u]: maximum step reached"), ESPHOME_LOG_LEVEL_VERBOSE, __LINE__, _sequence_current_step);
                    _clearSequence();
                    return;
                }

                // look at the type of the current element in the sequence
                switch (_sequence[_sequence_current_step].item_type)
                {
                    case HEATPUMP_SIT_FUNC:
                    {
                        // if the function pointer is empty, then we interrupt the sequence
                        if (_sequence[_sequence_current_step].func == nullptr)
                        {
                            _debugMsg(F("Sequence [step %u]: function pointer is NULL, sequence broken"), ESPHOME_LOG_LEVEL_WARN, __LINE__, _sequence_current_step);
                            _clearSequence();
                            return;
                        }

                        // save the pause start time
                        if (_sequence[_sequence_current_step].msec == 0)
                        {
                            _sequence[_sequence_current_step].msec = millis();
                            _debugMsg(F("Sequence [step %u]: step started"), ESPHOME_LOG_LEVEL_VERBOSE, __LINE__, _sequence_current_step);
                        }

                        // if no timeout is specified, take the default value
                        if (_sequence[_sequence_current_step].timeout == 0)
                            _sequence[_sequence_current_step].timeout = HEATPUMP_SEQUENCE_DEFAULT_TIMEOUT;

                        // if the time is up, then we report to the log and clear the sequence
                        if (millis() - _sequence[_sequence_current_step].msec >= _sequence[_sequence_current_step].timeout)
                        {
                            _debugMsg(F("Sequence  [step %u]: step timed out (it took %u ms instead of %u ms)"), ESPHOME_LOG_LEVEL_WARN, __LINE__, _sequence_current_step, millis() - _sequence[_sequence_current_step].msec, _sequence[_sequence_current_step].timeout);
                            _clearSequence();
                            return;
                        }

                        // you can call the function
                        // It automatically loads sent/received packets into the packet sequence
                        // and also automatically increases the sequence step counter _sequence_current_step
                        // the only exception is timeouts
                        if (!(this->*_sequence[_sequence_current_step].func)())
                        {
                            _debugMsg(F("Sequence  [step %u]: error was occur in step function"), ESPHOME_LOG_LEVEL_WARN, __LINE__, _sequence_current_step, millis() - _sequence[_sequence_current_step].msec);
                            _clearSequence();
                            return;
                        }
                        break;
                    }

                    case HEATPUMP_SIT_DELAY:
                    { // this is a pause in the sequence
                        // the pause is specified by the timeout parameter of the sequence element
                        // the start of the pause is stored in the msec parameter

                        // save the pause start time
                        if (_sequence[_sequence_current_step].msec == 0)
                        {
                            _sequence[_sequence_current_step].msec = millis();
                            _debugMsg(F("Sequence [step %u]: begin delay (%u ms)"), ESPHOME_LOG_LEVEL_VERBOSE, __LINE__, _sequence_current_step, _sequence[_sequence_current_step].timeout);
                        }

                        // if time is up, then we move on to the next step
                        if (millis() - _sequence[_sequence_current_step].msec >= _sequence[_sequence_current_step].timeout)
                        {
                            _debugMsg(F("Sequence  [step %u]: delay culminated (plan = %u ms, fact = %u ms)"), ESPHOME_LOG_LEVEL_VERBOSE, __LINE__, _sequence_current_step, _sequence[_sequence_current_step].timeout, millis() - _sequence[_sequence_current_step].msec);
                            _sequence_current_step++;
                        }
                        break;
                    }

                    case HEATPUMP_SIT_NONE: // the steps are over
                    default:          // or some garbage in the sequence
                        // I need to clear the sequence and leave
                        _debugMsg(F("Sequence [step %u]: sequence complete"), ESPHOME_LOG_LEVEL_VERBOSE, __LINE__, _sequence_current_step);
                        _clearSequence();
                        break;
                }
            }

            // fills the command structure with neutral values
            void _clearCommand(heatpump_command_t *cmd)
            {
                cmd->mode = HEATPUMP_MODE_UNTOUCHED;
                cmd->temp_target_co = 25;
                cmd->temp_target_cwu = 25;
                cmd->temp_target_co_matter = false;
                cmd->temp_target_cwu_matter = false;
                cmd->temp_ibhw = 0;
                cmd->temp_dhw = 0;
                cmd->temp_outside = 0;
                cmd->temp_freon_outgoing = 0;
                cmd->temp_freon_incoming = 0;
                cmd->temp_water_incoming = 0;
                cmd->temp_water_outgoing = 0;
                cmd->inverter_power = 0;
                cmd->defrost = false;
                cmd->eco_mode=false;
                cmd->co_on=false;
                cmd->cwu_on=false;
                cmd->fast_cwu=0;
                cmd->power_lim_state = HEATPUMP_POWLIMSTAT_UNTOUCHED;
                cmd->power_lim_value = HEATPUMP_POWLIMVAL_UNTOUCHED;
            };

            // clearing a buffer of size HEATPUMP_BUFFER_SIZE
            void _clearBuffer(uint8_t *buf)
            {
                memset(buf, 0, HEATPUMP_BUFFER_SIZE);
            }

            // clearing the package structure by pointer
            void _clearPacket(packet_t *pckt)
            {
                if (pckt == nullptr)
                {
                    _debugMsg(F("Clear packet error: pointer is NULL!"), ESPHOME_LOG_LEVEL_ERROR, __LINE__);
                    return;
                }
                pckt->crc = nullptr;
                pckt->header = (packet_header_t *)(pckt->data); // the header always starts from the beginning of the packet
                pckt->msec = 0;
                pckt->bytesLoaded = 0;
                pckt->body = nullptr;
                _clearBuffer(pckt->data);
            }

            // incoming packet cleaning
            void _clearInPacket()
            {
                _clearPacket(&_inPacket);
            }

            // clearing outgoing packet
            void _clearOutPacket()
            {
                _clearPacket(&_outPacket);
                _outPacket.header->start_byte = HEATPUMP_PACKET_START_BYTE; // for outgoing we immediately set the start byte
                _outPacket.header->wifi = HEATPUMP_PACKET_ANSWER;           // for the outgoing packet we immediately set the response flag
            }

            // copies a packet from one structure to another with correct transfer of pointers to headers, etc.
            bool _copyPacket(packet_t *dest, packet_t *src)
            {
                if (dest == nullptr)
                    return false;
                if (src == nullptr)
                    return false;

                dest->msec = src->msec;
                dest->bytesLoaded = src->bytesLoaded;
                memcpy(dest->data, src->data, HEATPUMP_BUFFER_SIZE);
                dest->header = (packet_header_t *)&dest->data;
                if (dest->header->body_length > 0)
                    dest->body = &dest->data[HEATPUMP_HEADER_SIZE];
                dest->crc = (packet_crc_t *)&dest->data[HEATPUMP_HEADER_SIZE + dest->header->body_length];

                return true;
            }

            // sets the state of a state machine
            // you can set the variable directly, but for debugging purposes it is better to do it this way
            void _setStateMachineState(hpsm_state state = HPSM_IDLE)
            {
                if (_heatpump_state == state)
                    return; // the state does not change

                _heatpump_state = state;

                switch (state)
                {
                    case HPSM_IDLE:
                        _debugMsg(F("State changed to HPSM_IDLE."), ESPHOME_LOG_LEVEL_VERBOSE, __LINE__);
                        break;

                    case HPSM_RECEIVING_PACKET:
                        _debugMsg(F("State changed to HPSM_RECEIVING_PACKET."), ESPHOME_LOG_LEVEL_VERBOSE, __LINE__);
                        break;

                    case HPSM_PARSING_PACKET:
                        _debugMsg(F("State changed to HPSM_PARSING_PACKET."), ESPHOME_LOG_LEVEL_VERBOSE, __LINE__);
                        break;

                    case HPSM_SENDING_PACKET:
                        _debugMsg(F("State changed to HPSM_SENDING_PACKET."), ESPHOME_LOG_LEVEL_VERBOSE, __LINE__);
                        break;

                    default:
                        _debugMsg(F("State changed to HPSM_IDLE by default. Given state is %02X."), ESPHOME_LOG_LEVEL_VERBOSE, __LINE__, state);
                        _heatpump_state = HPSM_IDLE;
                        break;
                }
            }

            // state machine: IDLE
            void _doIdleState()
            {
                // first you need to perform the next step of the command sequence
                _doSequence();

                // If there is no incoming data, then you can send an outgoing packet if there is one
                if (_heatpump_serial->available() == 0)
                {
                    // If there is a package to send, then it must be sent
                    // At first I thought that sending packets here is not needed now, because the HPSM_SENDING_PACKET state is set immediately in the packet parser
                    // but then I realized that we send packets not only when we need to respond, but we can also be the initiators
                    // so the send call will come in handy here
                    //if (_outPacket.msec > 0)
                    //    _setStateMachineState(HPSM_SENDING_PACKET);
                    // there's nothing else to do - let's go out
                    return;
                };

                if (this->peek() == HEATPUMP_PACKET_START_BYTE)
                {
                    // if something is already loaded into the incoming packet, then it is some kind of erroneous data or unknown packets
                    // I need to dump this info into the log
                    if (_inPacket.bytesLoaded > 0)
                    {
                        _debugMsg(F("Start byte received but there are some unparsed bytes in the buffer:"), ESPHOME_LOG_LEVEL_DEBUG, __LINE__);
                        _debugPrintPacket(&_inPacket, ESPHOME_LOG_LEVEL_DEBUG, __LINE__);
                    }
                    _clearInPacket();
                    _inPacket.msec = millis();
                    _setStateMachineState(HPSM_RECEIVING_PACKET);
                }
                else
                {
                    while (_heatpump_serial->available() > 0)
                    {
                        // if we come across the start of the packet, then we exit the while
                        // if some data was loaded into the buffer, it will be unloaded into the log when loading a new package
                        if (this->peek() == HEATPUMP_PACKET_START_BYTE)
                            break;

                        // read bytes into the incoming packet buffer
                        _inPacket.data[_inPacket.bytesLoaded] = this->read();
                        _inPacket.bytesLoaded++;

                        // if the buffer is already full, you need to dump it into the log and clear it
                        if (_inPacket.bytesLoaded >= HEATPUMP_BUFFER_SIZE)
                        {
                            _debugMsg(F("Some unparsed data on the bus:"), ESPHOME_LOG_LEVEL_DEBUG, __LINE__);
                            _debugPrintPacket(&_inPacket, ESPHOME_LOG_LEVEL_DEBUG, __LINE__);
                            _clearInPacket();
                        }
                    }
                }
            };

            // state machine state: HPSM_RECEIVING_PACKET
            void _doReceivingPacketState()
            {
                while (_heatpump_serial->available() > 0)
                {
                    // if the data packet buffer is already full, then you need to report the problem and exit
                    if (_inPacket.bytesLoaded >= HEATPUMP_BUFFER_SIZE)
                    {
                        _debugMsg(F("Receiver: packet buffer overflow!"), ESPHOME_LOG_LEVEL_WARN, __LINE__);
                        _debugPrintPacket(&_inPacket, ESPHOME_LOG_LEVEL_WARN, __LINE__);
                        _clearInPacket();
                        _setStateMachineState(HPSM_IDLE);
                        return;
                    }

                    _inPacket.data[_inPacket.bytesLoaded] = this->read();
                    _inPacket.bytesLoaded++;

                    // there is enough data for the title
                    if (_inPacket.bytesLoaded == HEATPUMP_HEADER_SIZE)
                    {
                        // the header pointer is already set when the packet is reset, you don't have to touch it
                        // _inPacket.header = (packet_header_t *)(_inPacket.data);

                        // we already know the packet size and can set pointers to the packet body and CRC
                        _inPacket.crc = (packet_crc_t *)&(_inPacket.data[HEATPUMP_HEADER_SIZE + _inPacket.header->body_length]);
                        if (_inPacket.header->body_length > 0)
                            _inPacket.body = &(_inPacket.data[HEATPUMP_HEADER_SIZE]);

                        _debugMsg(F("Header loaded: timestamp = %010u, start byte = %02X, packet type = %02X, body size = %02X"), ESPHOME_LOG_LEVEL_VERBOSE, __LINE__, _inPacket.msec, _inPacket.header->start_byte, _inPacket.header->packet_type, _inPacket.header->body_length);
                    }

                    // if all bytes of the packet are loaded, it needs to be parsed
                    // the maximum packet size will be limited by the buffer size. If such a packet is not parsed here,
                    // then on the next iteration there will be a buffer overflow error, which is at the beginning of the while loop
                    if (_inPacket.bytesLoaded == HEATPUMP_HEADER_SIZE + _inPacket.header->body_length + 2)
                    {
                        _debugMsg(F("Packet loaded: timestamp = %010u, start byte = %02X, packet type = %02X, body size = %02X, crc = [%02X, %02X]."), ESPHOME_LOG_LEVEL_VERBOSE, __LINE__, _inPacket.msec, _inPacket.header->start_byte, _inPacket.header->packet_type, _inPacket.header->body_length, _inPacket.crc->crc[0], _inPacket.crc->crc[1]);
                        _debugMsg(F("Loaded %02u bytes for a %u ms."), ESPHOME_LOG_LEVEL_VERBOSE, __LINE__, _inPacket.bytesLoaded, (millis() - _inPacket.msec));
                        _debugPrintPacket(&_inPacket, ESPHOME_LOG_LEVEL_VERBOSE, __LINE__);
                        _setStateMachineState(HPSM_PARSING_PACKET);
                        return;
                    }
                }

                // if the package is not loaded and the time is up, then you need to return to IDLE
                if (millis() - _inPacket.msec >= this->_packet_timeout)
                {
                    _debugMsg(F("Receiver: packet timed out!"), ESPHOME_LOG_LEVEL_WARN, __LINE__);
                    _debugPrintPacket(&_inPacket, ESPHOME_LOG_LEVEL_WARN, __LINE__);
                    _clearInPacket();
                    _setStateMachineState(HPSM_IDLE);
                    return;
                }
            };
            int ktory = 0;
            // state machine state: HPSM_PARSING_PACKET
            void _doParsingPacket()
            {
                
                if (!_checkCRC(&_inPacket))
                {
                    _debugMsg(F("Parser: packet CRC fail!"), ESPHOME_LOG_LEVEL_ERROR, __LINE__);
                    _debugPrintPacket(&_inPacket, ESPHOME_LOG_LEVEL_ERROR, __LINE__);
                    _clearInPacket();
                    _setStateMachineState(HPSM_IDLE);
                    return;
                }

                bool stateChangedFlag = false; // flag indicating whether the air conditioner status has changed
                uint8_t stateByte = 0;         // variable for temporary storing of current split parameters to check their changes
                float stateFloat = 0.0;        // variable for temporary storing of current split parameters to check their changes
                uint8_t stateInt = 0;
                // First, we output the received packet to the log so that it comes before the information about responses, etc.
                _debugPrintPacket(&_inPacket, ESPHOME_LOG_LEVEL_DEBUG, __LINE__);
                
                // understood package type
                switch (_inPacket.header->packet_type)
                {
                    case HEATPUMP_PTYPE_PING:
                    { // ping packet, sent by the air conditioner every 3 seconds; the module responds to it

                        if (_inPacket.header->body_length != 0)
                        { // the incoming ping packet must not have a body
                            // if there is a body, then we complain to the log
                            _debugMsg(F("Parser: ping packet should not to have body. Received one has body length %02X."), ESPHOME_LOG_LEVEL_WARN, __LINE__, _inPacket.header->body_length);
                            // we clean the package
                            _clearInPacket();
                            _setStateMachineState(HPSM_IDLE);
                            break;
                        }

                        _debugMsg(F("Parser: ping packet received"), ESPHOME_LOG_LEVEL_VERBOSE, __LINE__);
                        // we raise the flag that there is a connection with the air conditioner
                        _has_connection = true;
                        switch(ktory){
                            case 0:
                                // need to send a response to ping
                                _clearOutPacket();
                                _outPacket.msec = millis();
                                _outPacket.header->packet_type = HEATPUMP_PTYPE_PING;
                                _outPacket.header->ping_answer_01 = 0x00;
                                _outPacket.header->body_length = 8;
                                _outPacket.body = &(_outPacket.data[HEATPUMP_HEADER_SIZE]);

                                // fill the package body
                                packet_ping_answer_body_t* ping_body;
                                ping_body = (packet_ping_answer_body_t*)(_outPacket.body);
                                ping_body->byte_1C = 0x00;
                                ping_body->byte_27 = 0x00;
                                _outPacket.crc = (packet_crc_t *)&(_outPacket.data[HEATPUMP_HEADER_SIZE + _outPacket.header->body_length]);
                                _setCRC16(&_outPacket);
                                _outPacket.bytesLoaded = HEATPUMP_HEADER_SIZE + _outPacket.header->body_length + 2;
                            break;
                            case 1:
                                _fillStatusSmall();
                            break;
                            case 2:
                                _fillStatusBig();
                                ktory=-1;
                            break;
                        }
						
                        _debugMsg(F("Parser: generated ping answer. Waiting for sending."), ESPHOME_LOG_LEVEL_VERBOSE, __LINE__);
                        _setStateMachineState(HPSM_SENDING_PACKET);
                        ktory++;
                        //_setStateMachineState(HPSM_IDLE);
                        break;
                    }

                    case HEATPUMP_PTYPE_CMD:
                    { // split command; the module sends such commands when it wants something from the split
                        // split shouldn't send such commands, so we complain in the log
                        _debugMsg(F("Parser: packet type=0x06 received from HVAC. This isn't expected."), ESPHOME_LOG_LEVEL_WARN, __LINE__);
                        // we clean the package
                        _clearInPacket();
                        _setStateMachineState(HPSM_IDLE);
                        break;
                    }

                    case HEATPUMP_PTYPE_INFO:
                    { // information package
                        _debugMsg(F("Parser: status packet received"), ESPHOME_LOG_LEVEL_VERBOSE, __LINE__);
                        // we look at the type of the received packet by the second byte of the body
                        // but first let's check that such a body exists at all
                        if ((_inPacket.body == nullptr) || (_inPacket.bytesLoaded < HEATPUMP_HEADER_SIZE + 4) || (_inPacket.header->body_length < 2))
                        {
                            _debugMsg(F("Parser: packet type=0x07 without body. Error!"), ESPHOME_LOG_LEVEL_WARN, __LINE__);
                            _clearInPacket();
                            _setStateMachineState(HPSM_IDLE);
                            break;
                        }
                        // Now you can check the second byte of the packet body
                        switch (_inPacket.body[1])
                        {
                            case HEATPUMP_CMD_STATUS_SMALL:
                            { // small air conditioner status package
                                _debugMsg(F("Parser: status packet type = small"), ESPHOME_LOG_LEVEL_VERBOSE, __LINE__);
                                stateChangedFlag = false;

                                // we will access the packet body through a pointer to the structure
                                packet_small_info_body_t *small_info_body;
                                small_info_body = (packet_small_info_body_t *)(_inPacket.body);

                                stateInt=(small_info_body->status_1&0b10000000)>>7;
                                stateChangedFlag = stateChangedFlag || (_current_heatpump_state.co_on != stateInt);
                                _current_heatpump_state.co_on = stateInt;
                                
                                stateInt=(small_info_body->status_1&0b00000001);
                                stateChangedFlag = stateChangedFlag || (_current_heatpump_state.cwu_on !=stateInt);
                                _current_heatpump_state.cwu_on = stateInt;

                                int co_tmp=(small_info_body->target_temp_co_h<<8)|small_info_body->target_temp_co_l;
                                stateInt = (int)(co_tmp/10);
                                stateChangedFlag = stateChangedFlag || (_current_heatpump_state.temp_target_co != stateInt);
                                _current_heatpump_state.temp_target_co = stateInt;

                                int cwu_tmp=(small_info_body->target_temp_dhw_h<<8)|small_info_body->target_temp_dhw_l;
                                stateInt = (int)(cwu_tmp/10);
                                stateChangedFlag = stateChangedFlag || (_current_heatpump_state.temp_target_cwu != stateInt);
                                _current_heatpump_state.temp_target_cwu = stateInt;

                                stateInt=(small_info_body->eco_mode&0b00010000)>>4;
                                stateChangedFlag = stateChangedFlag || (_current_heatpump_state.eco_mode != stateInt);
                                _current_heatpump_state.eco_mode = stateInt;
                                
                                stateInt=(small_info_body->status_1);
                                stateChangedFlag = stateChangedFlag || (_current_heatpump_state.power_state != stateInt);
                                _current_heatpump_state.power_state = stateInt;
                                stateInt=(small_info_body->eco_mode);
                                stateChangedFlag = stateChangedFlag || (_current_heatpump_state.all_eco_mode != stateInt);
                                _current_heatpump_state.all_eco_mode = stateInt;

                                stateInt=(small_info_body->fast_cwu);
                                stateChangedFlag = stateChangedFlag || (_current_heatpump_state.other_mode != stateInt);
                                _current_heatpump_state.other_mode = stateInt;
                                _current_heatpump_state.fast_cwu = (stateInt&0b00000010)>>1;
                                
                                //TODO status and power limit
                                /*
                                _current_heatpump_state.mode = (heatpump_mode)small_info_body->status_co;
                                stateByte = small_info_body->mode & HEATPUMP_TEMPERATURE_UNIT_MASK;
                                stateChangedFlag = stateChangedFlag || (_current_heatpump_state.t_unit != (heatpump_temperature_unit)stateByte);
                                _current_heatpump_state.t_unit = (heatpump_temperature_unit)stateByte;

                                stateByte = small_info_body->mode & HEATPUMP_SLEEP_MASK;
                                stateChangedFlag = stateChangedFlag || (_current_heatpump_state.sleep != (heatpump_sleep)stateByte);
                                _current_heatpump_state.sleep = (heatpump_sleep)stateByte;

                                stateByte = small_info_body->status & HEATPUMP_HEALTH_MASK;
                                stateChangedFlag = stateChangedFlag || (_current_heatpump_state.health != (heatpump_health)stateByte);
                                _current_heatpump_state.health = (heatpump_health)stateByte;

                                stateByte = small_info_body->status & HEATPUMP_HEALTH_STATUS_MASK;
                                stateChangedFlag = stateChangedFlag || (_current_heatpump_state.health_status != (heatpump_health_status)stateByte);
                                _current_heatpump_state.health_status = (heatpump_health_status)stateByte;

                                stateByte = small_info_body->status & HEATPUMP_CLEAN_MASK;
                                stateChangedFlag = stateChangedFlag || (_current_heatpump_state.clean != (heatpump_clean)stateByte);
                                _current_heatpump_state.clean = (heatpump_clean)stateByte;

                                stateByte = HEATPUMP_POWLIMSTAT_ON * small_info_body->inverter_power_limitation_enable;
                                stateChangedFlag = stateChangedFlag || (_current_heatpump_state.power_lim_state != (heatpump_powLim_state)stateByte);
                                _current_heatpump_state.power_lim_state = (heatpump_powLim_state)stateByte;

                                stateByte = small_info_body->inverter_power_limitation_value;
                                stateChangedFlag = stateChangedFlag || (_current_heatpump_state.power_lim_value != stateByte);
                                _current_heatpump_state.power_lim_value = stateByte;
                                */
                                // We notify about split status change
                                

                                break;
                            }

                            case HEATPUMP_CMD_STATUS_BIG: // Big package of conditioner status
                            { // sent out in splits every 10 minutes, structure is similar to a large status packet
                                // TODO: it seems like HEATPUMP_CMD_STATUS_PERIODIC can be with other codes; for now, others will be ignored; if this is critical, it will need to be fixed
                                _debugMsg(F("Parser: status packet type = big or periodic"), ESPHOME_LOG_LEVEL_VERBOSE, __LINE__);
                                stateChangedFlag = false;

                                // we will access the packet body through a pointer to the structure
                                packet_big_info_body_t *big_info_body;
                                big_info_body = (packet_big_info_body_t *)(_inPacket.body);

                                // type of air conditioner (inverter or start stop)
                                _is_inverter = big_info_body->bi_byte2&0b00000001;

                                // outdoor/exchanger temperature according to split system version
                                stateFloat = big_info_body->ibhw_out_temperature - 0x20 ;
                                stateChangedFlag = stateChangedFlag || (_current_heatpump_state.temp_ibhw != stateFloat);
                                _current_heatpump_state.temp_ibhw = stateFloat;

                                // domestic hot water bufor temperature 
                                stateFloat = big_info_body->temp_dhw - 0x20;
                                stateChangedFlag = stateChangedFlag || (_current_heatpump_state.temp_dhw != stateFloat);
                                _current_heatpump_state.temp_dhw = stateFloat;

                                // incoming freon temperature
                                stateFloat = big_info_body->temp_freon_incoming - 0x20;
                                stateChangedFlag = stateChangedFlag || (_current_heatpump_state.temp_freon_incoming != stateFloat);
                                _current_heatpump_state.temp_freon_incoming = stateFloat;

                                // outgoing freon temperature
                                stateFloat = big_info_body->temp_freon_outgoing - 0x20;
                                stateChangedFlag = stateChangedFlag || (_current_heatpump_state.temp_freon_outgoing != stateFloat);
                                _current_heatpump_state.temp_freon_outgoing = stateFloat;

                                // incoming water temperature
                                stateFloat = big_info_body->temp_water_incoming - 0x20;
                                stateChangedFlag = stateChangedFlag || (_current_heatpump_state.temp_water_incoming != stateFloat);
                                _current_heatpump_state.temp_water_incoming = stateFloat;

                                // incoming water temperature
                                stateFloat = big_info_body->temp_water_outgoing - 0x20;
                                stateChangedFlag = stateChangedFlag || (_current_heatpump_state.temp_water_outgoing != stateFloat);
                                _current_heatpump_state.temp_water_outgoing = stateFloat;

                                // temp outside temperature
                                stateFloat = big_info_body->outside_temperature_int - 0x20;
                                stateChangedFlag = stateChangedFlag || (_current_heatpump_state.temp_outside != stateFloat);
                                _current_heatpump_state.temp_outside = stateFloat;
                                // compresor frequency
                                stateFloat = big_info_body->compressor_frequency;
                                stateChangedFlag = stateChangedFlag || (_current_heatpump_state.compressor_frequency != stateFloat);
                                _current_heatpump_state.compressor_frequency = stateFloat;
                                // water flow rate
                                stateFloat = big_info_body->flow_rate*60/1000.f;
                                stateChangedFlag = stateChangedFlag || (_current_heatpump_state.flow_rate != stateFloat);
                                _current_heatpump_state.flow_rate = stateFloat;
                                //  valve and pump statuses
                                stateFloat = big_info_body->valve_pump_and_heatingElements_status;
                                stateChangedFlag = stateChangedFlag || (_current_heatpump_state.valve_pump_and_heatingElements_status != stateFloat);
                                _current_heatpump_state.valve_pump_and_heatingElements_status = stateFloat;

                                stateFloat = big_info_body->bi_byte2;
	                            stateChangedFlag = stateChangedFlag || (_current_heatpump_state.bi_byte2 != stateFloat);
	                            _current_heatpump_state.bi_byte2 = stateFloat;
                                
                                stateFloat = big_info_body->bi_byte3;   
	                            stateChangedFlag = stateChangedFlag || (_current_heatpump_state.bi_byte3 != stateFloat);
	                            _current_heatpump_state.bi_byte3 = stateFloat;

                                stateFloat = big_info_body->bi_byte5;
	                            stateChangedFlag = stateChangedFlag || (_current_heatpump_state.bi_byte5 != stateFloat);
	                            _current_heatpump_state.bi_byte5 = stateFloat;

                                stateFloat = big_info_body->bi_byte14;
	                            stateChangedFlag = stateChangedFlag || (_current_heatpump_state.bi_byte14 != stateFloat);
	                            _current_heatpump_state.bi_byte14 = stateFloat;

                                stateFloat = big_info_body->bi_byte15;
	                            stateChangedFlag = stateChangedFlag || (_current_heatpump_state.bi_byte15 != stateFloat);
	                            _current_heatpump_state.bi_byte15 = stateFloat;

                                stateFloat = big_info_body->bi_byte16;
	                            stateChangedFlag = stateChangedFlag || (_current_heatpump_state.bi_byte16 != stateFloat);
	                            _current_heatpump_state.bi_byte16 = stateFloat;

                                stateFloat = big_info_body->bi_byte18;
	                            stateChangedFlag = stateChangedFlag || (_current_heatpump_state.bi_byte18 != stateFloat);
	                            _current_heatpump_state.bi_byte18 = stateFloat;

                                stateFloat = big_info_body->bi_byte19;
	                            stateChangedFlag = stateChangedFlag || (_current_heatpump_state.bi_byte19 != stateFloat);
	                            _current_heatpump_state.bi_byte19 = stateFloat;

                                stateFloat = big_info_body->bi_byte20;
	                            stateChangedFlag = stateChangedFlag || (_current_heatpump_state.bi_byte20 != stateFloat);
	                            _current_heatpump_state.bi_byte20 = stateFloat;

                                stateFloat = big_info_body->bi_byte21;
	                            stateChangedFlag = stateChangedFlag || (_current_heatpump_state.bi_byte21 != stateFloat);
	                            _current_heatpump_state.bi_byte21 = stateFloat;

                              

                                break;
                            }

                            case HEATPUMP_CMD_SET_PARAMS:
                            { // such a status packet is sent by the air conditioner in response to the command to set the parameters
                                // There is nothing remarkable in the body of the package
                                // in bytes 2 and 3 of the body, the CRC of the packet of the received command is transmitted, to which the split responds
                                // I decided not to check or control this point here.
                                // The correct setting of the parameters can be determined by requesting the status of the air conditioner immediately after receiving this command from the air conditioner
                                // at the moment the check is done in the sequences mechanism
                                break;
                            }
                            case HEATPUMP_CMD_STATUS_PERIODIC:
                                break;
                            default:
                                _debugMsg(F("Parser: status packet type = unknown (%02X)"), ESPHOME_LOG_LEVEL_WARN, __LINE__, _inPacket.body[1]);
                                break;
                        }
                        if (stateChangedFlag)
                            publish_all_states();
                        _setStateMachineState(HPSM_IDLE);
                        break;
                    }

                    case HEATPUMP_PTYPE_INIT: // initiating packet; sent by split if the HEALTH button on the remote control is pressed 8 times; I haven't figured out how it works.
                    case HEATPUMP_PTYPE_UNKN: // some strange packet sent by the remote control upon initiation and sometimes when power is turned on... I haven't figured out how it works and what it's for, the split doesn't seem to react to it
                    default:
                        // ignore. For our case these packages are not important
                        _setStateMachineState(HPSM_IDLE);
                        break;
                }

                // if there is a sequence of commands, then it is necessary to work out the sequence check
                if (hasSequence())
                    _doSequence();

                // After parsing the incoming packet, it must be cleaned
                _clearInPacket();
            }

            // state machine state: HPSM_SENDING_PACKET
            void _doSendingPacketState()
            {
                // if there is no outgoing packet, then exit
                if ((_outPacket.msec == 0) || (_outPacket.crc == nullptr) || (_outPacket.bytesLoaded == 0))
                {
                    _debugMsg(F("Sender: no packet to send.%d %d %d ;"), ESPHOME_LOG_LEVEL_DEBUG, __LINE__, _outPacket.msec, _outPacket.crc, _outPacket.bytesLoaded);
                    _setStateMachineState(HPSM_IDLE);
                    return;
                }

                _debugMsg(F("Sender: sending packet."), ESPHOME_LOG_LEVEL_VERBOSE, __LINE__);
                
                _heatpump_serial->write_array(_outPacket.data, _outPacket.bytesLoaded);
                _heatpump_serial->flush();
                
                _debugPrintPacket(&_outPacket, ESPHOME_LOG_LEVEL_DEBUG, __LINE__);
                _debugMsg(F("Sender: %u bytes sent (%u ms)."), ESPHOME_LOG_LEVEL_VERBOSE, __LINE__, _outPacket.bytesLoaded, millis() - _outPacket.msec);
                _clearOutPacket();

                _setStateMachineState(HPSM_IDLE);
            };

            // output debug information to the log
            //
            // dbgLevel - message level, defined in ESPHome. Using it, you can control the completeness of the information in the log from ESPHome.
            //msg - message output to the log
            //line - line on which the call occurred (useful for debugging)
            //
            void _debugMsg(const String &msg, uint8_t dbgLevel = ESPHOME_LOG_LEVEL_DEBUG, unsigned int line = 0, ...)
            {
                if (dbgLevel <= ESPHOME_LOG_LEVEL_NONE)
                    dbgLevel = ESPHOME_LOG_LEVEL_NONE;
                if (dbgLevel > ESPHOME_LOG_LEVEL_VERY_VERBOSE)
                    dbgLevel = ESPHOME_LOG_LEVEL_VERY_VERBOSE;

                if (line == 0)
                    line = __LINE__; // if the line is not passed, we take the current line

                va_list vl;
                va_start(vl, line);
                esp_log_vprintf_(dbgLevel, TAG, line, msg.c_str(), vl);
                va_end(vl);
            }

            //output packet data to the log for debugging
            //
            //dbgLevel - message level, defined in ESPHome. Using it, you can control the completeness of the log information from ESPHome.
            //packet - pointer to the packet to output;
            //if the pointer to crc is nullptr or the first byte in the buffer is not HEATPUMP_PACKET_START_BYTE, then we assume that a broken packet was transmitted
            //or not a packet at all. For this, we output only the byte array.
            //For a normal packet, the data is output with formatting.
            //line - the line on which the call occurred (convenient for debugging)
            //
            void _debugPrintPacket(packet_t *packet, uint8_t dbgLevel = ESPHOME_LOG_LEVEL_DEBUG, unsigned int line = __LINE__)
            {
                // we determine whether the package we have received is complete
                bool notAPacket = false;
                // the header pointer is always set to the beginning of the buffer
                notAPacket = notAPacket || (packet->crc == nullptr);
                notAPacket = notAPacket || (packet->data[0] != HEATPUMP_PACKET_START_BYTE);

                // If the packet is shorter than the length specified in the filter, then we do not output it.
                // If the output of packets is disabled using the HOLMES_WORKS directive, then we also do not output it.
                // "non-packets" are always output, since debugging bugs depends on them
                //if ((!notAPacket) && (packet->header->body_length < HOLMES_FILTER_LEN))
                //    return;
                if ((!notAPacket) && (!HOLMES_WORKS))
                    return;

                String st = "";
                char textBuf[11];

                // fill in the time of receiving the package
                memset(textBuf, 0, 11);
                //commit
                sprintf(textBuf, "%010" PRIu32, packet->msec);
                //sprintf(textBuf, "%010u", packet->msec);
                st = st + textBuf + ": ";

                // we form preambles
                if (packet == &_inPacket)
                {
                    st += "[<=] "; // incoming packet preamble
                }
                else if (packet == &_outPacket)
                {
                    st += "[=>] "; // outgoing packet preamble
                }
                else
                {
                    st += "[--] "; // preamble for "non-package"
                }

                // we are generating data
                for (int i = 0; i < packet->bytesLoaded; i++)
                {
                    // for normal packet headers you need to work out the brackets (if any)
                    if ((!notAPacket) && (i == 0))
                        st += HOLMES_HEADER_BRACKET_OPEN;
                    // for CRC of normal packets it is necessary to work out brackets (if any)
                    if ((!notAPacket) && (i == packet->header->body_length + HEATPUMP_HEADER_SIZE))
                        st += HOLMES_CRC_BRACKET_OPEN;

                    memset(textBuf, 0, 11);
                    sprintf(textBuf, HOLMES_BYTE_FORMAT, packet->data[i]);
                    st += textBuf;

                    // for normal packet headers you need to work out the brackets (if any)
                    if ((!notAPacket) && (i == HEATPUMP_HEADER_SIZE - 1))
                        st += HOLMES_HEADER_BRACKET_CLOSE;
                    // for CRC of normal packets it is necessary to work out brackets (if any)
                    if ((!notAPacket) && (i == packet->header->body_length + HEATPUMP_HEADER_SIZE + 2 - 1))
                        st += HOLMES_CRC_BRACKET_CLOSE;

                    st += HOLMES_DELIMITER;
                }

                _debugMsg(st, dbgLevel, line);
            }

            //CRC16 calculation for data block data of length len
            //data - data for CRC16 calculation, pointer to byte array
            //len - length of data block for calculation, in bytes
            //
            //return uint16_t CRC16
            //
            uint16_t _CRC16(uint8_t *data, uint8_t len)
            {
                uint32_t crc = 0;

                // we allocate a buffer for calculating the CRC and copy the transferred data into it
                // This is necessary so that in case of odd data length it is possible to supplement the packet body
                // with one zero byte and not spoil the downloaded packet (after all, in the downloaded one, the CRC comes immediately after the body)
                uint8_t _crcBuffer[HEATPUMP_BUFFER_SIZE];
                memset(_crcBuffer, 0, HEATPUMP_BUFFER_SIZE);
                memcpy(_crcBuffer, data, len);

                // if the data length is odd, then it must be made even by adding a zero byte at the end of the data
                // but since the buffer was filled with zeros above, there is no point in separately assigning 0x00 here
                if ((len % 2) == 1)
                    len++;

                // calculate CRC16
                uint32_t word = 0;
                for (uint8_t i = 0; i < len; i += 2)
                {
                    word = (_crcBuffer[i] << 8) + _crcBuffer[i + 1];
                    crc += word;
                }
                crc = (crc >> 16) + (crc & 0xFFFF);
                crc = ~crc;

                return crc & 0xFFFF;
            }

            // we calculate CRC16 and fill this data into the packet structure
            void _setCRC16(packet_t *pack = nullptr)
            {
                // if the packet is not specified, then we set the CRC for the outgoing packet
                if (pack == nullptr)
                    pack = &_outPacket;

                packet_crc_t crc;
                crc.crc16 = _CRC16(pack->data, HEATPUMP_HEADER_SIZE + pack->header->body_length);

                // if you forgot to set the pointer to crc, then we set it
                if (pack->crc == nullptr)
                    pack->crc = (packet_crc_t *)&(pack->data[HEATPUMP_HEADER_SIZE + pack->header->body_length]);

                pack->crc->crc[0] = crc.crc[1];
                pack->crc->crc[1] = crc.crc[0];
                return;
            }

            // checks the CRC of the packet by pointer
            bool _checkCRC(packet_t *pack = nullptr)
            {
                // if the package is not specified, then we check the incoming one
                if (pack == nullptr)
                    pack = &_inPacket;
                if (pack->bytesLoaded < HEATPUMP_HEADER_SIZE)
                {
                    _debugMsg(F("CRC check: incoming packet size error."), ESPHOME_LOG_LEVEL_WARN, __LINE__);
                    return false;
                }
                // if you forgot to set the pointer to crc, then we set it
                if (pack->crc == nullptr)
                    pack->crc = (packet_crc_t *)&(pack->data[HEATPUMP_HEADER_SIZE + pack->header->body_length]);

                packet_crc_t crc;
                crc.crc16 = _CRC16(pack->data, HEATPUMP_HEADER_SIZE + pack->header->body_length);

                return ((pack->crc->crc[0] == crc.crc[1]) && (pack->crc->crc[1] == crc.crc[0]));
            }

            // fills the packet by reference with the small status packet request command
            void _fillStatusSmall(packet_t *pack = nullptr)
            {
                // by default we fill the outgoing packet
                if (pack == nullptr)
                    pack = &_outPacket;

                // assign package parameters
                pack->msec = millis();
                pack->header->start_byte = HEATPUMP_PACKET_START_BYTE;
                pack->header->wifi = HEATPUMP_PACKET_ANSWER; // for the outgoing packet we set the response flag
                pack->header->packet_type = HEATPUMP_PTYPE_CMD;
                pack->header->body_length = 2; // command body 2 bytes
                pack->body = &(pack->data[HEATPUMP_HEADER_SIZE]);
                pack->body[0] = HEATPUMP_CMD_STATUS_SMALL;
                pack->body[1] = 0x01; // it's always 0x01
                pack->bytesLoaded = HEATPUMP_HEADER_SIZE + pack->header->body_length + 2;

                // we calculate and write the CRC into the packet
                pack->crc = (packet_crc_t *)&(pack->data[HEATPUMP_HEADER_SIZE + pack->header->body_length]);
                _setCRC16(pack);
            }

            // fills the packet by reference with the large status packet request command
            void _fillStatusBig(packet_t *pack = nullptr)
            {
                // by default we fill the outgoing packet
                if (pack == nullptr)
                    pack = &_outPacket;

                // assign package parameters
                pack->msec = millis();
                pack->header->start_byte = HEATPUMP_PACKET_START_BYTE;
                pack->header->wifi = HEATPUMP_PACKET_ANSWER; // for the outgoing packet we set the response flag
                pack->header->packet_type = HEATPUMP_PTYPE_CMD;
                pack->header->body_length = 2; // command body 2 bytes
                pack->body = &(pack->data[HEATPUMP_HEADER_SIZE]);
                pack->body[0] = HEATPUMP_CMD_STATUS_BIG;
                pack->body[1] = 0x01; // it's always 0x01
                pack->bytesLoaded = HEATPUMP_HEADER_SIZE + pack->header->body_length + 2;

                // we calculate and write the CRC into the packet
                pack->crc = (packet_crc_t *)&(pack->data[HEATPUMP_HEADER_SIZE + pack->header->body_length]);
                _setCRC16(pack);
            }

            //populates the package by reference with a parameter setting command
            //
            // a pointer to the package may be missing, then _outPacket should be filled in
            //the pointer to the command may also be absent, in which case the current state from _current_heatpump_state is used
            // all *__UNTOUCHED parameters are populated from _current_heatpump_state
            //
            bool sendCommandPacket(){
                _fillSetCommand(true,&_outPacket,&_current_heatpump_state);
                _debugPrintPacket(&_outPacket, ESPHOME_LOG_LEVEL_WARN, __LINE__);
                _setStateMachineState(HPSM_SENDING_PACKET);
                return true;
            }

            void _fillSetCommand(bool clrPacket = false, packet_t *pack = nullptr, heatpump_state_t *cmd = nullptr)
            {
                // by default we fill the outgoing packet
                if (pack == nullptr)
                    pack = &_outPacket;
                // clean the package if indicated
                if (clrPacket)
                    _clearPacket(pack);
                // we fill it with parameters from _current_heatpump_state
                //if (cmd != &_current_heatpump_state)
                //    _fillSetCommand(false, pack, &_current_heatpump_state);

                // if the command is not specified, then exit
                if (cmd == nullptr)
                    return;
                // the command is specified, we will additionally add to the package those parameters that are set in the command
                // assign package parameters
                pack->msec = millis();
                pack->header->start_byte = HEATPUMP_PACKET_START_BYTE;
                pack->header->wifi = HEATPUMP_PACKET_ANSWER; // for the outgoing packet we set the response flag
                pack->header->packet_type = HEATPUMP_PTYPE_CMD;
                pack->header->body_length = 0x09; // command body 15 (0x0F) bytes, same as Small status
                pack->body = &(pack->data[HEATPUMP_HEADER_SIZE]);
                pack->body[0] = HEATPUMP_CMD_SET_PARAMS; // set parameters
                pack->body[1] = 0x01;              // it's always 0x01
                pack->bytesLoaded = HEATPUMP_HEADER_SIZE + pack->header->body_length + 2;
//[BB,00,06,80,00,00,09,00,01,01,42,01,68,01,5E,00,00,2C,7B
//[BB 00 06 80 00 00 09 00]01 01 00 02 26 00 58 00 00 [B6 7B]
                // air conditioner target temperature
                pack->body[2]=0x42;
                pack->body[2]= (pack->body[2] | (cmd->co_on << 7)); 
                pack->body[2]= (pack->body[2]  | (cmd->cwu_on)); 

                cmd->temp_target_co = _temp_target_co_normalise(cmd->temp_target_co);
                    // integer part of temperature
                    pack->body[3]= ((uint8_t)((cmd->temp_target_co*10)>>8));
                    pack->body[4] = ((uint8_t)(cmd->temp_target_co*10));
                
                // air conditioner target temperature
                cmd->temp_target_cwu = _temp_target_cwu_normalise(cmd->temp_target_cwu);
                    // integer part of temperature
                    pack->body[5]= ((uint8_t)((cmd->temp_target_cwu*10)>>8));
                    pack->body[6] = ((uint8_t)(cmd->temp_target_cwu*10));

                    int tmp=0;
                    tmp= (cmd->all_eco_mode & 0b00001111);
                    pack->body[7]=(tmp | (cmd->eco_mode << 4));
                    
                    //tmp= (pack->body[7] & 0b11110000);
                    //pack->body[7]=(tmp | (1<<cmd->krzywa_grzewcza));
                    //TODO
                    tmp=0;
                    tmp= (cmd->other_mode & 0b11111101);
                    tmp= tmp | cmd->fast_cwu<<1;
                    pack->body[8]=tmp;
                    
                _debugMsg(F("sending state.%x %x"), ESPHOME_LOG_LEVEL_DEBUG, __LINE__,cmd->fast_cwu,tmp);
                // we calculate and write the CRC into the packet
                pack->crc = (packet_crc_t *)&(pack->data[HEATPUMP_HEADER_SIZE + pack->header->body_length]);
                _setCRC16(pack);
            }

            // sending a request to execute a command
            bool sq_requestDoCommand()
            {
                // if the outgoing packet is not empty, then we exit and wait for release
                if (_outPacket.bytesLoaded > 0)
                    return true;

                _fillSetCommand(true, &_outPacket, &_sequence[_sequence_current_step].cmd);
                _fillSetCommand(true, &_sequence[_sequence_current_step].packet, &_sequence[_sequence_current_step].cmd);
                //_sequence[_sequence_current_step].packet_type = HEATPUMP_SPT_SENT_PACKET;
 
                // We report to the log
                _debugMsg(F("Sequence [step %u]: doCommand request generated:"), ESPHOME_LOG_LEVEL_DEBUG, __LINE__, _sequence_current_step);
                _debugPrintPacket(&_outPacket, ESPHOME_LOG_LEVEL_VERBOSE, __LINE__);

                // increase the current step
                _sequence_current_step++;
                return true;
            }

            // checking the response to command execution
            bool sq_controlDoCommand()
            {
                // if for some reason there is no incoming packet, then we have nothing to check - we just exit
                if (_inPacket.bytesLoaded == 0)
                    return true;

                // We ignore pings
                if (_inPacket.header->packet_type == HEATPUMP_PTYPE_PING)
                    return true;

                // we save the received packet in a sequence so that we can work with it at possible next steps
                _copyPacket(&_sequence[_sequence_current_step].packet, &_inPacket);
                _sequence[_sequence_current_step].packet_type = HEATPUMP_SPT_RECEIVED_PACKET;

                // check the answer
                bool relevant = true;
                relevant = (relevant && (_inPacket.header->packet_type == HEATPUMP_PTYPE_INFO));
                relevant = (relevant && (_inPacket.header->body_length == 0x04));
                relevant = (relevant && (_inPacket.body[0] == 0x01));
                relevant = (relevant && (_inPacket.body[1] == HEATPUMP_CMD_SET_PARAMS));
                // bytes 2 and 3 are usually equal to the CRC of the sent command packet
                relevant = (relevant && (_inPacket.body[2] == _sequence[_sequence_current_step - 1].packet.crc->crc[0]));
                relevant = (relevant && (_inPacket.body[3] == _sequence[_sequence_current_step - 1].packet.crc->crc[1]));

                // If the package is suitable, then you can move on to the next step
                if (relevant)
                {
                    _debugMsg(F("Sequence [step %u]: correct doCommand packet received"), ESPHOME_LOG_LEVEL_VERBOSE, __LINE__, _sequence_current_step);
                    _sequence_current_step++;
                }
                else
                {
                    // if the package is not suitable, then we report it to the log...
                    _debugMsg(F("Sequence [step %u]: irrelevant incoming packet"), ESPHOME_LOG_LEVEL_WARN, __LINE__, _sequence_current_step);
                    _debugMsg(F("Incoming packet:"), ESPHOME_LOG_LEVEL_WARN, __LINE__);
                    _debugPrintPacket(&_inPacket, ESPHOME_LOG_LEVEL_WARN, __LINE__);
                    _debugMsg(F("Sequence packet needed: PACKET_TYPE = %02X, CMD = %02X"), ESPHOME_LOG_LEVEL_WARN, __LINE__, HEATPUMP_PTYPE_INFO, HEATPUMP_CMD_STATUS_BIG);
                    // ...and we interrupt the sequence
                }
                return relevant;
            }

            // sending request with test package
            bool sq_requestTestPacket()
            {
                // if the outgoing packet is not empty, then we exit and wait for release
                if (_outPacket.bytesLoaded > 0)
                    return true;

                _copyPacket(&_outPacket, &_outTestPacket);
                _copyPacket(&_sequence[_sequence_current_step].packet, &_outTestPacket);
                _sequence[_sequence_current_step].packet_type = HEATPUMP_SPT_SENT_PACKET;

                // We report to the log
                _debugMsg(F("Sequence [step %u]: Test Packet request generated:"), ESPHOME_LOG_LEVEL_VERBOSE, __LINE__, _sequence_current_step);
                _debugPrintPacket(&_outPacket, ESPHOME_LOG_LEVEL_VERBOSE, __LINE__);

                // increase the current step
                _sequence_current_step++;
                return true;
            }

            // sensors displaying split parameters
            esphome::sensor::Sensor * sensor_ibhw_out_temperature_  = nullptr;
            esphome::sensor::Sensor * sensor_temp_water_incoming_   = nullptr;
            esphome::sensor::Sensor * sensor_temp_water_outgoing_   = nullptr;
            esphome::sensor::Sensor * sensor_temp_freon_incoming_   = nullptr;
            esphome::sensor::Sensor * sensor_temp_freon_outgoing_   = nullptr;
            esphome::sensor::Sensor * sensor_temp_dhw_              = nullptr;
            esphome::sensor::Sensor * sensor_outside_temperature_int_ = nullptr;
            esphome::sensor::Sensor * sensor_compressor_frequency_    = nullptr;
            esphome::sensor::Sensor * sensor_flow_rate_                        = nullptr;
            esphome::sensor::Sensor * sensor_status_pracy_             = nullptr;
            esphome::sensor::Sensor * sensor_bi_byte2_ = nullptr;
            esphome::sensor::Sensor * sensor_bi_byte3_ = nullptr;
            esphome::sensor::Sensor * sensor_bi_byte5_ = nullptr;
            esphome::sensor::Sensor * sensor_bi_byte14_ = nullptr;
            esphome::sensor::Sensor * sensor_bi_byte15_ = nullptr;
            esphome::sensor::Sensor * sensor_bi_byte16_ = nullptr;
            esphome::sensor::Sensor * sensor_bi_byte18_ = nullptr;
            esphome::sensor::Sensor * sensor_bi_byte19_ = nullptr;
            esphome::sensor::Sensor * sensor_bi_byte20_ = nullptr;
            esphome::sensor::Sensor * sensor_bi_byte21_ = nullptr;


            esphome::sensor::Sensor * sensor_other_mode_    = nullptr;
            esphome::sensor::Sensor * sensor_eco_mode_                        = nullptr;
            esphome::sensor::Sensor * sensor_power_status_             = nullptr;
            esphome::binary_sensor::BinarySensor *sensor_cwu_status_ = nullptr;
            esphome::binary_sensor::BinarySensor *sensor_co_status_ = nullptr;
            esphome::binary_sensor::BinarySensor *sensor_eco_status_ = nullptr;
            esphome::binary_sensor::BinarySensor *sensor_fast_cwu_status_ = nullptr;

            esphome::sensor::Sensor * sensor_target_co_temperature_ = nullptr;
            esphome::sensor::Sensor * sensor_target_cwu_temperature_ = nullptr;
            esphome::binary_sensor::BinarySensor *sensor_defrost_ = nullptr;
            esphome::sensor::Sensor *sensor_inverter_power_ = nullptr;
            esphome::sensor::Sensor *sensor_inverter_power_limit_value_ = nullptr;
            esphome::binary_sensor::BinarySensor *sensor_inverter_power_limit_state_ = nullptr;

        public:
            // object initialization
            void initAC(esphome::uart::UARTComponent *parent = nullptr)
            {
                _dataMillis = millis();
                _clearInPacket();
                _clearOutPacket();
                _clearPacket(&_outTestPacket);
                _clearPacket(&_last_raw_data.last_big_info_packet);
                _clearPacket(&_last_raw_data.last_small_info_packet);

                _setStateMachineState(HPSM_IDLE);
                _heatpump_serial = parent;
                _hw_initialized = (_heatpump_serial != nullptr);
                _has_connection = false;
                _packet_timeout = Constants::HEATPUMP_PACKET_TIMEOUT_MIN;
                // fill the state structure with initial values
                _clearCommand((heatpump_command_t *)&_current_heatpump_state);

                // clear the packet sequence
                _clearSequence();

                // has the starting sequence of commands already been executed (collecting information about the air conditioner status)
                _startupSequenceComplete = false;

            };

            float get_setup_priority() const override { return esphome::setup_priority::DATA; }
            void set_compresor_frequency_sensor(sensor::Sensor* sensor) { sensor_compressor_frequency_ = sensor; }
            void set_flow_rate_sensor(sensor::Sensor* sensor) { sensor_flow_rate_ = sensor; }
            void set_status_pracy_sensor(sensor::Sensor* sensor) { sensor_status_pracy_ = sensor; }

            void set_water_outgoing_temperature_sensor(sensor::Sensor* temperature_sensor) { sensor_temp_water_outgoing_ = temperature_sensor; }
            void set_water_incoming_temperature_sensor(sensor::Sensor *temperature_sensor) { sensor_temp_water_incoming_ = temperature_sensor; }
            void set_freon_outgoing_temperature_sensor(sensor::Sensor *temperature_sensor) { sensor_temp_freon_outgoing_ = temperature_sensor; }
            void set_freon_incoming_temperature_sensor(sensor::Sensor *temperature_sensor) { sensor_temp_freon_incoming_ = temperature_sensor; }
            void set_ibhw_out_temperature_sensor(sensor::Sensor *temperature_sensor) { sensor_ibhw_out_temperature_ = temperature_sensor; }
            void set_dhw_temperature_sensor(sensor::Sensor *temperature_sensor) { sensor_temp_dhw_ = temperature_sensor; }
            void set_outside_temperature_sensor(sensor::Sensor *temperature_sensor) { sensor_outside_temperature_int_ = temperature_sensor; }
            //UNUSED 
            void set_bi_byte2_sensor(sensor::Sensor *sensor)	{ sensor_bi_byte2_ = sensor; }
            void set_bi_byte3_sensor(sensor::Sensor *sensor)	{ sensor_bi_byte3_ = sensor; }
            void set_bi_byte5_sensor(sensor::Sensor *sensor)	{ sensor_bi_byte5_ = sensor; }
            void set_bi_byte14_sensor(sensor::Sensor *sensor)	{ sensor_bi_byte14_ = sensor; }
            void set_bi_byte15_sensor(sensor::Sensor *sensor)	{ sensor_bi_byte15_ = sensor; }
            void set_bi_byte16_sensor(sensor::Sensor *sensor)	{ sensor_bi_byte16_ = sensor; }
            void set_bi_byte18_sensor(sensor::Sensor *sensor)	{ sensor_bi_byte18_ = sensor; }
            void set_bi_byte19_sensor(sensor::Sensor *sensor)	{ sensor_bi_byte19_ = sensor; }
            void set_bi_byte20_sensor(sensor::Sensor *sensor)	{ sensor_bi_byte20_ = sensor; }
            void set_bi_byte21_sensor(sensor::Sensor *sensor)	{ sensor_bi_byte21_ = sensor; }

            void set_power_state_sensor(sensor::Sensor *p_sensor) { sensor_power_status_    = p_sensor; }
            void set_other_mode_state_sensor(sensor::Sensor *o_sensor) { sensor_other_mode_      = o_sensor; }
            void set_eco_mode_state_sensor(sensor::Sensor *e_sensor) { sensor_eco_mode_        = e_sensor; }

            void set_co_status_sensor(binary_sensor::BinarySensor*sensor) { sensor_co_status_ = sensor;}
            void set_cwu_status_sensor(binary_sensor::BinarySensor*sensor) { sensor_cwu_status_ = sensor;}
            void set_eco_status_sensor(binary_sensor::BinarySensor*sensor) { sensor_eco_status_ = sensor;}
            void set_fast_cwu_status_sensor(binary_sensor::BinarySensor*sensor) { sensor_fast_cwu_status_ = sensor;}

            void set_target_co_temperature_sensor(sensor::Sensor* target_co_temperature_sensor) { sensor_target_co_temperature_ = target_co_temperature_sensor; }
            void set_target_cwu_temperature_sensor(sensor::Sensor* target_cwu_temperature_sensor) { sensor_target_cwu_temperature_ = target_cwu_temperature_sensor; }

            void set_defrost_state(binary_sensor::BinarySensor* defrost_state_sensor) { sensor_defrost_ = defrost_state_sensor; }
            void set_inverter_power_sensor(sensor::Sensor *inverter_power_sensor) { sensor_inverter_power_ = inverter_power_sensor; }
            void set_inverter_power_limit_value_sensor(sensor::Sensor *inverter_power_limit_value_sensor) { sensor_inverter_power_limit_value_ = inverter_power_limit_value_sensor; }
            void set_inverter_power_limit_state_sensor(binary_sensor::BinarySensor *inverter_power_limit_state_sensor) { sensor_inverter_power_limit_state_ = inverter_power_limit_state_sensor; }

            bool get_hw_initialized() { return _hw_initialized; };
            bool get_has_connection() { return _has_connection; };

            // returns whether there are elements in a command sequence
            bool hasSequence()
            {
                return (_sequence[0].item_type != HEATPUMP_SIT_NONE);
            }

            bool powerLimitationSetSequence(uint8_t targetValue, bool set_on = false)
            {
                if (!get_has_connection())
                {
                    _debugMsg(F("powerLimitationSetSequence: no pings from HeatPump. It seems like no controller connected."), ESPHOME_LOG_LEVEL_ERROR, __LINE__);
                    return false;
                }
                if (targetValue != this->_power_limitation_value_normalise(targetValue))
                {
                    _debugMsg(F("powerLimitationSetSequence: incorrect power limit value."), ESPHOME_LOG_LEVEL_WARN, __LINE__);
                    return false;
                }

                if (set_on)
                {
                    _debugMsg(F("powerLimitationSetSequence: loaded (state = %02X, power limit = %02X)"), ESPHOME_LOG_LEVEL_VERBOSE, __LINE__, targetValue, targetValue);
                }
                else {
                    _debugMsg(F("powerLimitationSetSequence: loaded (power limit = %02X)"), ESPHOME_LOG_LEVEL_VERBOSE, __LINE__, targetValue);
                }
                return true;
            }

            bool targetCOTemperatureSet(uint8_t targetTemperature)
            {
                _current_heatpump_state.temp_target_co=targetTemperature;
                if (sensor_target_co_temperature_ != nullptr)
                    sensor_target_co_temperature_->publish_state(_current_heatpump_state.temp_target_co);
                if (!get_has_connection())
                {
                    _debugMsg(F("targetCOTemperatureSet: no pings from HEATPUMP. It seems like no controller connected."), ESPHOME_LOG_LEVEL_ERROR, __LINE__);
                    return false;
                }
                
                if (targetTemperature != this->_temp_target_co_normalise(targetTemperature))
                {
                    _debugMsg(F("targetCOTemperatureSet: incorrect temperature value."), ESPHOME_LOG_LEVEL_WARN, __LINE__);
                    return false;
                }
                sendCommandPacket();
                return false;
            }

            bool targetCWUTemperatureSet(uint8_t targetTemperature)
            {
                _current_heatpump_state.temp_target_cwu=targetTemperature;
                if (sensor_target_cwu_temperature_ != nullptr)
                    sensor_target_cwu_temperature_->publish_state(_current_heatpump_state.temp_target_cwu);
                if (!get_has_connection())
                {
                    _debugMsg(F("targetCWUTemperatureSet: no pings from HEATPUMP. It seems like no controller connected."), ESPHOME_LOG_LEVEL_ERROR, __LINE__);
                    return false;
                }

                if (targetTemperature != this->_temp_target_cwu_normalise(targetTemperature))
                {
                    _debugMsg(F("targetCWUTemperatureSet: incorrect temperature value."), ESPHOME_LOG_LEVEL_WARN, __LINE__);
                    return false;
                }
                sendCommandPacket(); 
                return true;
            }

            bool powerLimitationOnOffSequence(bool enable_limit)
            {
                if (!get_has_connection())
                {
                    _debugMsg(F("powerLimitationOnOffSequence: no pings from HEATPUMP. It seems like no controller connected."), ESPHOME_LOG_LEVEL_ERROR, __LINE__);
                    return false;
                }

                if (!this->_is_inverter)
                {
                    _debugMsg(F("powerLimitationOnSequence: unsupported for noninverter AC."), ESPHOME_LOG_LEVEL_WARN, __LINE__);
                    return false;
                }

                
                _debugMsg(F("powerLimitationOnOffSequence: loaded (state = %02X)"), ESPHOME_LOG_LEVEL_VERBOSE, __LINE__,1);
                return true;
            }

            bool coPowerOffSequence()
            {
                if (!get_has_connection())
                {
                    _debugMsg(F("powerLimitationOnOffSequence: no pings from HEATPUMP. It seems like no controller connected."), ESPHOME_LOG_LEVEL_ERROR, __LINE__);
                    return false;
                }
                _current_heatpump_state.co_on=0;
                sendCommandPacket();
                return false;
            }

            bool coPowerOnSequence()
            {
                if (!get_has_connection())
                {
                    _debugMsg(F("powerLimitationOnOffSequence: no pings from HEATPUMP. It seems like no controller connected."), ESPHOME_LOG_LEVEL_ERROR, __LINE__);
                    return false;
                }
                _current_heatpump_state.co_on=1;
                sendCommandPacket();
                return true;
            }
            bool cwuPowerOffSequence()
            {
                if (!get_has_connection())
                {
                    _debugMsg(F("powerLimitationOnOffSequence: no pings from HEATPUMP. It seems like no controller connected."), ESPHOME_LOG_LEVEL_ERROR, __LINE__);
                    return false;
                }
                _current_heatpump_state.cwu_on=0;
                sendCommandPacket();
                return false;
            }

            // 
            bool cwuPowerOnSequence()
            {
                if (!get_has_connection())
                {
                    _debugMsg(F("powerLimitationOnOffSequence: no pings from HEATPUMP. It seems like no controller connected."), ESPHOME_LOG_LEVEL_ERROR, __LINE__);
                    return false;
                }
                _current_heatpump_state.cwu_on=1;
                sendCommandPacket();
                return true;
            }

            //ECO
            bool ecoOffSequence()
            {
                if (!get_has_connection())
                {
                    _debugMsg(F("powerLimitationOffSequence: no pings from HEATPUMP. It seems like no controller connected."), ESPHOME_LOG_LEVEL_ERROR, __LINE__);
                    return false;
                }
                _current_heatpump_state.eco_mode=0;
                sendCommandPacket();
                return false;
            }
            bool ecoOnSequence()
            {
                if (!get_has_connection())
                {
                    _debugMsg(F("powerLimitationOnSequence: no pings from HEATPUMP. It seems like no controller connected."), ESPHOME_LOG_LEVEL_ERROR, __LINE__);
                    return false;
                }
                _current_heatpump_state.eco_mode=1;
                sendCommandPacket();
                return true;
            }
            // 
            bool fast_CWUOffSequence()
            {
                if (!get_has_connection())
                {
                    _debugMsg(F("powerLimitationOffSequence: no pings from HEATPUMP. It seems like no controller connected."), ESPHOME_LOG_LEVEL_ERROR, __LINE__);
                    return false;
                }
                _current_heatpump_state.fast_cwu=0;
                sendCommandPacket();
                return false;
            }
            bool fast_CWUOnSequence()
            {
                if (!get_has_connection())
                {
                    _debugMsg(F("powerLimitationOnSequence: no pings from HEATPUMP. It seems like no controller connected."), ESPHOME_LOG_LEVEL_ERROR, __LINE__);
                    return false;
                }                
                _current_heatpump_state.fast_cwu=1;
                sendCommandPacket();
                return true;
            }
            // 
            bool powerLimitationOnSequence()
            {
                return powerLimitationOnOffSequence(true);
            }

            // 
            bool powerLimitationOnSequence(uint8_t power_limit)
            {
                return powerLimitationSetSequence(power_limit, true);
            }

            // 
            bool powerLimitationOffSequence()
            {
                return powerLimitationOnOffSequence(false);
            }


            // we publish all states of sensors and split
            void publish_all_states()
            {
                if (sensor_bi_byte2_ != nullptr)	 sensor_bi_byte2_->publish_state(_current_heatpump_state.bi_byte2);
                if (sensor_bi_byte3_ != nullptr)	 sensor_bi_byte3_->publish_state(_current_heatpump_state.bi_byte3);
                if (sensor_bi_byte5_ != nullptr)	 sensor_bi_byte5_->publish_state(_current_heatpump_state.bi_byte5);
                if (sensor_bi_byte14_ != nullptr)	 sensor_bi_byte14_->publish_state(_current_heatpump_state.bi_byte14);
                if (sensor_bi_byte15_ != nullptr)	 sensor_bi_byte15_->publish_state(_current_heatpump_state.bi_byte15);
                if (sensor_bi_byte16_ != nullptr)	 sensor_bi_byte16_->publish_state(_current_heatpump_state.bi_byte16);
                if (sensor_bi_byte18_ != nullptr)	 sensor_bi_byte18_->publish_state(_current_heatpump_state.bi_byte18);
                if (sensor_bi_byte19_ != nullptr)	 sensor_bi_byte19_->publish_state(_current_heatpump_state.bi_byte19);
                if (sensor_bi_byte20_ != nullptr)	 sensor_bi_byte20_->publish_state(_current_heatpump_state.bi_byte20);
                if (sensor_bi_byte21_ != nullptr)	 sensor_bi_byte21_->publish_state(_current_heatpump_state.bi_byte21);

                if (sensor_status_pracy_ != nullptr)
                sensor_status_pracy_->publish_state(_current_heatpump_state.valve_pump_and_heatingElements_status);
                if (sensor_eco_mode_ != nullptr)
                sensor_eco_mode_->publish_state(_current_heatpump_state.all_eco_mode);
                if (sensor_power_status_ != nullptr)
                sensor_power_status_->publish_state(_current_heatpump_state.power_state);
                if (sensor_other_mode_ != nullptr)
                sensor_other_mode_->publish_state(_current_heatpump_state.other_mode);
                
                if (sensor_compressor_frequency_ != nullptr)
                sensor_compressor_frequency_->publish_state(_current_heatpump_state.compressor_frequency);
                if (sensor_flow_rate_ != nullptr)
                sensor_flow_rate_->publish_state(_current_heatpump_state.flow_rate);

                if (sensor_fast_cwu_status_ != nullptr)
                sensor_fast_cwu_status_->publish_state(_current_heatpump_state.fast_cwu);
                if (sensor_ibhw_out_temperature_ != nullptr)
                    sensor_ibhw_out_temperature_->publish_state(_current_heatpump_state.temp_ibhw);

                if (sensor_outside_temperature_int_ != nullptr)
                    sensor_outside_temperature_int_->publish_state(_current_heatpump_state.temp_outside);

                if (sensor_temp_freon_incoming_ != nullptr)
                    sensor_temp_freon_incoming_->publish_state(_current_heatpump_state.temp_freon_incoming);

                if (sensor_temp_freon_outgoing_ != nullptr)
                    sensor_temp_freon_outgoing_->publish_state(_current_heatpump_state.temp_freon_outgoing);

                if (sensor_temp_water_incoming_ != nullptr)
                    sensor_temp_water_incoming_->publish_state(_current_heatpump_state.temp_water_incoming);
                if (sensor_temp_water_outgoing_ != nullptr)
                    sensor_temp_water_outgoing_->publish_state(_current_heatpump_state.temp_water_outgoing);
                // outlet line temperature
                if (sensor_temp_dhw_ != nullptr)
                    sensor_temp_dhw_->publish_state(_current_heatpump_state.temp_dhw);

                if (sensor_co_status_ != nullptr)
                    sensor_co_status_->publish_state(_current_heatpump_state.co_on);

                if (sensor_cwu_status_ != nullptr)
                    sensor_cwu_status_->publish_state(_current_heatpump_state.cwu_on);

                if (sensor_eco_status_ != nullptr)
                    sensor_eco_status_->publish_state(_current_heatpump_state.eco_mode);
                    
                if (sensor_target_co_temperature_ != nullptr)
                    sensor_target_co_temperature_->publish_state(_current_heatpump_state.temp_target_co);

                if (sensor_target_cwu_temperature_ != nullptr)
                    sensor_target_cwu_temperature_->publish_state(_current_heatpump_state.temp_target_cwu);
                /*// inverter power
                if (sensor_inverter_power_ != nullptr)
                    sensor_inverter_power_->publish_state(_current_heatpump_state.inverter_power);
                    */
                // defrost mode flag
                if (sensor_defrost_ != nullptr)
                    sensor_defrost_->publish_state(_current_heatpump_state.defrost);
                // inverter power limit enabled flag
                if (sensor_inverter_power_limit_state_ != nullptr)
                    sensor_inverter_power_limit_state_->publish_state(_current_heatpump_state.power_lim_state == HEATPUMP_POWLIMSTAT_ON);
                // inverter power limit value
                if (sensor_inverter_power_limit_value_ != nullptr)
                    sensor_inverter_power_limit_value_->publish_state(_current_heatpump_state.power_lim_value);
                if (sensor_target_co_temperature_ != nullptr)
                    sensor_target_co_temperature_->publish_state(_current_heatpump_state.temp_target_co);
                if (sensor_target_cwu_temperature_ != nullptr)
                    sensor_target_cwu_temperature_->publish_state(_current_heatpump_state.temp_target_cwu);

            }

            // output to debug of the current component configuration
            void dump_config()
            {
                ESP_LOGCONFIG(TAG, "AUX HVAC:");
                ESP_LOGCONFIG(TAG, "  [x] Firmware version: %s", Constants::HEATPUMP_FIRMWARE_VERSION.c_str());
                ESP_LOGCONFIG(TAG, "  [x] Packet timeout: %" PRIu32 "ms", this->get_packet_timeout());//#commit
                //ESP_LOGCONFIG(TAG, "  [x] Packet timeout: %dms", this->get_packet_timeout());
                
                LOG_SENSOR("  ", "Inverter Power", this->sensor_inverter_power_);
                LOG_SENSOR("  ", "Inverter Power Limit Value", this->sensor_inverter_power_limit_value_);
                LOG_BINARY_SENSOR("  ", "Inverter Power Limit State", this->sensor_inverter_power_limit_state_);
                LOG_SENSOR("  ", "IBH W-out", this->sensor_ibhw_out_temperature_);
                LOG_SENSOR("  ", "CWU Temperature", this->sensor_temp_dhw_);
                LOG_SENSOR("  ", "Freon incoming Temperature", this->sensor_temp_freon_incoming_);
                LOG_SENSOR("  ", "Freon outgoingTemperature", this->sensor_temp_freon_outgoing_);
                LOG_SENSOR("  ", "Water incoming Temperature", this->sensor_temp_water_incoming_);
                LOG_SENSOR("  ", "Water outgoing Temperature", this->sensor_temp_water_outgoing_);
                LOG_SENSOR("  ", "Condenser Temperature", this->sensor_outside_temperature_int_);
                LOG_BINARY_SENSOR("  ", "CO Status", this->sensor_co_status_);
                LOG_BINARY_SENSOR("  ", "CWU Status", this->sensor_cwu_status_);
                
            }

            // sends the split a given set of bytes
            // Before sending:
            // sets the first byte to 0xBB
            // checks that the length of the packet body in the header does not exceed the length of the buffer
            // calculates and writes CRC to the end of the packet
            bool sendTestPacket(const std::vector<uint8_t> &data)
            {
                if (data.size() == 0)
                {
                    _debugMsg(F("sendTestPacket: no data to send."), ESPHOME_LOG_LEVEL_ERROR, __LINE__);
                    return false;
                }
                // if (data.size() > HEATPUMP_BUFFER_SIZE) return false;

                // there is no point in sending if there is no connection with the air conditioner
                
				if (!get_has_connection())
                {
                    _debugMsg(F("sendTestPacket: no pings from HEATPUMP. It seems like no controller connected."), ESPHOME_LOG_LEVEL_ERROR, __LINE__);
                    return false;
                }
				
                // we clean the package
                _clearPacket(&_outTestPacket);

                // copy data into the package
                uint8_t i = 0;
                for (uint8_t n : data)
                {
                    // Anything that doesn't fit into the buffer - we ignore
                    if (i >= HEATPUMP_BUFFER_SIZE)
                    {
                        _debugMsg(F("sendTestPacket: buffer size =  %02d, data length = %02d. Extra data was omitted."), ESPHOME_LOG_LEVEL_ERROR, __LINE__, HEATPUMP_BUFFER_SIZE, data.size());
                        break;
                    }
                    // whatever fits - copy to clipboard
                    _outTestPacket.data[i] = n;
                    i++;
                }

                // just in case, we indicate some correct bytes:
                // - set the start byte
                _outTestPacket.header->start_byte = HEATPUMP_PACKET_START_BYTE;
                // - we will set the length of the body if it is greater than possible for our buffer
                if (_outTestPacket.header->body_length > (HEATPUMP_BUFFER_SIZE - HEATPUMP_HEADER_SIZE - 2))
                    _outTestPacket.header->body_length = HEATPUMP_BUFFER_SIZE - HEATPUMP_HEADER_SIZE - 2;

                _outTestPacket.msec = millis();
                _outTestPacket.body = &(_outTestPacket.data[HEATPUMP_HEADER_SIZE]);
                _outTestPacket.bytesLoaded = HEATPUMP_HEADER_SIZE + _outTestPacket.header->body_length + 2;

                // we calculate and write the CRC into the packet
                _outTestPacket.crc = (packet_crc_t *)&(_outTestPacket.data[HEATPUMP_HEADER_SIZE + _outTestPacket.header->body_length]);
                _setCRC16(&_outTestPacket);

                _debugMsg(F("sendTestPacket: test packet loaded:"), ESPHOME_LOG_LEVEL_WARN, __LINE__);
                _debugPrintPacket(&_outTestPacket, ESPHOME_LOG_LEVEL_WARN, __LINE__);

                // below is the block for adding a packet to the command sequence
                // *****************************************************************
                // is there a place for the query in the command sequence?
                if (_getFreeSequenceSpace() < 1)
                {
                    _debugMsg(F("sendTestPacket: not enough space in command sequence. Sequence steps doesn't loaded."), ESPHOME_LOG_LEVEL_WARN, __LINE__);
                    return false;
                }

                /*************************************** sendTestPacket request ***********************************************/
                if (!_addSequenceFuncStep(&HeatPump::sq_requestTestPacket))
                {
                    _debugMsg(F("sendTestPacket: sendTestPacket request sequence step fail."), ESPHOME_LOG_LEVEL_WARN, __LINE__);
                    return false;
                }
                /**************************************************************************************/

                _debugMsg(F("sendTestPacket: loaded to sequence"), ESPHOME_LOG_LEVEL_VERBOSE, __LINE__);
                _copyPacket(&_outPacket, &_outTestPacket);
                _setStateMachineState(HPSM_SENDING_PACKET);
                return true;
            }

            void set_packet_timeout(uint32_t ms)
            {
                if (ms < Constants::HEATPUMP_PACKET_TIMEOUT_MIN)
                    ms = Constants::HEATPUMP_PACKET_TIMEOUT_MIN;
                if (ms > Constants::HEATPUMP_PACKET_TIMEOUT_MAX)
                    ms = Constants::HEATPUMP_PACKET_TIMEOUT_MIN;
                this->_packet_timeout = ms;
            }
            uint32_t get_packet_timeout() { return this->_packet_timeout; }

            void loop() override
            {
                if (!get_hw_initialized())
                    return;

                // / we process the states of the finite state machine
                switch (_heatpump_state)
                {
                    case HPSM_RECEIVING_PACKET:
                        // we are in the process of receiving a package, no sending is possible in this state
                        _doReceivingPacketState();
                        break;

                    case HPSM_PARSING_PACKET:
                        // we analyze the received package
                        _doParsingPacket();
                        break;

                    case HPSM_SENDING_PACKET:
                        // we send the package to split
                        _doSendingPacketState();
                        break;

                    case HPSM_IDLE: // we do nothing, we wait for something to react to
                    default:        // if the state is some other one, then we consider it IDLE
                        _doIdleState();
                        break;
                }

            };
        };

    } // namespace aux_heatpump
} // namespace esphome
