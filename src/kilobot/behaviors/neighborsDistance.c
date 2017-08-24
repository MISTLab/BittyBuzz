#include <bbzkilobot.h>
#include <bbzkilobot_include.h>

void bbz_led() {
    bbzvm_assert_lnum(1);
#ifndef DEBUG
    uint8_t color = (uint8_t)bbzheap_obj_at(bbzvm_locals_at(1))->i.value;
    set_color(RGB(color&1?3:0, color&2?3:0, color&4?3:0));
#endif
    bbzvm_ret0();
}

void bbz_delay() {
    bbzvm_assert_lnum(1);
#ifndef DEBUG
    uint16_t d = (uint16_t)bbzheap_obj_at(bbzvm_locals_at(1))->i.value;
    delay(d);
#endif
    bbzvm_ret0();
}

void bbz_setmotor() {
    bbzvm_assert_lnum(2);
#ifndef DEBUG
    spinup_motors();
    set_motors((uint8_t)bbzheap_obj_at(bbzvm_locals_at(1))->i.value, (uint8_t)bbzheap_obj_at(bbzvm_locals_at(2))->i.value);
#endif
    bbzvm_ret0();
}

void bbz_rand() {
    bbzvm_pushi(((uint16_t)rand_soft() << 8) | rand_soft());
    bbzvm_ret1();
}

void setup() {
    bbzvm_function_register(BBZSTRING_ID(led), bbz_led);
    bbzvm_function_register(BBZSTRING_ID(delay), bbz_delay);
    bbzvm_function_register(BBZSTRING_ID(set_motor), bbz_setmotor);
    rand_seed(rand_hard());
}

int main() {
    bbzkilo_init();
    bbzkilo_start(setup);

    return 0;
}