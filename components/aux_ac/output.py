
import logging
from esphome.core import CORE, Define
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.components import output,uart, sensor, binary_sensor
from esphome import automation
from esphome.automation import maybe_simple_id
from esphome.const import (
    CONF_DATA,
    CONF_ID,
    CONF_INTERNAL,
    CONF_TIMEOUT,
    CONF_UART_ID,
    UNIT_CELSIUS,
    UNIT_PERCENT,
    ICON_THERMOMETER,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_POWER_FACTOR,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_NONE,
)

AUX_HEATPUMP_FIRMWARE_VERSION = '0.2.15'
HEATPUMP_PACKET_TIMEOUT_MIN = 150
HEATPUMP_PACKET_TIMEOUT_MAX = 600
HEATPUMP_POWER_LIMIT_MIN = 30
HEATPUMP_POWER_LIMIT_MAX = 100

_LOGGER = logging.getLogger(__name__)

CODEOWNERS = ["@arni077r"]
DEPENDENCIES = [ "uart"]
AUTO_LOAD = ["sensor", "binary_sensor","number","switch","button"]

CONF_SHOW_ACTION = "show_action"
CONF_BUTTON="button"
CONF_DHW_TEMPERATURE =              "dhw_temperature"

CONF_WATER_INBOUND_TEMPERATURE =    "water_incoming_temperature"
ICON_WATER_INBOUND_TEMPERATURE = "mdi:thermometer-plus"

CONF_WATER_OUTGOING_TEMPERATURE =   "water_outgoing_temperature"
ICON_WATER_OUTGOING_TEMPERATURE = "mdi:thermometer-minus"

CONF_OUTSIDE_TEMPERATURE =        "outside_temperature"
ICON_OUTSIDE_TEMPERATURE = "mdi:thermometer-lines"

CONF_FREON_INBOUND_TEMPERATURE =    "freon_incoming_temperature"
ICON_FREON_INBOUND_TEMPERATURE = "mdi:thermometer-plus"

CONF_FREON_OUTGOING_TEMPERATURE =   "freon_outgoing_temperature"
ICON_FREON_OUTGOING_TEMPERATURE = "mdi:thermometer-minus"

CONF_IBHW_TEMPERATURE =             "ibhw_temperature"
ICON_IBHW_TEMPERATURE = "mdi:thermometer-minus"

CONF_INVERTER_POWER = "inverter_power"
CONF_INVERTER_POWER_DEPRICATED = "invertor_power"

CONF_COMPRESOR_FREQUENCY = "compresor_frequency_state"
ICON_COMPRESOR_FREQUENCY = "mdi:sine-wave"

CONF_FLOWRATE = "flow_rate_state"
CONF_STATUS_PRACY_MOZE = "status_sterowania_zaworami_pompkami"
CONF_DEFROST_STATE = "defrost_state"
ICON_DEFROST = "mdi:snowflake-melt"

ICON_STATE = "mdi:state-machine"
CONF_POWER_STATE = "all_power_state"
CONF_ECO_MODE_STATE = "all_eco_mode_state"
CONF_OTHER_MODE_STATE = "all_other_mode_state"

CONF_CWU_POWER_STATE = "cwu_power_state"
ICON_CWU_STATE = "mdi:snowflake-melt"

CONF_CO_POWER_STATE = "co_power_state"
ICON_CO_STATE = "mdi:snowflake-melt"

CONF_DISPLAY_INVERTED = "display_inverted"
ICON_DISPLAY = "mdi:clock-digital"

CONF_LIMIT = "limit"
CONF_INVERTER_POWER_LIMIT_VALUE = "inverter_power_limit_value"
ICON_INVERTER_POWER_LIMIT_VALUE = "mdi:meter-electric-outline"
CONF_INVERTER_POWER_LIMIT_STATE = "inverter_power_limit_state"
ICON_INVERTER_POWER_LIMIT_STATE = "mdi:meter-electric-outline"

CONF_TARGET_CO_TEMPERATURE = "co_target_temperature"
CONF_TARGET_CWU_TEMPERATURE = "cwu_target_temperature"

CONF_ECO_STATE = "eco_mode_state"

