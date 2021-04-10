#include <avr/io.h>         // io port and addresses
#include <avr/wdt.h>        // watch dog timer
#include <avr/interrupt.h>  // interrupt handling
#include <avr/eeprom.h>     // read eeprom values
#include <avr/pgmspace.h>   // read flash data
#include <avr/sleep.h>      // enter powersaving sleep mode
#include <util/delay.h>     // delay macros
#include <bittybuzz/bbzneighbors.h>

#include "bbzkilobot.h"
#include "bbzmessage_send.h"
#include "bbzmacros.h"
#include "bbzohc.h"

#define EEPROM_OSCCAL         (uint8_t*)0x01
#define EEPROM_TXMASK         (uint8_t*)0x90
#define EEPROM_IRLOW          (uint8_t*)0x20
#define EEPROM_IRHIGH         (uint8_t*)0x50
#define EEPROM_UID            (uint8_t*)0xB0
#define EEPROM_LEFT_ROTATE    (uint8_t*)0x05
#define EEPROM_RIGHT_ROTATE   (uint8_t*)0x09
#define EEPROM_LEFT_STRAIGHT  (uint8_t*)0x0C
#define EEPROM_RIGHT_STRAIGHT (uint8_t*)0x14
#define TX_MASK_MAX   ((1<<0)|(1<<1)|(1<<2)|(1<<6)|(1<<7))
#define TX_MASK_MIN   ((1<<0))

/* Number of clock cycles per bit. */
#define rx_bitcycles 269
/* Number of clock cycles for an entire message. */
#define rx_msgcycles (11*rx_bitcycles)

void message_rx_txsuccess_dummy() { }
message_t *message_tx_dummy() { return NULL; }

message_rx_t kilo_message_rx = (message_rx_t)message_rx_txsuccess_dummy;
message_tx_t kilo_message_tx = message_tx_dummy;
message_tx_success_t kilo_message_tx_success = (message_tx_success_t)message_rx_txsuccess_dummy;

message_t rx_msg;                  // message being received
distance_measurement_t rx_dist;    // signal strength of message being received
static uint8_t *rawmsg = (uint8_t*)&rx_msg;
volatile uint8_t rx_busy;          // flag that signals if message is being received
uint8_t rx_leadingbit;             // flag that signals start bit
uint8_t rx_leadingbyte;            // flag that signals start byte
uint8_t rx_byteindex;              // index to the current byte being decoded
uint8_t rx_bytevalue;              // value of the current byte being decoded
volatile uint8_t tx_mask;
volatile uint16_t kilo_tx_period;

#ifndef BOOTLOADER
uint16_t tx_clock;                 // number of timer cycles we have waited
uint16_t tx_increment;             // number of timer cycles until next interrupt
volatile uint32_t kilo_ticks;      // internal clock (updated in tx ISR)
uint16_t kilo_uid;                 // unique identifier (stored in EEPROM)
uint8_t kilo_turn_left;
uint8_t kilo_turn_right;
uint8_t kilo_straight_left;
uint8_t kilo_straight_right;
uint16_t kilo_irhigh;
uint16_t kilo_irlow;
bbzvm_t kilo_vmObj;
message_t bbzmsg_tx;
uint8_t bbzmsg_buf[11];
bbzmsg_payload_t bbz_payload_buf;
#endif // !BOOTLOADER

static volatile enum {
    SLEEPING,
    IDLE,
    BATTERY,
    SETUP,
    RUNNING,
    CHARGING,
    MOVING
} kilo_state;

/***********************************/

message_t* bbzwhich_msg_tx() ;
void bbzprocess_msg_rx(message_t* msg_rx, distance_measurement_t* d) ;

