#include "ido.h"

#include "../blocks/const.h"
#include "../blocks/decoder.h"
#include "../blocks/encoder.h"
#include "../blocks/generic.h"
#include "../blocks/math.h"

#define TAG "SubGhzProtocol_iDo_117/111"

static const SubGhzBlockConst subghz_protocol_ido_const = {
    .te_short = 450,
    .te_long = 1450,
    .te_delta = 150,
    .min_count_bit_for_found = 48,
};

struct SubGhzProtocolDecoderIDo {
    SubGhzProtocolDecoderBase base;

    SubGhzBlockDecoder decoder;
    SubGhzBlockGeneric generic;
};

struct SubGhzProtocolEncoderIDo {
    SubGhzProtocolEncoderBase base;

    SubGhzProtocolBlockEncoder encoder;
    SubGhzBlockGeneric generic;
};

typedef enum {
    IDoDecoderStepReset = 0,
    IDoDecoderStepFoundPreambula,
    IDoDecoderStepSaveDuration,
    IDoDecoderStepCheckDuration,
} IDoDecoderStep;

const SubGhzProtocolDecoder subghz_protocol_ido_decoder = {
    .alloc = subghz_protocol_decoder_ido_alloc,
    .free = subghz_protocol_decoder_ido_free,

    .feed = subghz_protocol_decoder_ido_feed,
    .reset = subghz_protocol_decoder_ido_reset,

    .serialize = subghz_protocol_decoder_ido_serialization,
    .save_file = NULL,
};

const SubGhzProtocolEncoder subghz_protocol_ido_encoder = {
    .alloc = NULL,
    .free = NULL,

    .load = NULL,
    .stop = NULL,
    .yield = NULL,
    .load_file = NULL,
};

const SubGhzProtocol subghz_protocol_ido = {
    .name = SUBGHZ_PROTOCOL_IDO_NAME,
    .type = SubGhzProtocolCommonTypeDynamic_,

    .decoder = &subghz_protocol_ido_decoder,
    .encoder = &subghz_protocol_ido_encoder,
};

void* subghz_protocol_decoder_ido_alloc(SubGhzEnvironment* environment) {
    SubGhzProtocolDecoderIDo* instance = furi_alloc(sizeof(SubGhzProtocolDecoderIDo));
    instance->base.protocol = &subghz_protocol_ido;
    instance->generic.protocol_name = instance->base.protocol->name;

    return instance;
}

void subghz_protocol_decoder_ido_free(void* context) {
    furi_assert(context);
    SubGhzProtocolDecoderIDo* instance = context;
    free(instance);
}

void subghz_protocol_decoder_ido_reset(void* context) {
    furi_assert(context);
    SubGhzProtocolDecoderIDo* instance = context;
    instance->decoder.parser_step = IDoDecoderStepReset;
}