CONF_FAST_CWU_STATE = "fast_cwu_state"

#unused
CONF_BI_BYTE2 	= "bi_byte2"
CONF_BI_BYTE3 	= "bi_byte3"
CONF_BI_BYTE5 	= "bi_byte5"
CONF_BI_BYTE14 	= "bi_byte14"
CONF_BI_BYTE15 	= "bi_byte15"
CONF_BI_BYTE16 	= "bi_byte16"
CONF_BI_BYTE18 	= "bi_byte18"
CONF_BI_BYTE19 	= "bi_byte19"
CONF_BI_BYTE20 	= "bi_byte20"
CONF_BI_BYTE21 	= "bi_byte21"
CONF_BI_BYTE22 	= "bi_byte22"

aux_heatpump_nsa = cg.esphome_ns.namespace("aux_heatpump")
aux_heatpump_ns = cg.esphome_ns.namespace("aux_heatpump")
HeatPump = aux_heatpump_ns.class_("HeatPump", output.FloatOutput, cg.Component)
Capabilities = aux_heatpump_ns.namespace("Constants")

# test packet action
HeatPumpSendTestPacketAction = aux_heatpump_ns.class_(
    "HeatPumpSendTestPacketAction", automation.Action
)

#Actions
COPowerOffAction = aux_heatpump_ns.class_("COPowerOffAction", automation.Action)
COPowerOnAction = aux_heatpump_ns.class_("COPowerOnAction", automation.Action)
CWUPowerOffAction = aux_heatpump_ns.class_("CWUPowerOffAction", automation.Action)
CWUPowerOnAction = aux_heatpump_ns.class_("CWUPowerOnAction", automation.Action)
ECOOffAction = aux_heatpump_ns.class_("ECOOffAction", automation.Action)
ECOOnAction = aux_heatpump_ns.class_("ECOOnAction", automation.Action)
Fast_CWUOffAction = aux_heatpump_ns.class_("Fast_CWUOffAction", automation.Action)
Fast_CWUOnAction = aux_heatpump_ns.class_("Fast_CWUOnAction", automation.Action)

# power limitation actions
HeatPumpPowerLimitationOffAction = aux_heatpump_ns.class_(
    "HeatPumpPowerLimitationOffAction", automation.Action
)
HeatPumpPowerLimitationOnAction = aux_heatpump_ns.class_(
    "HeatPumpPowerLimitationOnAction", automation.Action
)


def validate_packet_timeout(value):
    minv = HEATPUMP_PACKET_TIMEOUT_MIN
    maxv = HEATPUMP_PACKET_TIMEOUT_MAX
    if value in range(minv, maxv+1):
        return cv.Schema(cv.uint32_t)(value)
    raise cv.Invalid(f"Timeout should be in range: {minv}..{maxv}.")


def validate_power_limit_range(value):
    minv = HEATPUMP_POWER_LIMIT_MIN
    maxv = HEATPUMP_POWER_LIMIT_MAX
    if value in range(minv, maxv+1):
        return cv.Schema(cv.uint32_t)(value)
    raise cv.Invalid(f"Power limit should be in range: {minv}..{maxv}")


def validate_raw_data(value):
    if isinstance(value, list):
        return cv.Schema([cv.hex_uint8_t])(value)
    raise cv.Invalid("data must be a list of bytes")


def output_info(config):
    _LOGGER.info("AUX_HEATPUMP firmware version: %s", AUX_HEATPUMP_FIRMWARE_VERSION)
    return config