void bbzkilo_init() {
    cli();

    ports_off();
    ports_on();
    tx_timer_setup();
    rx_timer_setup();
    motors_setup();
    acomp_setup();
    adc_setup();
    adc_trigger_high_gain();

    uint8_t osccal = eeprom_read_byte(EEPROM_OSCCAL);
    if (osccal != 0xFF)
        OSCCAL = osccal;

    rx_busy = 0;
    rx_leadingbit = 1;
    rx_leadingbyte = 1;
    rx_byteindex = 0;
    rx_bytevalue = 0;
#ifndef BOOTLOADER
    tx_mask = eeprom_read_byte(EEPROM_TXMASK);
    if (tx_mask & ~TX_MASK_MAX)
        tx_mask = TX_MASK_MIN;
    tx_clock = 0;
    tx_increment = 255;
    kilo_ticks = 0;
    kilo_state = IDLE;
    kilo_tx_period = (uint16_t)(rand_hard()>>4) << 3;//512;//3906;//2048;//
    kilo_uid = eeprom_read_byte(EEPROM_UID) | eeprom_read_byte(EEPROM_UID+1)<<8;
    kilo_turn_left = eeprom_read_byte(EEPROM_LEFT_ROTATE);
    kilo_turn_right = eeprom_read_byte(EEPROM_RIGHT_ROTATE);
    kilo_straight_left = eeprom_read_byte(EEPROM_LEFT_STRAIGHT);
    kilo_straight_right = eeprom_read_byte(EEPROM_RIGHT_STRAIGHT);

    kilo_irlow  = ((eeprom_read_byte(EEPROM_IRLOW) <<8) | eeprom_read_byte(EEPROM_IRLOW + 1)) >> 2;
    kilo_irhigh = ((eeprom_read_byte(EEPROM_IRHIGH) <<8) | eeprom_read_byte(EEPROM_IRHIGH + 1)) >> 2;
    vm = &kilo_vmObj;
    bbzringbuf_construct(&bbz_payload_buf, bbzmsg_buf, 1, 11);
#ifdef DEBUG
    kilo_state = SETUP;
    kilo_uid = 0;
#endif // DEBUG
#endif // !BOOTLOADER
    sei();
}

#ifndef BOOTLOADER

enum {
    MOVE_STOP,
    MOVE_LEFT,
    MOVE_RIGHT,
    MOVE_STRAIGHT
};

static volatile uint8_t prev_motion = MOVE_STOP, cur_motion = MOVE_STOP;

uint8_t buf[4];
const uint8_t* bbzkilo_bcodeFetcher(bbzpc_t offset, uint8_t size) {
    uint16_t __addr16 = (uint16_t) ((uint16_t) ((uint16_t) &bcode + sizeof(*bcode) * offset));
    __asm__ __volatile__("subi %0, 1  \n\t"
                         "brvs .+14   \n\t"
                         "lpm %A2, Z+ \n\t"
                         "sbrc %0, 0  \n\t"
                         "lpm %B2, Z+ \n\t"
                         "sbrs %0, 1  \n\t"
                         "rjmp .+4    \n\t"
                         "lpm %C2, Z+ \n\t"
                         "lpm %D2, Z  \n\t"
                        :"=r"(size), "=z"(__addr16), "=r"(buf):"0"(size), "1"(__addr16));
    return buf;
}

message_t* bbzwhich_msg_tx() {
#ifndef BBZ_DISABLE_MESSAGES
    if(bbzoutmsg_queue_size()) {
        bbzoutmsg_queue_first(&bbz_payload_buf);
        for (uint8_t i=0;i<9;++i) {
            bbzmsg_tx.data[i] = *bbzringbuf_at(&bbz_payload_buf, i);
        }
        bbzmsg_tx.type = BBZMSG;
        bbzmsg_tx.crc = bbzmessage_crc(&bbzmsg_tx);
        return &bbzmsg_tx;
    }
#endif
    return 0;
}

void bbzprocess_msg_rx(message_t* msg_rx, distance_measurement_t* d) {
#ifndef BBZ_DISABLE_MESSAGES
    if (msg_rx->type == BBZMSG) {
        bbzringbuf_clear(&bbz_payload_buf);
        for (uint8_t i = 0; i < 9; ++i) {
            *bbzringbuf_rawat(&bbz_payload_buf, bbzringbuf_makeslot(&bbz_payload_buf)) = msg_rx->data[i];
        }
        // Add the neighbor data.
#ifndef BBZ_DISABLE_NEIGHBORS
        if (*bbzmsg_buf == BBZMSG_BROADCAST) {
            uint8_t dist = ((uint8_t)(d->high_gain>>2) + (uint8_t)(d->low_gain>>2))>>1;
            bbzneighbors_elem_t elem = {.azimuth=0,.elevation=0};
            uint8_t distance = (kilo_irhigh + kilo_irlow) >> 1;
            distance -= (dist>distance?distance:dist);
#ifndef BBZ_NEIGHBORS_USE_FLOATS
            elem.distance -= distance;
#else // !BBZ_NEIGHBORS_USE_FLOATS
            elem.distance -= bbzfloat_fromint(distance);
#endif // !BBZ_NEIGHBORS_USE_FLOATS
            elem.robot = *(uint16_t*)(bbzmsg_buf + 1);
            bbzneighbors_add(&elem);
        }
#endif // !BBZ_DISABLE_NEIGHBORS
        bbzinmsg_queue_append(&bbz_payload_buf);
    }
#endif // !BBZ_DISABLE_MESSAGES
}

