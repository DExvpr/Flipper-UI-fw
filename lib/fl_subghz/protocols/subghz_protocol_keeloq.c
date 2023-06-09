#include "subghz_protocol_keeloq.h"
#include "subghz_protocol_keeloq_common.h"

#include "../subghz_keystore.h"

#include <furi.h>

#include <m-string.h>

struct SubGhzProtocolKeeloq {
    SubGhzProtocolCommon common;
    SubGhzKeystore* keystore;
    const char* manufacture_name;
};

SubGhzProtocolKeeloq* subghz_protocol_keeloq_alloc(SubGhzKeystore* keystore) {
    SubGhzProtocolKeeloq* instance = furi_alloc(sizeof(SubGhzProtocolKeeloq));

    instance->keystore = keystore;

    instance->common.name = "KeeLoq";
    instance->common.code_min_count_bit_for_found = 64;
    instance->common.te_shot = 400;
    instance->common.te_long = 800;
    instance->common.te_delta = 140;
    instance->common.to_string = (SubGhzProtocolCommonToStr)subghz_protocol_keeloq_to_str;

    return instance;
}

void subghz_protocol_keeloq_free(SubGhzProtocolKeeloq* instance) {
    furi_assert(instance);
    free(instance);
}

/** Checking the accepted code against the database manafacture key
 * 
 * @param instance SubGhzProtocolKeeloq instance
 * @param fix fix part of the parcel
 * @param hop hop encrypted part of the parcel
 * @return true on successful search
 */
uint8_t subghz_protocol_keeloq_check_remote_controller_selector(SubGhzProtocolKeeloq* instance, uint32_t fix , uint32_t hop) {
    uint16_t end_serial = (uint16_t)(fix&0x3FF);
    uint8_t btn = (uint8_t)(fix>>28);
    uint32_t decrypt = 0;
    uint64_t man_normal_learning;

    for
        M_EACH(manufacture_code, *subghz_keystore_get_data(instance->keystore), SubGhzKeyArray_t) {
            switch (manufacture_code->type){
                case KEELOQ_LEARNING_SIMPLE:
                    //Simple Learning
                    decrypt = subghz_protocol_keeloq_common_decrypt(hop, manufacture_code->key);
                    if((decrypt>>28 == btn) && ((((uint16_t)(decrypt>>16)) & 0x3FF) == end_serial)){
                        instance->manufacture_name = string_get_cstr(manufacture_code->name);
                        instance->common.cnt = decrypt & 0x0000FFFF;
                        return 1;
                    }
                break;
                case KEELOQ_LEARNING_NORMAL:
                    // Normal_Learning
                    // https://phreakerclub.com/forum/showpost.php?p=43557&postcount=37
                    man_normal_learning = subghz_protocol_keeloq_common_normal_learning(fix, manufacture_code->key);
                    decrypt=subghz_protocol_keeloq_common_decrypt(hop, man_normal_learning);
                    if( (decrypt>>28 ==btn)&& ((((uint16_t)(decrypt>>16))&0x3FF)==end_serial)){ 
                        instance->manufacture_name = string_get_cstr(manufacture_code->name);
                        instance->common.cnt = decrypt & 0x0000FFFF;
                        return 1;
                    }
                break;
                case KEELOQ_LEARNING_UNKNOWN:
                    // Simple Learning
                    decrypt=subghz_protocol_keeloq_common_decrypt(hop, manufacture_code->key);
                    if( (decrypt>>28 ==btn) && ((((uint16_t)(decrypt>>16))&0x3FF)==end_serial)){
                        instance->manufacture_name = string_get_cstr(manufacture_code->name);
                        instance->common.cnt = decrypt & 0x0000FFFF;
                        return 1;
                    }
                    // Check for mirrored man
                    uint64_t man_rev=0;
                    uint64_t man_rev_byte=0;
                    for(uint8_t i=0; i<64; i+=8){
                        man_rev_byte=(uint8_t)(manufacture_code->key >> i);
                        man_rev = man_rev  | man_rev_byte << (56-i);
                    }
                    decrypt=subghz_protocol_keeloq_common_decrypt(hop, man_rev);
                    if( (decrypt>>28 ==btn) && ((((uint16_t)(decrypt>>16))&0x3FF)==end_serial)){
                      instance->manufacture_name = string_get_cstr(manufacture_code->name);
                      instance->common.cnt= decrypt&0x0000FFFF;
                      return 1;
                    }
                    //###########################
                    // Normal_Learning
                    // https://phreakerclub.com/forum/showpost.php?p=43557&postcount=37
                    man_normal_learning = subghz_protocol_keeloq_common_normal_learning(fix, manufacture_code->key);
                    decrypt=subghz_protocol_keeloq_common_decrypt(hop, man_normal_learning);
                    if( (decrypt>>28 ==btn)&& ((((uint16_t)(decrypt>>16))&0x3FF)==end_serial)){
                        instance->manufacture_name = string_get_cstr(manufacture_code->name);
                        instance->common.cnt= decrypt&0x0000FFFF;
                        return 1;
                    }
                    // Check for mirrored man
                    man_rev=0;
                    man_rev_byte=0;
                    for(uint8_t i=0; i<64; i+=8){
                        man_rev_byte = (uint8_t)(manufacture_code->key >> i);
                        man_rev = man_rev  | man_rev_byte << (56-i);
                    }
                    man_normal_learning = subghz_protocol_keeloq_common_normal_learning(fix, man_rev);
                    decrypt=subghz_protocol_keeloq_common_decrypt(hop, man_normal_learning);
                    if( (decrypt>>28 ==btn) && ((((uint16_t)(decrypt>>16))&0x3FF)==end_serial)){
                        instance->manufacture_name = string_get_cstr(manufacture_code->name);
                        instance->common.cnt= decrypt&0x0000FFFF;
                        return 1;
                    }
                break;
            }
        }

    instance->manufacture_name = "Unknown";
    instance->common.cnt=0;

    return 0;
}