CONFIG_SCHEMA = cv.All(
    output.FLOAT_OUTPUT_SCHEMA.extend( # type: ignore
        {
            cv.GenerateID(): cv.declare_id(HeatPump),
            cv.Optional(CONF_SHOW_ACTION, default="false"): cv.boolean,
            cv.Optional(CONF_TIMEOUT, default=HEATPUMP_PACKET_TIMEOUT_MIN): validate_packet_timeout,
            cv.Optional(CONF_INVERTER_POWER_DEPRICATED): cv.invalid(
                "The name of sensor was changed in v.0.2.9 from 'invertor_power' to 'inverter_power'. Update your config please."
            ),
            cv.Optional(CONF_INVERTER_POWER): sensor.sensor_schema(
                unit_of_measurement=UNIT_PERCENT,
                icon=ICON_STATE,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_POWER_FACTOR,
                state_class=STATE_CLASS_MEASUREMENT,
            ).extend(
                {
                    cv.Optional(CONF_INTERNAL, default="false"): cv.boolean,
                }
            ),

            cv.Optional(CONF_DHW_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                icon=ICON_THERMOMETER,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ).extend(
                {
                    cv.Optional(CONF_INTERNAL, default="false"): cv.boolean,
                }
            ),
            cv.Optional(CONF_OUTSIDE_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                icon=ICON_OUTSIDE_TEMPERATURE,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ).extend(
                {
                    cv.Optional(CONF_INTERNAL, default="false"): cv.boolean,
                }
            ),
            cv.Optional(CONF_WATER_INBOUND_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                icon=ICON_WATER_INBOUND_TEMPERATURE,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ).extend(
                {
                    cv.Optional(CONF_INTERNAL, default="false"): cv.boolean,
                }
            ),
            cv.Optional(CONF_COMPRESOR_FREQUENCY): sensor.sensor_schema(
                unit_of_measurement='Hz',
                icon=ICON_COMPRESOR_FREQUENCY,
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
            ).extend(
                {
                    cv.Optional(CONF_INTERNAL, default="false"): cv.boolean,
                }
            ),
            cv.Optional(CONF_FLOWRATE): sensor.sensor_schema(
                unit_of_measurement='m3/h',
                icon=ICON_INVERTER_POWER_LIMIT_VALUE,
                accuracy_decimals=2,
                device_class='',
                state_class=STATE_CLASS_NONE,
            ).extend(
                {
                    cv.Optional(CONF_INTERNAL, default="false"): cv.boolean,
                }
            ),
            cv.Optional(CONF_STATUS_PRACY_MOZE): sensor.sensor_schema(
                unit_of_measurement='',
                icon=ICON_STATE,
                accuracy_decimals=0,
                device_class='',
                state_class=STATE_CLASS_MEASUREMENT,
            ).extend(
                {
                    cv.Optional(CONF_INTERNAL, default="false"): cv.boolean,
                }
            ),
            cv.Optional(CONF_POWER_STATE): sensor.sensor_schema(
                unit_of_measurement='',
                icon=ICON_STATE,
                accuracy_decimals=0,
                device_class='',
                state_class=STATE_CLASS_MEASUREMENT,
            ).extend(
                {
                    cv.Optional(CONF_INTERNAL, default="false"): cv.boolean,
                }
            ),
            cv.Optional(CONF_ECO_MODE_STATE): sensor.sensor_schema(
                unit_of_measurement='',
                icon=ICON_STATE,
                accuracy_decimals=0,
                device_class='',
                state_class=STATE_CLASS_MEASUREMENT,
            ).extend(
                {
                    cv.Optional(CONF_INTERNAL, default="false"): cv.boolean,
                }
            ),
            cv.Optional(CONF_OTHER_MODE_STATE): sensor.sensor_schema(
                unit_of_measurement='',
                icon=ICON_STATE,
                accuracy_decimals=0,
                device_class='',
                state_class=STATE_CLASS_MEASUREMENT,
            ).extend(
                {
                    cv.Optional(CONF_INTERNAL, default="false"): cv.boolean,
                }
            ),
            cv.Optional(CONF_WATER_OUTGOING_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                icon=ICON_WATER_OUTGOING_TEMPERATURE,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ).extend(
                {
                    cv.Optional(CONF_INTERNAL, default="false"): cv.boolean,
                }
            ),
            cv.Optional(CONF_FREON_INBOUND_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                icon=ICON_FREON_INBOUND_TEMPERATURE,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ).extend(
                {
                    cv.Optional(CONF_INTERNAL, default="false"): cv.boolean,
                }
            ),
            cv.Optional(CONF_FREON_OUTGOING_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                icon=ICON_FREON_OUTGOING_TEMPERATURE,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ).extend(
                {
                    cv.Optional(CONF_INTERNAL, default="false"): cv.boolean,
                }
            ),
            cv.Optional(CONF_IBHW_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                icon=ICON_IBHW_TEMPERATURE,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ).extend(
                {
                    cv.Optional(CONF_INTERNAL, default="false"): cv.boolean,
                }
            ),
            cv.Optional(CONF_OUTSIDE_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                icon=ICON_OUTSIDE_TEMPERATURE,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ).extend(
                {
                    cv.Optional(CONF_INTERNAL, default="false"): cv.boolean,
                }
            ),
            cv.Optional(CONF_DEFROST_STATE): binary_sensor.binary_sensor_schema(
                icon=ICON_DEFROST,
            ).extend(
                {
                    cv.Optional(CONF_INTERNAL, default="false"): cv.boolean,
                }
            ),
            cv.Optional(CONF_CO_POWER_STATE): binary_sensor.binary_sensor_schema(
                icon=ICON_DEFROST,
            ).extend(
                {
                    cv.Optional(CONF_INTERNAL, default="false"): cv.boolean,
                }
            ),
            cv.Optional(CONF_CWU_POWER_STATE): binary_sensor.binary_sensor_schema(
                icon=ICON_DEFROST,
            ).extend(
                {
                    cv.Optional(CONF_INTERNAL, default="false"): cv.boolean,
                }
            ),
            cv.Optional(CONF_ECO_STATE): binary_sensor.binary_sensor_schema(
                icon=ICON_DEFROST,
            ).extend(
                {
                    cv.Optional(CONF_INTERNAL, default="false"): cv.boolean,
                }
            ),
            cv.Optional(CONF_FAST_CWU_STATE): binary_sensor.binary_sensor_schema(
                icon=ICON_DEFROST,
            ).extend(
                {
                    cv.Optional(CONF_INTERNAL, default="false"): cv.boolean,
                }
            ),
            cv.Optional(CONF_INVERTER_POWER_LIMIT_VALUE): sensor.sensor_schema(
                unit_of_measurement=UNIT_PERCENT,
                icon=ICON_INVERTER_POWER_LIMIT_VALUE,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_POWER_FACTOR,
                state_class=STATE_CLASS_MEASUREMENT,
            ).extend(
                {
                    cv.Optional(CONF_INTERNAL, default="false"): cv.boolean,
                }
            ),
            cv.Optional(CONF_INVERTER_POWER_LIMIT_STATE): binary_sensor.binary_sensor_schema(
                icon=ICON_INVERTER_POWER_LIMIT_STATE,
            ).extend(
                {
                    cv.Optional(CONF_INTERNAL, default="false"): cv.boolean,
                }
            ),
            cv.Optional(CONF_TARGET_CO_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                icon=ICON_IBHW_TEMPERATURE,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ).extend(
                {
                    cv.Optional(CONF_INTERNAL, default="false"): cv.boolean,
                }
            ),
            cv.Optional(CONF_TARGET_CWU_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                icon=ICON_IBHW_TEMPERATURE,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ).extend(
                {
                    cv.Optional(CONF_INTERNAL, default="false"): cv.boolean,
                }
            ),
            cv.Optional(CONF_BI_BYTE2): sensor.sensor_schema(
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
            ).extend(
                {
                    cv.Optional(CONF_INTERNAL, default="false"): cv.boolean,
                }
            ),
            cv.Optional(CONF_BI_BYTE3): sensor.sensor_schema(
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
            ).extend(
                {
                    cv.Optional(CONF_INTERNAL, default="false"): cv.boolean,
                }
            ),
            cv.Optional(CONF_BI_BYTE5): sensor.sensor_schema(
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
            ).extend(
                {
                    cv.Optional(CONF_INTERNAL, default="false"): cv.boolean,
                }
            ),
            cv.Optional(CONF_BI_BYTE14): sensor.sensor_schema(
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
            ).extend(
                {
                    cv.Optional(CONF_INTERNAL, default="false"): cv.boolean,
                }
            ),
            cv.Optional(CONF_BI_BYTE15): sensor.sensor_schema(
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
            ).extend(
                {
                    cv.Optional(CONF_INTERNAL, default="false"): cv.boolean,
                }
            ),
            cv.Optional(CONF_BI_BYTE16): sensor.sensor_schema(
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
            ).extend(
                {
                    cv.Optional(CONF_INTERNAL, default="false"): cv.boolean,
                }
            ),
            cv.Optional(CONF_BI_BYTE18): sensor.sensor_schema(
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
            ).extend(
                {
                    cv.Optional(CONF_INTERNAL, default="false"): cv.boolean,
                }
            ),
            cv.Optional(CONF_BI_BYTE19): sensor.sensor_schema(
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
            ).extend(
                {
                    cv.Optional(CONF_INTERNAL, default="false"): cv.boolean,
                }
            ),
            cv.Optional(CONF_BI_BYTE20): sensor.sensor_schema(
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
            ).extend(
                {
                    cv.Optional(CONF_INTERNAL, default="false"): cv.boolean,
                }
            ),
            cv.Optional(CONF_BI_BYTE21): sensor.sensor_schema(
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
            ).extend(
                {
                    cv.Optional(CONF_INTERNAL, default="false"): cv.boolean,
                }
            ),
        }
    )
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA),
    output_info,
)