#ifdef DEBUG
__attribute__((used))
void inject_vstig(int16_t val, bbzrobot_id_t rid, uint8_t lamport) {
    bbzringbuf_clear(&bbz_payload_buf);
    bbzobj_t o = {0};
    bbztype_cast(o, BBZTYPE_INT);
    o.i.value = val;
    bbzmsg_serialize_u8(&bbz_payload_buf, BBZMSG_VSTIG_PUT);
    bbzmsg_serialize_u16(&bbz_payload_buf, rid);
    bbzmsg_serialize_u16(&bbz_payload_buf, 47); // STRID "1"
    bbzmsg_serialize_obj(&bbz_payload_buf, &o);
    bbzmsg_serialize_u8(&bbz_payload_buf, lamport);
    bbzinmsg_queue_append(&bbz_payload_buf);
}
#ifndef BBZ_DISABLE_NEIGHBORS
__attribute__((used))
void inject_bc(int16_t val, bbzrobot_id_t rid, uint8_t dist) {
    bbzringbuf_clear(&bbz_payload_buf);
    bbzobj_t o = {0};
    bbztype_cast(o, BBZTYPE_INT);
    o.i.value = val;
    bbzmsg_serialize_u8(&bbz_payload_buf, BBZMSG_BROADCAST);
    bbzmsg_serialize_u16(&bbz_payload_buf, rid);
    bbzmsg_serialize_u16(&bbz_payload_buf, 49); // STRID "1"
    bbzmsg_serialize_obj(&bbz_payload_buf, &o);
    bbzinmsg_queue_append(&bbz_payload_buf);
    bbzneighbors_elem_t elem = {.azimuth=0,.elevation=0};
    elem.robot = rid;
    elem.distance = dist;
    bbzneighbors_add(&elem);
}
#endif // !BBZ_DISABLE_NEIGHBORS
#endif // DEBUG

void bbzkilo_func_call(uint16_t strid) {
    bbzvm_pushs(strid);
    bbzheap_idx_t l = bbzvm_stack_at(0);
    bbzvm_pop();
    if(bbztable_get(kilo_vmObj.gsyms, l, &l)) {
        bbzvm_pushnil(); // Push self table
        bbzvm_push(l);
        bbzvm_closure_call(0);
    }
}