/** Analysis of received data
 * 
 * @param instance SubGhzProtocolKeeloq instance
 */
void subghz_protocol_keeloq_check_remote_controller(SubGhzProtocolKeeloq* instance) {
    uint64_t key = subghz_protocol_common_reverse_key(instance->common.code_found, instance->common.code_count_bit);
    uint32_t key_fix = key >> 32;
    uint32_t key_hop = key & 0x00000000ffffffff;
    // Check key AN-Motors
    if((key_hop >> 24) == ((key_hop>>16)&0x00ff) && (key_fix>>28) ==((key_hop>>12)&0x0f) && (key_hop & 0xFFF ) == 0x404){
        instance->manufacture_name = "AN-Motors";
        instance->common.cnt = key_hop>>16;
    } else if((key_hop & 0xFFF) == (0x000) && (key_fix>>28) ==((key_hop>>12)&0x0f) ){
        instance->manufacture_name = "HCS101";
        instance->common.cnt = key_hop>>16;
    } else {
        subghz_protocol_keeloq_check_remote_controller_selector(instance, key_fix, key_hop);
    }
    instance ->common.serial= key_fix&0x0FFFFFFF;
    instance->common.btn = key_fix >> 28;
    if (instance->common.callback) instance->common.callback((SubGhzProtocolCommon*)instance, instance->common.context);
}

/** Send bit 
 * 
 * @param instance - SubGhzProtocolKeeloq instance
 * @param bit - bit
 */
void subghz_protocol_keeloq_send_bit(SubGhzProtocolKeeloq* instance, uint8_t bit) {
    if (bit) {
        // send bit 1
        SUBGHZ_TX_PIN_HIGTH();
        delay_us(instance->common.te_shot);
        SUBGHZ_TX_PIN_LOW();
        delay_us(instance->common.te_long);
    } else {
        // send bit 0
        SUBGHZ_TX_PIN_HIGTH();
        delay_us(instance->common.te_long);
        SUBGHZ_TX_PIN_LOW();
        delay_us(instance->common.te_shot);
    }
}

void subghz_protocol_keeloq_send_key(SubGhzProtocolKeeloq* instance, uint64_t key, uint8_t bit, uint8_t repeat) {
    while (repeat--) {
        // Send header
        for (uint8_t i = 11; i > 0; i--) {
            SUBGHZ_TX_PIN_HIGTH();
            delay_us(instance->common.te_shot);
            SUBGHZ_TX_PIN_LOW();
            delay_us(instance->common.te_shot);
        }
        delay_us(instance->common.te_shot * 9); //+1 up Send header

        for (uint8_t i = bit; i > 0; i--) {
            subghz_protocol_keeloq_send_bit(instance, bit_read(key, i - 1));
        }
        // +send 2 status bit
        subghz_protocol_keeloq_send_bit(instance, 0);
        subghz_protocol_keeloq_send_bit(instance, 0);
        // send end
        subghz_protocol_keeloq_send_bit(instance, 0);
        delay_us(instance->common.te_shot * 2);   //+2 interval END SEND
    }
}

