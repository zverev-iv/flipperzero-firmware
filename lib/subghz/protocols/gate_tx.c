#include "gate_tx.h"

#include "../blocks/const.h"
#include "../blocks/decoder.h"
#include "../blocks/encoder.h"
#include "../blocks/generic.h"
#include "../blocks/math.h"

#define TAG "SubGhzProtocolGateTx"

static const SubGhzBlockConst subghz_protocol_gate_tx_const = {
    .te_short = 350,
    .te_long = 700,
    .te_delta = 100,
    .min_count_bit_for_found = 24,
};

struct SubGhzProtocolDecoderGateTx {
    SubGhzProtocolDecoderBase base;

    SubGhzBlockDecoder decoder;
    SubGhzBlockGeneric generic;
};

struct SubGhzProtocolEncoderGateTx {
    SubGhzProtocolEncoderBase base;

    SubGhzProtocolBlockEncoder encoder;
    SubGhzBlockGeneric generic;
};

typedef enum {
    GateTXDecoderStepReset = 0,
    GateTXDecoderStepFoundStartBit,
    GateTXDecoderStepSaveDuration,
    GateTXDecoderStepCheckDuration,
} GateTXDecoderStep;

const SubGhzProtocolDecoder subghz_protocol_gate_tx_decoder = {
    .alloc = subghz_protocol_decoder_gate_tx_alloc,
    .free = subghz_protocol_decoder_gate_tx_free,

    .feed = subghz_protocol_decoder_gate_tx_feed,
    .reset = subghz_protocol_decoder_gate_tx_reset,

    .serialize = subghz_protocol_decoder_gate_tx_serialization,
    .save_file = subghz_protocol_gate_tx_save_file,
};

const SubGhzProtocolEncoder subghz_protocol_gate_tx_encoder = {
    .alloc = subghz_protocol_encoder_gate_tx_alloc,
    .free = subghz_protocol_encoder_gate_tx_free,

    .load = subghz_protocol_encoder_gate_tx_load,
    .stop = subghz_protocol_encoder_gate_tx_stop,
    .yield = subghz_protocol_encoder_gate_tx_yield,
    .load_file = subghz_protocol_gate_tx_load_file,
};

const SubGhzProtocol subghz_protocol_gate_tx = {
    .name = SUBGHZ_PROTOCOL_GATE_TX_NAME,
    .type = SubGhzProtocolCommonTypeStatic_,

    .decoder = &subghz_protocol_gate_tx_decoder,
    .encoder = &subghz_protocol_gate_tx_encoder,
};

void* subghz_protocol_encoder_gate_tx_alloc(SubGhzEnvironment* environment) {
    SubGhzProtocolEncoderGateTx* instance = furi_alloc(sizeof(SubGhzProtocolEncoderGateTx));

    instance->base.protocol = &subghz_protocol_gate_tx;
    instance->generic.protocol_name = instance->base.protocol->name;

    instance->encoder.repeat = 10;
    instance->encoder.size_upload = 52; //max 24bit*2 + 2 (start, stop)
    instance->encoder.upload = furi_alloc(instance->encoder.size_upload * sizeof(LevelDuration));
    instance->encoder.is_runing = false;
    return instance;
}

void subghz_protocol_encoder_gate_tx_free(void* context) {
    furi_assert(context);
    SubGhzProtocolEncoderGateTx* instance = context;
    free(instance->encoder.upload);
    free(instance);
}