void bbzkilo_start(void (*setup)(void)) {
//    int16_t voltage;
    uint8_t has_setup = 0;
    while (1) {
        switch(kilo_state) {
            case SLEEPING:
#ifndef DEBUG
                do{}while(0); // This is to avoid errors with clang from having inline assembly at the begenning of a case.
                cli();
                acomp_off();
                adc_off();
                ports_off();
                wdt_enable(WDTO_8S);
                WDTCSR |= (1<<WDIE);
                set_sleep_mode(SLEEP_MODE_PWR_DOWN);
                cli();
                sleep_enable();
                sei();
                sleep_cpu();
                sleep_disable();
                sei();
                rx_busy = 0;
                ports_on();
                adc_on();
                _delay_us(300);
                acomp_on();

                set_color(RGB(3,3,3));
                delay(10);
                if (rx_busy) {
                    set_color(RGB(3,0,0));
                    delay(100);
                }
                set_color(RGB(0,0,0));
#endif
                break;
            case IDLE:
                set_color(RGB(0,3,0));
                delay(1);
                set_color(RGB(0,0,0));
                delay(200);
                break;
            case BATTERY:
//                voltage = get_voltage();
//                if(voltage > 682)
//                    set_color(RGB(0,3,0));
//                else if(voltage > 648)
//                    set_color(RGB(0,0,3));
//                else if(voltage > 614)
//                    set_color(RGB(3,3,0));
//                else
//                    set_color(RGB(3,0,0));
//                break;
            case CHARGING:
//                if (is_charging()) {
//                    set_color(RGB(1,0,0));
//                    _delay_ms(1);
//                    set_color(RGB(0,0,0));
//                    _delay_ms(200);
//                } else
//                    set_color(RGB(0,0,0));
                break;
            case SETUP:
                if (!has_setup) {
                    bbzvm_construct(kilo_uid);
                    bbzvm_set_bcode(bbzkilo_bcodeFetcher, pgm_read_word((uint16_t)&bcode_size));
                    bbzvm_set_error_receiver(bbzkilo_err_receiver);
                    setup();
                    has_setup = 1;
                }
                if (vm->state == BBZVM_STATE_READY) {
                    bbzvm_step();
                }
                else {
                    kilo_state = RUNNING;
                    vm->state = BBZVM_STATE_READY;
                    bbzkilo_func_call(__BBZSTRID_init);
#ifndef BBZ_DISABLE_MESSAGES
                    kilo_message_tx = bbzwhich_msg_tx;
                    kilo_message_tx_success = bbzoutmsg_queue_next;
                    kilo_message_rx = bbzprocess_msg_rx;
#endif
                }
                break;
            case RUNNING:
                if (vm->state != BBZVM_STATE_ERROR) {
                    bbzvm_process_inmsgs();
                    bbzkilo_func_call(__BBZSTRID_step);
                    bbzvm_process_outmsgs();
                }
                break;
            case MOVING:
//                if (cur_motion == MOVE_STOP) {
//                    set_motors(0,0);
//                    prev_motion = MOVE_STOP;
//                } else {
//                    if (cur_motion != prev_motion) {
//                        prev_motion = cur_motion;
//                        if (cur_motion == MOVE_LEFT) {
//                            set_motors(0xFF, 0);
//                            _delay_ms(15);
//                            set_motors(kilo_turn_left, 0);
//                        } else if (cur_motion == MOVE_RIGHT) {
//                            set_motors(0, 0xFF);
//                            _delay_ms(15);
//                            set_motors(0, kilo_turn_right);
//                        } else {
//                            set_motors(0, 0xFF);
//                            set_motors(0xFF, 0xFF);
//                            _delay_ms(15);
//                            set_motors(kilo_straight_left, kilo_straight_right);
//                        }
//                    }
//                }
                break;
        }
    }
}
#ifndef DEBUG
//// Ensure that wdt is inactive after system reset.
void wdt_init(void) __attribute__((naked)) __attribute__((section(".init3")));

void wdt_init(void) {
    MCUSR = 0;
    wdt_disable();
}

/**
 * Watchdog timer interrupt.
 * Used to wakeup from low power sleep mode.
 */