async def to_code(config):
    CORE.add_define(
        Define("AUX_HEATPUMP_FIRMWARE_VERSION", '"'+AUX_HEATPUMP_FIRMWARE_VERSION+'"')
    )
    CORE.add_define(
        Define("AUX_HEATPUMP_PACKET_TIMEOUT_MIN", HEATPUMP_PACKET_TIMEOUT_MIN)
    )
    CORE.add_define(
        Define("AUX_HEATPUMP_PACKET_TIMEOUT_MAX", HEATPUMP_PACKET_TIMEOUT_MAX)
    )
    CORE.add_define(
        Define("AUX_HEATPUMP_MIN_INVERTER_POWER_LIMIT", HEATPUMP_POWER_LIMIT_MIN)
    )
    CORE.add_define(
        Define("AUX_HEATPUMP_MAX_INVERTER_POWER_LIMIT", HEATPUMP_POWER_LIMIT_MAX)
    )
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    parent = await cg.get_variable(config[CONF_UART_ID])

    cg.add(var.initAC(parent))

    if CONF_DHW_TEMPERATURE in config:
        conf = config[CONF_DHW_TEMPERATURE]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_dhw_temperature_sensor(sens))

    if CONF_OUTSIDE_TEMPERATURE in config:
        conf = config[CONF_OUTSIDE_TEMPERATURE]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_outside_temperature_sensor(sens))

    if CONF_WATER_OUTGOING_TEMPERATURE in config:
        conf = config[CONF_WATER_OUTGOING_TEMPERATURE]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_water_outgoing_temperature_sensor(sens))

    if CONF_COMPRESOR_FREQUENCY in config:
        conf = config[CONF_COMPRESOR_FREQUENCY]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_compresor_frequency_sensor(sens))
    if CONF_FLOWRATE in config:
        conf = config[CONF_FLOWRATE]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_flow_rate_sensor(sens))
    if CONF_STATUS_PRACY_MOZE in config:
        conf = config[CONF_STATUS_PRACY_MOZE]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_status_pracy_sensor(sens))

    if CONF_POWER_STATE in config:
        conf = config[CONF_POWER_STATE]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_power_state_sensor(sens))

    if CONF_ECO_MODE_STATE in config:
        conf = config[CONF_ECO_MODE_STATE]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_eco_mode_state_sensor(sens))

    if CONF_OTHER_MODE_STATE in config:
        conf = config[CONF_OTHER_MODE_STATE]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_other_mode_state_sensor(sens))

    if CONF_WATER_INBOUND_TEMPERATURE in config:
        conf = config[CONF_WATER_INBOUND_TEMPERATURE]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_water_incoming_temperature_sensor(sens))

    if CONF_FREON_OUTGOING_TEMPERATURE in config:
        conf = config[CONF_FREON_OUTGOING_TEMPERATURE]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_freon_outgoing_temperature_sensor(sens))

    if CONF_FREON_INBOUND_TEMPERATURE in config:
        conf = config[CONF_FREON_INBOUND_TEMPERATURE]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_freon_incoming_temperature_sensor(sens))

    if CONF_IBHW_TEMPERATURE in config:
        conf = config[CONF_IBHW_TEMPERATURE]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_ibhw_out_temperature_sensor(sens))

    if CONF_DEFROST_STATE in config:
        conf = config[CONF_DEFROST_STATE]
        sens = await binary_sensor.new_binary_sensor(conf)
        cg.add(var.set_defrost_state(sens))

    if CONF_INVERTER_POWER in config:
        conf = config[CONF_INVERTER_POWER]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_inverter_power_sensor(sens))

    if CONF_INVERTER_POWER_LIMIT_VALUE in config:
        conf = config[CONF_INVERTER_POWER_LIMIT_VALUE]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_inverter_power_limit_value_sensor(sens))

    if CONF_INVERTER_POWER_LIMIT_STATE in config:
        conf = config[CONF_INVERTER_POWER_LIMIT_STATE]
        sens = await binary_sensor.new_binary_sensor(conf)
        cg.add(var.set_inverter_power_limit_state_sensor(sens))

    if CONF_CO_POWER_STATE in config:
        conf = config[CONF_CO_POWER_STATE]
        sens = await binary_sensor.new_binary_sensor(conf)
        cg.add(var.set_co_status_sensor(sens))

    if CONF_CWU_POWER_STATE in config:
        conf = config[CONF_CWU_POWER_STATE]
        sens = await binary_sensor.new_binary_sensor(conf)
        cg.add(var.set_cwu_status_sensor(sens))

    if CONF_TARGET_CO_TEMPERATURE in config:
        conf = config[CONF_TARGET_CO_TEMPERATURE]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_target_co_temperature_sensor(sens))
    if CONF_TARGET_CWU_TEMPERATURE in config:
        conf = config[CONF_TARGET_CWU_TEMPERATURE]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_target_cwu_temperature_sensor(sens))

    if CONF_ECO_STATE in config:
        conf = config[CONF_ECO_STATE]
        sens = await binary_sensor.new_binary_sensor(conf)
        cg.add(var.set_eco_status_sensor(sens))

    if CONF_FAST_CWU_STATE in config:
        conf = config[CONF_FAST_CWU_STATE]
        sens = await binary_sensor.new_binary_sensor(conf)
        cg.add(var.set_fast_cwu_status_sensor(sens))