void subghz_protocol_decoder_ido_feed(void* context, bool level, uint32_t duration) {
    furi_assert(context);
    SubGhzProtocolDecoderIDo* instance = context;
    
    switch(instance->decoder.parser_step) {
    case IDoDecoderStepReset:
        if((level) && (DURATION_DIFF(duration, subghz_protocol_ido_const.te_short * 10) <
                       subghz_protocol_ido_const.te_delta * 5)) {
            instance->decoder.parser_step = IDoDecoderStepFoundPreambula;
        }
        break;
    case IDoDecoderStepFoundPreambula:
        if((!level) && (DURATION_DIFF(duration, subghz_protocol_ido_const.te_short * 10) <
                        subghz_protocol_ido_const.te_delta * 5)) {
            //Found Preambula
            instance->decoder.parser_step = IDoDecoderStepSaveDuration;
            instance->decoder.decode_data = 0;
            instance->decoder.decode_count_bit = 0;
        } else {
            instance->decoder.parser_step = IDoDecoderStepReset;
        }
        break;
    case IDoDecoderStepSaveDuration:
        if(level) {
            if(duration >= (subghz_protocol_ido_const.te_short * 5 + subghz_protocol_ido_const.te_delta)) {
                instance->decoder.parser_step = IDoDecoderStepFoundPreambula;
                if(instance->decoder.decode_count_bit>=
                   subghz_protocol_ido_const.min_count_bit_for_found) {
                    instance->generic.data = instance->decoder.decode_data;
                    instance->generic.data_count_bit  = instance->decoder.decode_count_bit;
                    if(instance->base.callback)
                        instance->base.callback(&instance->base, instance->base.context);
                }
                instance->decoder.decode_data = 0;
                instance->decoder.decode_count_bit = 0;
                break;
            } else {
                instance->decoder.te_last = duration;
                instance->decoder.parser_step = IDoDecoderStepCheckDuration;
            }

        } else {
            instance->decoder.parser_step = IDoDecoderStepReset;
        }
        break;
    case IDoDecoderStepCheckDuration:
        if(!level) {
            if((DURATION_DIFF(instance->decoder.te_last, subghz_protocol_ido_const.te_short) <
                subghz_protocol_ido_const.te_delta) &&
               (DURATION_DIFF(duration, subghz_protocol_ido_const.te_long) <
                subghz_protocol_ido_const.te_delta * 3)) {
                subghz_protocol_blocks_add_bit(&instance->decoder, 0);
                instance->decoder.parser_step = IDoDecoderStepSaveDuration;
            } else if(
                (DURATION_DIFF(instance->decoder.te_last, subghz_protocol_ido_const.te_short) <
                 subghz_protocol_ido_const.te_delta * 3) &&
                (DURATION_DIFF(duration, subghz_protocol_ido_const.te_short) < subghz_protocol_ido_const.te_delta)) {
                subghz_protocol_blocks_add_bit(&instance->decoder, 1);
                instance->decoder.parser_step = IDoDecoderStepSaveDuration;
            } else {
                instance->decoder.parser_step = IDoDecoderStepReset;
            }
        } else {
            instance->decoder.parser_step = IDoDecoderStepReset;
        }
        break;
    }
}


static void subghz_protocol_ido_check_remote_controller(
    SubGhzBlockGeneric* instance) {

    uint64_t code_found_reverse = subghz_protocol_blocks_reverse_key(
        instance->data, instance->data_count_bit);
    uint32_t code_fix = code_found_reverse & 0xFFFFFF;

    instance->serial = code_fix & 0xFFFFF;
    instance->btn = (code_fix >> 20) & 0x0F;
}



void subghz_protocol_decoder_ido_serialization(void* context, string_t output) {
    furi_assert(context);
    SubGhzProtocolDecoderIDo* instance = context;

    subghz_protocol_ido_check_remote_controller(&instance->generic);
    uint64_t code_found_reverse = subghz_protocol_blocks_reverse_key(
        instance->generic.data, instance->generic.data_count_bit);
    uint32_t code_fix = code_found_reverse & 0xFFFFFF;
    uint32_t code_hop = (code_found_reverse >> 24) & 0xFFFFFF;

    string_cat_printf(
        output,
        "%s %dbit\r\n"
        "Key:0x%lX%08lX\r\n"
        "Fix:%06lX \r\n"
        "Hop:%06lX \r\n"
        "Sn:%05lX Btn:%lX\r\n",
        instance->generic.protocol_name,
        instance->generic.data_count_bit,
        (uint32_t)(instance->generic.data >> 32),
        (uint32_t)instance->generic.data,
        code_fix,
        code_hop,
        instance->generic.serial,
        instance->generic.btn);
}

// void subghz_decoder_ido_to_load_protocol(SubGhzProtocolIDo* instance, void* context) {
//     furi_assert(context);
//     furi_assert(instance);
//     SubGhzProtocolCommonLoad* data = context;
//     instance->common.code_last_found = data->code_found;
//     instance->common.code_last_count_bit = data->code_count_bit;
//     subghz_protocol_ido_check_remote_controller(instance);
// }