static bool subghz_protocol_gate_tx_encoder_get_upload(SubGhzProtocolEncoderGateTx* instance) {
    furi_assert(instance);
    size_t index = 0;
    size_t size_upload = (instance->generic.data_count_bit * 2) + 2;
    if(size_upload > instance->encoder.size_upload) {
        FURI_LOG_E(TAG, "Size upload exceeds allocated encoder buffer.");
        return false;
    } else {
        instance->encoder.size_upload = size_upload;
    }
    //Send header
    instance->encoder.upload[index++] =
        level_duration_make(false, (uint32_t)subghz_protocol_gate_tx_const.te_short * 49);
    //Send start bit
    instance->encoder.upload[index++] =
        level_duration_make(true, (uint32_t)subghz_protocol_gate_tx_const.te_long);
    //Send key data
    for(uint8_t i = instance->generic.data_count_bit; i > 0; i--) {
        if(bit_read(instance->generic.data, i - 1)) {
            //send bit 1
            instance->encoder.upload[index++] =
                level_duration_make(false, (uint32_t)subghz_protocol_gate_tx_const.te_long);
            instance->encoder.upload[index++] =
                level_duration_make(true, (uint32_t)subghz_protocol_gate_tx_const.te_short);
        } else {
            //send bit 0
            instance->encoder.upload[index++] =
                level_duration_make(false, (uint32_t)subghz_protocol_gate_tx_const.te_short);
            instance->encoder.upload[index++] =
                level_duration_make(true, (uint32_t)subghz_protocol_gate_tx_const.te_long);
        }
    }
    return true;
}

bool subghz_protocol_encoder_gate_tx_load(
    void* context,
    uint64_t key,
    uint8_t count_bit,
    size_t repeat) {
    furi_assert(context);
    SubGhzProtocolEncoderGateTx* instance = context;
    instance->generic.data = key;
    instance->generic.data_count_bit = count_bit;
    instance->encoder.repeat = repeat;
    subghz_protocol_gate_tx_encoder_get_upload(instance);
    instance->encoder.is_runing = true;
    return true;
}

void subghz_protocol_encoder_gate_tx_stop(void* context) {
    SubGhzProtocolEncoderGateTx* instance = context;
    instance->encoder.is_runing = false;
}

LevelDuration subghz_protocol_encoder_gate_tx_yield(void* context) {
    SubGhzProtocolEncoderGateTx* instance = context;

    if(instance->encoder.repeat == 0 || !instance->encoder.is_runing) {
        instance->encoder.is_runing = false;
        return level_duration_reset();
    }

    LevelDuration ret = instance->encoder.upload[instance->encoder.front];

    if(++instance->encoder.front == instance->encoder.size_upload) {
        instance->encoder.repeat--;
        instance->encoder.front = 0;
    }

    return ret;
}

void* subghz_protocol_decoder_gate_tx_alloc(SubGhzEnvironment* environment) {
    SubGhzProtocolDecoderGateTx* instance = furi_alloc(sizeof(SubGhzProtocolDecoderGateTx));
    instance->base.protocol = &subghz_protocol_gate_tx;
    instance->generic.protocol_name = instance->base.protocol->name;
    return instance;
}

void subghz_protocol_decoder_gate_tx_free(void* context) {
    furi_assert(context);
    SubGhzProtocolDecoderGateTx* instance = context;
    free(instance);
}

void subghz_protocol_decoder_gate_tx_reset(void* context) {
    furi_assert(context);
    SubGhzProtocolDecoderGateTx* instance = context;
    instance->decoder.parser_step = GateTXDecoderStepReset;
}