#unused
    if CONF_BI_BYTE2 in config:
        conf = config[CONF_BI_BYTE2]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_bi_byte2_sensor(sens))
    if CONF_BI_BYTE3 in config:
        conf = config[CONF_BI_BYTE3]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_bi_byte3_sensor(sens))

    if CONF_BI_BYTE5 in config:
        conf = config[CONF_BI_BYTE5]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_bi_byte5_sensor(sens))

    if CONF_BI_BYTE14 in config:
        conf = config[CONF_BI_BYTE14]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_bi_byte14_sensor(sens))

    if CONF_BI_BYTE15 in config:
        conf = config[CONF_BI_BYTE15]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_bi_byte15_sensor(sens))

    if CONF_BI_BYTE16 in config:
        conf = config[CONF_BI_BYTE16]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_bi_byte16_sensor(sens))

    if CONF_BI_BYTE18 in config:
        conf = config[CONF_BI_BYTE18]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_bi_byte18_sensor(sens))

    if CONF_BI_BYTE19 in config:
        conf = config[CONF_BI_BYTE19]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_bi_byte19_sensor(sens))

    if CONF_BI_BYTE20 in config:
        conf = config[CONF_BI_BYTE20]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_bi_byte20_sensor(sens))

    if CONF_BI_BYTE21 in config:
        conf = config[CONF_BI_BYTE21]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_bi_byte21_sensor(sens))



    cg.add(var.set_packet_timeout(config[CONF_TIMEOUT]))