void subghz_protocol_keeloq_reset(SubGhzProtocolKeeloq* instance) {
    instance->common.parser_step = 0;
}

void subghz_protocol_keeloq_parse(SubGhzProtocolKeeloq* instance, bool level, uint32_t duration) {
    switch (instance->common.parser_step) {
    case 0:
        if ((level) && DURATION_DIFF(duration, instance->common.te_shot)< instance->common.te_delta) {
            instance->common.parser_step = 1;
            instance->common.header_count++;
        } else {
            instance->common.parser_step = 0;
        }

        break;
    case 1:
        if ((!level) && (DURATION_DIFF(duration, instance->common.te_shot ) < instance->common.te_delta)) {
            instance->common.parser_step = 0;
            break;
        }
        if ((instance->common.header_count > 2) && ( DURATION_DIFF(duration, instance->common.te_shot * 10)< instance->common.te_delta * 10)) {
            // Found header
            instance->common.parser_step = 2;
            instance->common.code_found = 0;
            instance->common.code_count_bit = 0;
        } else {
            instance->common.parser_step = 0;
            instance->common.header_count = 0;
        }
        break;
    case 2:
        if (level) {
            instance->common.te_last = duration;
            instance->common.parser_step = 3;
        }
        break;
    case 3:
        if (!level) {
            if (duration >= (instance->common.te_shot * 2 + instance->common.te_delta)) {
                // Found end TX
                instance->common.parser_step = 0;
                if (instance->common.code_count_bit >= instance->common.code_min_count_bit_for_found) {
                    if(instance->common.code_last_found != instance->common.code_found ){
                        subghz_protocol_keeloq_check_remote_controller(instance);  
                    }
                    instance->common.code_last_found = instance->common.code_found;
                    instance->common.code_found = 0;
                    instance->common.code_count_bit = 0;
                    instance->common.header_count = 0;
                }
                break;
            } else if ((DURATION_DIFF(instance->common.te_last, instance->common.te_shot) < instance->common.te_delta)
                    && (DURATION_DIFF(duration, instance->common.te_long) < instance->common.te_delta)) {
                if (instance->common.code_count_bit < instance->common.code_min_count_bit_for_found) {
                    subghz_protocol_common_add_bit(&instance->common, 1);
                }
                instance->common.parser_step = 2;
            } else if ((DURATION_DIFF(instance->common.te_last, instance->common.te_long) < instance->common.te_delta)
                    && (DURATION_DIFF(duration, instance->common.te_shot) < instance->common.te_delta)) {
                if (instance->common.code_count_bit < instance->common.code_min_count_bit_for_found) {
                    subghz_protocol_common_add_bit(&instance->common, 0);
                }
                instance->common.parser_step = 2;
            } else {
                instance->common.parser_step = 0;
                instance->common.header_count = 0;
            }
        } else {
            instance->common.parser_step = 0;
            instance->common.header_count = 0;
        }
        break;
    }
}

void subghz_protocol_keeloq_to_str(SubGhzProtocolKeeloq* instance, string_t output) {
    uint32_t code_found_hi = instance->common.code_found >> 32;
    uint32_t code_found_lo = instance->common.code_found & 0x00000000ffffffff;

    uint64_t code_found_reverse = subghz_protocol_common_reverse_key(instance->common.code_found, instance->common.code_count_bit);

    uint32_t code_found_reverse_hi = code_found_reverse>>32;
    uint32_t code_found_reverse_lo = code_found_reverse&0x00000000ffffffff;
    string_cat_printf(
        output,
        "Protocol %s, %d Bit\r\n"
        "KEY:0x%lX%lX\r\n"
        "FIX:%08lX MF:%s \r\n"
        "HOP:%08lX \r\n"
        "SN:%07lX CNT:%04X B:%02lX\r\n",
        instance->common.name,
        instance->common.code_count_bit,
        code_found_hi,
        code_found_lo,
        code_found_reverse_hi,
        instance->manufacture_name,
        code_found_reverse_lo,
        instance->common.serial,
        instance->common.cnt, 
        instance->common.btn
    );
}
