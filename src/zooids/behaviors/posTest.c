#include <functions.h>
#include <bbzsymbols.h>
#include <bittybuzz/bbzutil.h>
#include <bittybuzz/util/bbzstring.h>
#include <bbzzooids.h>

extern Target currentGoal;
extern Motor motorValues;
extern uint8_t currentGoal_reached;

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

void bbz_goto() {
    bbzvm_assert_lnum(2);
    int16_t x = bbzheap_obj_at(bbzvm_locals_at(1))->i.value;
    int16_t y = bbzheap_obj_at(bbzvm_locals_at(2))->i.value;
    currentGoal.x = x;
    currentGoal.y = y;
    currentGoal_reached = false;
    positionControl(currentGoal.x, currentGoal.y, currentGoal.angle, &motorValues, &currentGoal_reached, true, currentGoal.finalGoal, currentGoal.ignoreOrientation);
    bbzvm_pushi(currentGoal_reached);
    bbzvm_ret1();
}

void bbz_greyCode() {
    bbzvm_assert_lnum(1);
    uint16_t b = (uint16_t)bbzheap_obj_at(bbzvm_locals_at(1))->i.value;
    bbzvm_pushi(b ^ (b >> 1));
    bbzvm_ret1();
}

#define registerFunc(name) bbzvm_function_register(BBZSTRING_ID(name), bbz_##name)

void setup() {
    registerFunc(led);//bbzvm_function_register(BBZSTRING_ID(led), bbz_led);
    registerFunc(delay);//bbzvm_function_register(BBZSTRING_ID(delay), bbz_delay);
    registerFunc(goto);//bbzvm_function_register(BBZSTRING_ID(goto), bbz_goto);
    registerFunc(greyCode);//bbzvm_function_register(BBZSTRING_ID(greyCode), bbz_greyCode);
}

int main() {
    bbz_init();
    bbz_start(setup);

    return 0;
}