POWER_LIMITATION_OFF_ACTION_SCHEMA = cv.Schema(
     {
         cv.Required(CONF_ID): cv.use_id(HeatPump),
         cv.Optional(CONF_LIMIT, default=HEATPUMP_POWER_LIMIT_MIN): validate_power_limit_range,
     }
 )
@automation.register_action(
    "aux_heatpump.power_limit_off", HeatPumpPowerLimitationOffAction, POWER_LIMITATION_OFF_ACTION_SCHEMA
)
async def power_limit_off_to_code(config, action_id, template_arg):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)

POWER_LIMITATION_ON_ACTION_SCHEMA = cv.Schema(
     {
         cv.Required(CONF_ID): cv.use_id(HeatPump),
         cv.Optional(CONF_LIMIT, default=HEATPUMP_POWER_LIMIT_MIN): validate_power_limit_range,
     }
 )

TURNING_STUFF_ON_OFF_ACTION_SCHEMA = maybe_simple_id(
    {
        cv.Required(CONF_ID): cv.use_id(HeatPump),
    }
)
@automation.register_action(
    "aux_heatpump.co_power_off", COPowerOffAction, TURNING_STUFF_ON_OFF_ACTION_SCHEMA
)
async def display_off_to_code(config, action_id, template_arg):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)

