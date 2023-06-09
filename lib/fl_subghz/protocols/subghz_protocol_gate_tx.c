#include "subghz_protocol_gate_tx.h"


struct SubGhzProtocolGateTX {
    SubGhzProtocolCommon common;
};

SubGhzProtocolGateTX* subghz_protocol_gate_tx_alloc(void) {
    SubGhzProtocolGateTX* instance = furi_alloc(sizeof(SubGhzProtocolGateTX));

    instance->common.name = "GateTX";
    instance->common.code_min_count_bit_for_found = 24;
    instance->common.te_shot = 350;
    instance->common.te_long = 700;
    instance->common.te_delta = 100;
    instance->common.to_string = (SubGhzProtocolCommonToStr)subghz_protocol_gate_tx_to_str;

    return instance;
}

void subghz_protocol_gate_tx_free(SubGhzProtocolGateTX* instance) {
    furi_assert(instance);
    free(instance);
}

/** Send bit 
 * 
 * @param instance - SubGhzProtocolGateTX instance
 * @param bit - bit
 */
void subghz_protocol_gate_tx_send_bit(SubGhzProtocolGateTX* instance, uint8_t bit) {
    if (bit) {
        //send bit 1
        SUBGHZ_TX_PIN_LOW();
        delay_us(instance->common.te_long);
        SUBGHZ_TX_PIN_HIGTH();
        delay_us(instance->common.te_shot);
    } else {
        //send bit 0
        SUBGHZ_TX_PIN_LOW();
        delay_us(instance->common.te_shot);
        SUBGHZ_TX_PIN_HIGTH();
        delay_us(instance->common.te_long);
    }
}

void subghz_protocol_gate_tx_send_key(SubGhzProtocolGateTX* instance, uint64_t key, uint8_t bit,uint8_t repeat) {
    while (repeat--) {
        //Send header
        SUBGHZ_TX_PIN_LOW();
        delay_us(instance->common.te_shot * 47); //+2 interval v bit 1
        //Send start bit
        SUBGHZ_TX_PIN_HIGTH();
        delay_us(instance->common.te_long);
        //Send key data
        for (uint8_t i = bit; i > 0; i--) {
            subghz_protocol_gate_tx_send_bit(instance, bit_read(key, i - 1));
        }
    }
}

void subghz_protocol_gate_tx_reset(SubGhzProtocolGateTX* instance) {
    instance->common.parser_step = 0;
}

/** Analysis of received data
 * 
 * @param instance SubGhzProtocolFaacSLH instance
 */
void subghz_protocol_gate_tx_check_remote_controller(SubGhzProtocolGateTX* instance) {
    uint32_t code_found_reverse = subghz_protocol_common_reverse_key(instance->common.code_found, instance->common.code_count_bit);

    instance->common.serial = (code_found_reverse & 0xFF) << 12 | ((code_found_reverse >>8) & 0xFF) << 4 | ((code_found_reverse >>20) & 0x0F) ;
    instance->common.btn = ((code_found_reverse >> 16) & 0x0F);

    if (instance->common.callback) instance->common.callback((SubGhzProtocolCommon*)instance, instance->common.context);

}

void subghz_protocol_gate_tx_parse(SubGhzProtocolGateTX* instance, bool level, uint32_t duration) {
    switch (instance->common.parser_step) {
    case 0:
        if ((!level)
                && (DURATION_DIFF(duration,instance->common.te_shot * 47)< instance->common.te_delta * 47)) {
            //Found Preambula
            instance->common.parser_step = 1;
        } else {
            instance->common.parser_step = 0;
        }
        break;
    case 1:
        if (level && ((DURATION_DIFF(duration,instance->common.te_long)< instance->common.te_delta*3))){
            //Found start bit
            instance->common.parser_step = 2;
            instance->common.code_found = 0;
            instance->common.code_count_bit = 0;
        } else {
            instance->common.parser_step = 0;
        }
        break;
    case 2:
        if (!level) {
            if (duration >= (instance->common.te_shot * 10 + instance->common.te_delta)) {
                instance->common.parser_step = 1;
                if (instance->common.code_count_bit>= instance->common.code_min_count_bit_for_found) {
                    subghz_protocol_gate_tx_check_remote_controller(instance);
                }
                instance->common.code_found = 0;
                instance->common.code_count_bit = 0;
                break;
            } else {
                instance->common.te_last = duration;
                instance->common.parser_step = 3;
            }
        }
         break;
    case 3:
        if(level){
            if ((DURATION_DIFF(instance->common.te_last,instance->common.te_shot)< instance->common.te_delta)
                    && (DURATION_DIFF(duration,instance->common.te_long)< instance->common.te_delta*3)) {
                subghz_protocol_common_add_bit(&instance->common, 0);
                instance->common.parser_step = 2;
            } else if ((DURATION_DIFF(instance->common.te_last,instance->common.te_long)< instance->common.te_delta*3)
                    && (DURATION_DIFF(duration,instance->common.te_shot)< instance->common.te_delta)) {
                subghz_protocol_common_add_bit(&instance->common, 1);
                instance->common.parser_step = 2;
            } else {
                instance->common.parser_step = 0;
            }
        }else{
            instance->common.parser_step = 0;
        }
        break;
    }
}

void subghz_protocol_gate_tx_to_str(SubGhzProtocolGateTX* instance, string_t output) {
    
   // uint64_t code_found_reverse = subghz_protocol_common_reverse_key(instance->common.code_found, instance->common.code_count_bit);
   // uint32_t code_fix = code_found_reverse & 0xFFFFFFFF;
   // uint32_t code_hop = (code_found_reverse >>32) & 0xFFFFFFFF;

    //uint32_t rev_hi =

    string_cat_printf(output,
                      "Protocol %s, %d Bit\r\n"
                      " KEY:%06lX\r\n"
                      " SN:%05lX  BTN:%lX\r\n",
                      instance->common.name,
                      instance->common.code_count_bit,
                      (uint32_t)(instance->common.code_found & 0xFFFFFF),
                      instance->common.serial, 
                      instance->common.btn);
}