ISR(WDT_vect) {
    wdt_disable();
}
#endif
#define JUMPTO_PASTER(addr) do{}while(0); __asm__ __volatile__("jmp "#addr"\n\t")
#define JUMPTO(addr) JUMPTO_PASTER(addr)
static inline void bbzprocess_message() {
//    AddressPointer_t reset = (AddressPointer_t)0x0000, bootload = (AddressPointer_t)BBZ_BOOTLOADER_ADDR;
//    calibmsg_t *calibmsg = (calibmsg_t*)&rx_msg.data;
    if (rx_msg.type < BOOT) {
        kilo_message_rx(&rx_msg, &rx_dist);
        return;
    }
    if (/*rx_msg.type != READUID && */rx_msg.type != RUN/* && rx_msg.type != CALIB*/)
    motors_off();
    switch (rx_msg.type) {
        case BOOT:
        tx_timer_off();
//            ((AddressPointer_t)BBZ_BOOTLOADER_ADDR)();
            JUMPTO(BBZ_BOOTLOADER_ADDR);
            break;
        case RESET:
            JUMPTO(0x0000);
            break;
        case SLEEP:
            kilo_state = SLEEPING;
            break;
        case WAKEUP:
//            kilo_state = IDLE;
            break;
        case CHARGE:
//            kilo_state = CHARGING;
            break;
        case VOLTAGE:
//            kilo_state = BATTERY;
            break;
        case RUN:
            if (kilo_state != SETUP && kilo_state != RUNNING) {
                motors_on();
                kilo_state = SETUP;
            }
            break;
        case CALIB:
//            switch(calibmsg->mode) {
//                case CALIB_SAVE:
//                    if (kilo_state == MOVING) {
//                        eeprom_write_byte(EEPROM_UID, kilo_uid&0xFF);
//                        eeprom_write_byte(EEPROM_UID+1, (kilo_uid>>8)&0xFF);
//                        eeprom_write_byte(EEPROM_LEFT_ROTATE, kilo_turn_left);
//                        eeprom_write_byte(EEPROM_RIGHT_ROTATE, kilo_turn_right);
//                        eeprom_write_byte(EEPROM_LEFT_STRAIGHT, kilo_straight_left);
//                        eeprom_write_byte(EEPROM_RIGHT_STRAIGHT, kilo_straight_right);
//                        motors_off();
//                        kilo_state = IDLE;
//                    }
//                    break;
//                case CALIB_UID:
//                    kilo_uid = calibmsg->uid;
//                    cur_motion = MOVE_STOP;
//                    break;
//                case CALIB_TURN_LEFT:
//                    if (cur_motion != MOVE_LEFT || kilo_turn_left != calibmsg->turn_left) {
//                        prev_motion = MOVE_STOP;
//                        cur_motion = MOVE_LEFT;
//                        kilo_turn_left = calibmsg->turn_left;
//                    }
//                    break;
//                case CALIB_TURN_RIGHT:
//                    if (cur_motion != MOVE_RIGHT || kilo_turn_right != calibmsg->turn_right) {
//                        prev_motion = MOVE_STOP;
//                        cur_motion = MOVE_RIGHT;
//                        kilo_turn_right = calibmsg->turn_right;
//                    }
//                    break;
//                case CALIB_STRAIGHT:
//                    if (cur_motion != MOVE_STRAIGHT || kilo_straight_right != calibmsg->straight_right || kilo_straight_left != calibmsg->straight_left) {
//                        prev_motion = MOVE_STOP;
//                        cur_motion = MOVE_STRAIGHT;
//                        kilo_straight_left = calibmsg->straight_left;
//                        kilo_straight_right = calibmsg->straight_right;
//                    }
//                    break;
//            }
//            if (calibmsg->mode != CALIB_SAVE && kilo_state != MOVING) {
//                motors_on();
//                kilo_state = MOVING;
//            }
            break;
        case READUID:
//            if (kilo_state != MOVING) {
//                motors_on();
//                set_color(RGB(0,0,0));
//                prev_motion = cur_motion = MOVE_STOP;
//                kilo_state = MOVING;
//            }
//
//            if (kilo_uid&(1<<rx_msg.data[0]))
//                cur_motion = MOVE_LEFT;
//            else
//                cur_motion = MOVE_STOP;
            break;
        default:
            break;
    }
}

void delay(uint16_t ms) {
#ifndef DEBUG
    while (ms) {
        --ms;
        _delay_ms(1);
    };
#endif
}

void set_motors(uint8_t ccw, uint8_t cw) {
#ifndef DEBUG
    OCR2A = ccw;
    OCR2B = cw;
#endif
}

void spinup_motors() {
#ifndef DEBUG
    set_motors(255, 255);
    delay(15);
#endif
}

int16_t get_ambientlight() {
    int16_t light = -1;
    if (!rx_busy) {
        cli();
        adc_setup_conversion(7);
        adc_start_conversion();
        adc_finish_conversion();
        light = ADCW;                             // store AD result
        adc_trigger_high_gain();                     // set AD to measure high gain (for distance sensing)
        sei();                                    // reenable interrupts
    }
    return light;
}

int16_t get_temperature() {
    int16_t temp = -1;
    if (!rx_busy) {
        cli();
        ADMUX = (1<<3)|(1<<6)|(1<<7);
        ADCSRA = (1<<ADEN)|(1<<ADPS2)|(1<<ADPS1)|(1<<ADPS0);
        adc_start_conversion();
        adc_finish_conversion();
        temp = ADCW;                             // store AD result
        adc_trigger_high_gain();                     // set AD to measure high gain (for distance sensing)
        sei();                                    // reenable interrupts
    }
    return temp;
}