@automation.register_action(
    "aux_heatpump.co_power_on", COPowerOnAction, TURNING_STUFF_ON_OFF_ACTION_SCHEMA
)
async def co_power_on_to_code(config, action_id, template_arg):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)

@automation.register_action(
    "aux_heatpump.cwu_power_off", CWUPowerOffAction, TURNING_STUFF_ON_OFF_ACTION_SCHEMA
)
async def co_power_off_to_code(config, action_id, template_arg):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)

@automation.register_action(
    "aux_heatpump.cwu_power_on", CWUPowerOnAction, TURNING_STUFF_ON_OFF_ACTION_SCHEMA
)
async def cwu_power_on_to_code(config, action_id, template_arg):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)

@automation.register_action(
    "aux_heatpump.eco_mode_off", ECOOffAction, TURNING_STUFF_ON_OFF_ACTION_SCHEMA
)
async def eco_mode_off_to_code(config, action_id, template_arg):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)

@automation.register_action(
    "aux_heatpump.eco_mode_on", ECOOnAction, TURNING_STUFF_ON_OFF_ACTION_SCHEMA
)
async def eco_mode_on_to_code(config, action_id, template_arg):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)

@automation.register_action(
    "aux_heatpump.fast_cwu_off", Fast_CWUOffAction, TURNING_STUFF_ON_OFF_ACTION_SCHEMA
)
async def fast_cwu_off_to_code(config, action_id, template_arg):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)

@automation.register_action(
    "aux_heatpump.fast_cwu_on", Fast_CWUOnAction, TURNING_STUFF_ON_OFF_ACTION_SCHEMA
)
async def fast_cwu_on_to_code(config, action_id, template_arg):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)

@automation.register_action(
    "aux_heatpump.power_limit_on", HeatPumpPowerLimitationOnAction,
    POWER_LIMITATION_ON_ACTION_SCHEMA
)
async def power_limit_on_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    template_ = await cg.templatable(config[CONF_LIMIT], args, int)
    cg.add(var.set_value(template_))
    return var

# **************************************************************************************************
# IMPORTANT! For engineers only!
# Call the aux_heatpump.send_packet method only if you know what you're doing! It doesn't check the data, it just passes it on
# to the air conditioner everything as is. What effect will come from transmitting random bytes to the air conditioner, no one knows.
# You act at your own risk.
# **************************************************************************************************
SEND_TEST_PACKET_ACTION_SCHEMA = maybe_simple_id(
    {
        cv.Required(CONF_ID): cv.use_id(HeatPump),
        cv.Required(CONF_DATA): cv.templatable(validate_raw_data),
    }
)

@automation.register_action(
    "aux_heatpump.send_packet", HeatPumpSendTestPacketAction, SEND_TEST_PACKET_ACTION_SCHEMA
)
async def send_packet_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)

    data = config[CONF_DATA]
    if isinstance(data, bytes):
        data = list(data)

    if cg.is_template(data):
        templ = await cg.templatable(data, args, cg.std_vector.template(cg.uint8))
        cg.add(var.set_data_template(templ))
    else:
        cg.add(var.set_data_static(data))

    return var
