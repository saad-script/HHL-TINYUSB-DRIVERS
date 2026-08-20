#include "hhl_tusb_lamparray.h"
#include <stdint.h>
#include <string.h>

static uint32_t _current_lamp_id = 0;
static const hhl_tusb_lamparray_config_s* _lamparray_cfg = NULL;

static uint16_t _attr_report(uint8_t* buffer) {
    LampArrayAttributesReport report = {
        .lamp_count = _lamparray_cfg->lamp_count,
        .width_um = _lamparray_cfg->dimensions.width_um,
        .height_um = _lamparray_cfg->dimensions.height_um,
        .depth_um = _lamparray_cfg->dimensions.depth_um,
        .lamp_array_kind = _lamparray_cfg->kind,
        .min_update_interval_us = _lamparray_cfg->update_interval_us
    };

    memcpy(buffer, &report, sizeof(LampArrayAttributesReport));
    return sizeof(LampArrayAttributesReport);
}

static uint16_t _attr_res_report(uint8_t* buffer) {
    LampPosition lamp_pos;
    if (_lamparray_cfg->lamp_positions != NULL && _current_lamp_id < _lamparray_cfg->lamp_count) {
        lamp_pos = _lamparray_cfg->lamp_positions[_current_lamp_id];
    } else {
        lamp_pos = (LampPosition){_current_lamp_id, 0, 0};
    }

    LampPurposeFlags purpose_flags;
    if (_lamparray_cfg->lamp_purpose_flags == 0) {
        purpose_flags = _lamparray_cfg->lamp_purpose_flags;
    } else {
        purpose_flags = LAMP_PURPOSE_CONTROL | LAMP_PURPOSE_ILLUMINATION;
    }

    LampAttributesResponseReport report = {
        .lamp_id = _current_lamp_id,
        .lamp_position = lamp_pos,
        .update_latency = _lamparray_cfg->update_interval_us,
        .lamp_purposes = purpose_flags,
        .red_level_count = 255,
        .green_level_count = 255,
        .blue_level_count = 255,
        .intensity_level_count = 1,
        .is_programmable = 1,
        .input_binding = 0
    };

    memcpy(buffer, &report, sizeof(LampAttributesResponseReport));
    if (_lamparray_cfg->lamp_count > 0) {
        _current_lamp_id = (_current_lamp_id + 1) % _lamparray_cfg->lamp_count;
    } else {
        _current_lamp_id = 0;
    }

    return sizeof(LampAttributesResponseReport);
}

static void _attr_req_report(const uint8_t* buffer) {
    const LampAttributesRequestReport* report = (LampAttributesRequestReport*) buffer;
    _current_lamp_id = report->lamp_id < _lamparray_cfg->lamp_count ? report->lamp_id : 0;
}

static void _update_multi_report(const uint8_t* buffer) {
    if (_lamparray_cfg != NULL) {
        void (*hook)(const LampMultiUpdateReport* report) = _lamparray_cfg->hooks.update_multi_report;
        if (hook != NULL) {
            const LampMultiUpdateReport* report = (LampMultiUpdateReport*) buffer;
            hook(report);
        }
    }
}

static void _update_range_report(const uint8_t* buffer) {
    if (_lamparray_cfg != NULL) {
        void (*hook)(const LampRangeUpdateReport* report) = _lamparray_cfg->hooks.update_range_report;
        if (hook != NULL) {
            const LampRangeUpdateReport* report = (LampRangeUpdateReport*) buffer;
            hook(report);
        }
    }
}

static void _control_report(const uint8_t* buffer) {
    if (_lamparray_cfg != NULL) {
        void (*hook)(const LampArrayControlReport* report) = _lamparray_cfg->hooks.control_report;
        if (hook != NULL) {
            const LampArrayControlReport* report = (LampArrayControlReport*) buffer;
            hook(report);
        }
    }
}

void hhl_tusb_lamparray_init(const hhl_tusb_lamparray_config_s *cfg) {
    _lamparray_cfg = cfg;
}

bool hhl_tusb_lamparray_is_init() {
    return _lamparray_cfg != NULL;
}

uint16_t hhl_tusb_lamparray_handle_get_report(uint8_t report_id, hid_report_type_t report_type, uint8_t *buffer)
{
    if (_lamparray_cfg == NULL) {
        return 0;
    }

    // Must be FEATURE report according to LampArray specs
    if (report_type != HID_REPORT_TYPE_FEATURE)
        return 0;

    if (report_id == _lamparray_cfg->report_ids.attributes) {
        return _attr_report(buffer);
    } else if (report_id == _lamparray_cfg->report_ids.attributes_response) {
        return _attr_res_report(buffer);
    }

    return 0;
}

void hhl_tusb_lamparray_handle_set_report(uint8_t report_id, hid_report_type_t report_type, uint8_t const *buffer, uint16_t bufsize) {
    if (_lamparray_cfg == NULL) {
        return;
    }

    // Must be either FEATURE or OUTPUT report according to LampArray specs
    if (report_type != HID_REPORT_TYPE_FEATURE && report_type != HID_REPORT_TYPE_OUTPUT)
        return;

    if (bufsize == 0)
        return;

    // For some reason TinyUSB embeds the report id directly into the buffer for OUTPUT reports only
    if (!report_id && report_type == HID_REPORT_TYPE_OUTPUT) {
        report_id = buffer[0];
        buffer++;
        bufsize--;
    }

    if (report_id == _lamparray_cfg->report_ids.attributes_request) {
        _attr_req_report(buffer);
    } else if (report_id == _lamparray_cfg->report_ids.multi_update) {
        _update_multi_report(buffer);
    } else if (report_id == _lamparray_cfg->report_ids.range_update) {
        _update_range_report(buffer);
    } else if (report_id == _lamparray_cfg->report_ids.control) {
        _control_report(buffer);
    }
}

uint32_t hhl_tusb_lamparray_get_report_desc(uint8_t* data) {
    if (_lamparray_cfg == NULL) {
        return 0;
    }

    const uint8_t report_desc[] = {
        HHL_TUSB_HID_REPORT_DESC_LIGHTING(
            _lamparray_cfg->report_ids.attributes,
            _lamparray_cfg->report_ids.attributes_request,
            _lamparray_cfg->report_ids.attributes_response,
            _lamparray_cfg->report_ids.multi_update,
            _lamparray_cfg->report_ids.range_update,
            _lamparray_cfg->report_ids.control
        )
    };

    if (data != NULL) {
        memcpy(data, report_desc, sizeof(report_desc));
    }

    return sizeof(report_desc);
}