void subghz_protocol_decoder_gate_tx_feed(void* context, bool level, uint32_t duration) {
    furi_assert(context);
    SubGhzProtocolDecoderGateTx* instance = context;

    switch(instance->decoder.parser_step) {
    case GateTXDecoderStepReset:
        if((!level) && (DURATION_DIFF(duration, subghz_protocol_gate_tx_const.te_short * 47) <
                        subghz_protocol_gate_tx_const.te_delta * 47)) {
            //Found Preambula
            instance->decoder.parser_step = GateTXDecoderStepFoundStartBit;
        }
        break;
    case GateTXDecoderStepFoundStartBit:
        if(level && ((DURATION_DIFF(duration, subghz_protocol_gate_tx_const.te_long) <
                      subghz_protocol_gate_tx_const.te_delta * 3))) {
            //Found start bit
            instance->decoder.parser_step = GateTXDecoderStepSaveDuration;
            instance->decoder.decode_data = 0;
            instance->decoder.decode_count_bit = 0;
        } else {
            instance->decoder.parser_step = GateTXDecoderStepReset;
        }
        break;
    case GateTXDecoderStepSaveDuration:
        if(!level) {
            if(duration >= (subghz_protocol_gate_tx_const.te_short * 10 +
                            subghz_protocol_gate_tx_const.te_delta)) {
                instance->decoder.parser_step = GateTXDecoderStepFoundStartBit;
                if(instance->decoder.decode_count_bit >=
                   subghz_protocol_gate_tx_const.min_count_bit_for_found) {
                    instance->generic.data = instance->decoder.decode_data;
                    instance->generic.data_count_bit = instance->decoder.decode_count_bit;

                    if(instance->base.callback)
                        instance->base.callback(&instance->base, instance->base.context);
                }
                instance->decoder.decode_data = 0;
                instance->decoder.decode_count_bit = 0;
                break;
            } else {
                instance->decoder.te_last = duration;
                instance->decoder.parser_step = GateTXDecoderStepCheckDuration;
            }
        }
        break;
    case GateTXDecoderStepCheckDuration:
        if(level) {
            if((DURATION_DIFF(instance->decoder.te_last, subghz_protocol_gate_tx_const.te_short) <
                subghz_protocol_gate_tx_const.te_delta) &&
               (DURATION_DIFF(duration, subghz_protocol_gate_tx_const.te_long) <
                subghz_protocol_gate_tx_const.te_delta * 3)) {
                subghz_protocol_blocks_add_bit(&instance->decoder, 0);
                instance->decoder.parser_step = GateTXDecoderStepSaveDuration;
            } else if(
                (DURATION_DIFF(instance->decoder.te_last, subghz_protocol_gate_tx_const.te_long) <
                 subghz_protocol_gate_tx_const.te_delta * 3) &&
                (DURATION_DIFF(duration, subghz_protocol_gate_tx_const.te_short) <
                 subghz_protocol_gate_tx_const.te_delta)) {
                subghz_protocol_blocks_add_bit(&instance->decoder, 1);
                instance->decoder.parser_step = GateTXDecoderStepSaveDuration;
            } else {
                instance->decoder.parser_step = GateTXDecoderStepReset;
            }
        } else {
            instance->decoder.parser_step = GateTXDecoderStepReset;
        }
        break;
    }
}

static void subghz_protocol_gate_tx_check_remote_controller(
    SubGhzBlockGeneric* instance) {
    uint32_t code_found_reverse = subghz_protocol_blocks_reverse_key(
        instance->data, instance->data_count_bit);

    instance->serial = (code_found_reverse & 0xFF) << 12 |
                               ((code_found_reverse >> 8) & 0xFF) << 4 |
                               ((code_found_reverse >> 20) & 0x0F);
    instance->btn = ((code_found_reverse >> 16) & 0x0F);
}

void subghz_protocol_decoder_gate_tx_serialization(void* context, string_t output) {
    furi_assert(context);
    SubGhzProtocolDecoderGateTx* instance = context;
    subghz_protocol_gate_tx_check_remote_controller(&instance->generic);
    string_cat_printf(
        output,
        "%s %dbit\r\n"
        "Key:%06lX\r\n"
        "Sn:%05lX  Btn:%lX\r\n",
        instance->generic.protocol_name,
        instance->generic.data_count_bit,
        (uint32_t)(instance->generic.data & 0xFFFFFF),
        instance->generic.serial,
        instance->generic.btn);
}

bool subghz_protocol_gate_tx_save_file(void* context, FlipperFile* flipper_file) {
    furi_assert(context);
    SubGhzProtocolDecoderGateTx* instance = context;
    return subghz_block_generic_save_file(&instance->generic, flipper_file);
}

bool subghz_protocol_gate_tx_load_file(
    void* context,
    FlipperFile* flipper_file,
    const char* file_path) {
    furi_assert(context);
    SubGhzProtocolEncoderGateTx* instance = context;
    return subghz_block_generic_load_file(&instance->generic, flipper_file);
}

// void subghz_decoder_gate_tx_to_load_protocol(SubGhzProtocolGateTX* instance, void* context) {
//     furi_assert(context);
//     furi_assert(instance);
//     SubGhzProtocolCommonLoad* data = context;
//     instance->generic.data = data->code_found;
//     instance->generic.data_count_bit = data->code_count_bit;
//     subghz_protocol_gate_tx_check_remote_controller(instance);
// }