uint8_t rand_hard() {
    uint8_t num = 0;
    uint8_t a, b, i, tries;
    for (i = 0; i < 8; ++i) {
        tries = 0;
        do {
            cli();
            adc_setup_conversion(6);
            adc_start_conversion();
            adc_finish_conversion();
            a = ADCW&0x1;
            adc_start_conversion();
            adc_finish_conversion();
            b = ADCW&0x1;
            adc_trigger_high_gain();
            sei();
            tries++;
        } while (a == b && tries < 128);
        if (a)
            num |= (1<<i);
    }
    return num;
}

static uint8_t seed=0xaa, accumulator = 0;

uint8_t rand_soft() {
    seed ^= seed<<3;
    seed ^= seed>>5;
    seed ^= accumulator++>>2;
    return seed;
}

void rand_seed(uint8_t s) {
    seed = s;
}

int16_t get_voltage() {
    int16_t voltage=-1;
    if (!rx_busy) {
        cli();                                    // disable interrupts
        adc_setup_conversion(6);
        adc_start_conversion();
        adc_finish_conversion();
        voltage = ADCW;                           // store AD result
//        adc_trigger_high_gain();                     // set AD to measure high gain (for distance sensing)
        sei();                                    // reenable interrupts
    }
    return voltage;
}

/**
 * Timer0 interrupt.
 * Used to send messages every kilo_tx_period ticks.
 */
ISR(TIMER0_COMPA_vect) {
    tx_clock += tx_increment;
    tx_increment = 0xFF;
    OCR0A = tx_increment;
    kilo_ticks++;

    if(!rx_busy && tx_clock>kilo_tx_period && kilo_state == RUNNING) {
        message_t *msg = kilo_message_tx();
        if (msg) {
            if (bbzmessage_send(msg)) {
                kilo_message_tx_success();
                tx_clock = 0;
            } else {
                tx_increment = rand_hard()&0xFF;
                OCR0A = tx_increment;
            }
        }
    }
}

#else// BOOTLOADER

static inline void process_message() {
    kilo_message_rx(&rx_msg, &rx_dist);
}

EMPTY_INTERRUPT(TIMER0_COMPA_vect)

#endif

void set_color(uint8_t rgb) {
#ifndef DEBUG
    if (rgb&(1<<0))
        DDRD |= (1<<5);
    else
        DDRD &= ~(1<<5);

    if (rgb&(1<<1))
        DDRD |= (1<<4);
    else
        DDRD &= ~(1<<4);

    if (rgb&(1<<2))
        DDRC |= (1<<3);
    else
        DDRC &= ~(1<<3);

    if (rgb&(1<<3))
        DDRC |= (1<<2);
    else
        DDRC &= ~(1<<2);

    if (rgb&(1<<4))
        DDRC |= (1<<5);
    else
        DDRC &= ~(1<<5);

    if (rgb&(1<<5))
        DDRC |= (1<<4);
    else
        DDRC &= ~(1<<4);
#endif
}

void tx_clock_reset() {
    tx_clock = kilo_tx_period - 0;
}

static void ___led(uint8_t x) {set_color(x); delay(50); set_color(0); delay(100);}
void bbzkilo_err_receiver(bbzvm_error errcode) {
    set_motors(0,0);
    uint8_t i;
    ___led(RGB(1,2,0));
    ___led(RGB(1,2,0));
    ___led(RGB(1,2,0));
    ___led(RGB(1,2,0));
    delay(300);
#if 1
    for (i = 4; i; --i) {
        delay(700);
        switch(errcode) {
            case BBZVM_ERROR_INSTR:      ___led(RGB(2,0,0)); ___led(RGB(2,0,0)); break;
            case BBZVM_ERROR_STACK:      ___led(RGB(1,2,0)); if (bbzvm_stack_size() >= BBZSTACK_SIZE) { ___led(RGB(0,3,0)); } else if (bbzvm_stack_size() <= 0) { ___led(RGB(2,0,0)); } else { ___led(RGB(1,2,0)); } break;
            case BBZVM_ERROR_LNUM:       ___led(RGB(3,1,0)); ___led(RGB(3,1,0)); break;
            case BBZVM_ERROR_PC:         ___led(RGB(0,3,0)); ___led(RGB(0,3,0)); break;
            case BBZVM_ERROR_FLIST:      ___led(RGB(0,3,0)); ___led(RGB(2,0,0)); break;
            case BBZVM_ERROR_TYPE:       ___led(RGB(0,3,0)); ___led(RGB(1,2,0)); break;
            case BBZVM_ERROR_OUTOFRANGE: ___led(RGB(0,0,2)); ___led(RGB(2,0,0)); break;
            case BBZVM_ERROR_NOTIMPL:    ___led(RGB(0,0,2)); ___led(RGB(0,2,0)); break;
            case BBZVM_ERROR_RET:        ___led(RGB(0,3,0)); ___led(RGB(0,0,2)); break;
            case BBZVM_ERROR_STRING:     ___led(RGB(0,2,1)); ___led(RGB(0,2,1)); break;
            case BBZVM_ERROR_SWARM:      ___led(RGB(0,2,1)); ___led(RGB(2,0,0)); break;
            case BBZVM_ERROR_VSTIG:      ___led(RGB(0,2,1)); ___led(RGB(1,2,0)); break;
            case BBZVM_ERROR_MEM:        ___led(RGB(0,2,1)); ___led(RGB(0,0,2)); break;
            case BBZVM_ERROR_MATH:       ___led(RGB(0,0,2)); ___led(RGB(0,0,2)); break;
            default: ___led(RGB(2,0,2)); ___led(RGB(2,0,2)); break;
        }
    }
#endif
    ___led(RGB(2,2,2));
    ___led(RGB(2,2,2));
}

/**
 * Timer1 interrupt.
 * Timeout which is trigerred if stop bit is not received.
 */
ISR(TIMER1_COMPA_vect) {
    rx_timer_off();
    rx_leadingbit = 1;
    rx_leadingbyte = 1;
    rx_busy = 0;
    adc_trigger_high_gain();
}

#ifndef DEBUG
/**
 * Analog comparator trigger interrupt.
 * Triggerred for incoming IR pulses (i.e. individual bits).
 */
ISR(ANALOG_COMP_vect) {
    uint16_t timer = TCNT1;

    rx_busy = 1;
    /* adc_trigger_stop(); */

    if(rx_leadingbit) {       // Start bit received.
        rx_timer_on();
        rx_bytevalue = 0;
        rx_leadingbit = 0;
        if (rx_leadingbyte) {
            adc_finish_conversion();
            rx_dist.high_gain = ADCW;
            adc_trigger_low_gain();
        }
    } else {
        // Stray bit received
        if (timer <= rx_bitcycles/2 || timer >= rx_bitcycles*9+rx_bitcycles/2) {
            rx_timer_off();
            rx_leadingbit = 1;
            rx_leadingbyte = 1;
            rx_busy = 0;
            adc_trigger_high_gain();
        } else {
            // NOTE: The following code avoids a division which takes
            // too many clock cycles and throws off the interrupt.
            const uint16_t M = ((1L<<16)+rx_bitcycles-1)/rx_bitcycles;
            uint8_t bitindex = ((uint32_t)(timer-rx_bitcycles/2)*M)>>16;
            if (bitindex <= 7) { // Data bit received.
                rx_bytevalue |= (1<<bitindex);
            } else {             // Stop bit received.
                rx_leadingbit = 1;
                if (rx_leadingbyte) {
                    adc_finish_conversion();
                    rx_dist.low_gain = ADCW;
                    adc_trigger_high_gain();
                    if (rx_bytevalue != 0) { // Collision detected.
                        rx_timer_off();
                        rx_leadingbyte = 1;
                        rx_busy = 0;
                    } else {                // Leading byte received.
                        rx_leadingbyte = 0;
                        rx_byteindex = 0;
                    }
                } else {
                    rawmsg[rx_byteindex] = rx_bytevalue;
                    rx_byteindex++;
                    if (rx_byteindex == sizeof(message_t)) {
                        rx_timer_off();
                        rx_leadingbyte = 1;
                        rx_busy = 0;

                        if (rx_msg.crc == bbzmessage_crc(&rx_msg))
                            bbzprocess_message();
                    }
                }
            }
        }
    }
}
#endif