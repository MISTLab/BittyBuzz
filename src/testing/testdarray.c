#include <bittybuzz/bbzdarray.h>

#define NUM_TEST_CASES 10
#define TEST_MODULE darray
#include "testingconfig.h"

TEST(da_new) {
    bbzvm_t vmObj;
    vm = &vmObj;
    bbzheap_clear();

    uint16_t darray;
    ASSERT(bbzdarray_new(&darray));
    ASSERT(bbztype_isdarray(*bbzheap_obj_at(darray)));
    ASSERT_EQUAL(bbzdarray_size(darray), 0);
    ASSERT(bbzheap_obj_at(darray)->t.value != BBZHEAP_SEG_NO_NEXT);
}

TEST(da_push) {
    bbzvm_t vmObj;
    vm = &vmObj;
    bbzheap_clear();

    uint16_t darray;
    REQUIRE(bbzdarray_new(&darray));

    uint16_t o;
    REQUIRE(bbzheap_obj_alloc(BBZTYPE_INT, &o));
    bbzint_t* io = (bbzint_t*)bbzheap_obj_at(o);
    io->value = 10;
    ASSERT(bbzdarray_push(darray, o));
    ASSERT_EQUAL(bbzdarray_size(darray), 1);
    ASSERT_EQUAL(bbzheap_aseg_elem_get(bbzheap_aseg_at(bbzheap_obj_at(darray)->t.value)->values[0]), o);
}

TEST(da_find) {
    bbzvm_t vmObj;
    vm = &vmObj;
    bbzheap_clear();

    uint16_t darray;
    REQUIRE(bbzdarray_new(&darray));

    uint16_t o;
    for (uint16_t i = 0; i < BBZHEAP_ELEMS_PER_ASEG + 1; ++i) {
        REQUIRE(bbzheap_obj_alloc(BBZTYPE_INT, &o));
        bbzint_t* io = (bbzint_t*)bbzheap_obj_at(o);
        io->value = i;
        REQUIRE(bbzdarray_push(darray, o));
    }
    REQUIRE(bbzdarray_size(darray) == BBZHEAP_ELEMS_PER_ASEG + 1);

    ASSERT_EQUAL(bbzdarray_find(darray, bbztype_cmp, o), BBZHEAP_ELEMS_PER_ASEG);
}

TEST(da_set) {
    bbzvm_t vmObj;
    vm = &vmObj;
    bbzheap_clear();

    uint16_t darray;
    REQUIRE(bbzdarray_new(&darray));

    uint16_t o;
    REQUIRE(bbzheap_obj_alloc(BBZTYPE_INT, &o));
    bbzint_t* io = (bbzint_t*)bbzheap_obj_at(o);
    io->value = 10;
    REQUIRE(bbzdarray_push(darray, o));
    REQUIRE(bbzdarray_size(darray) == 1);

    uint16_t o2;
    REQUIRE(bbzheap_obj_alloc(BBZTYPE_INT, &o2));
    bbzint_t* io2 = (bbzint_t*)bbzheap_obj_at(o2);
    io2->value = 255;
    ASSERT(bbzdarray_set(darray, 0, o2));
    REQUIRE(bbzdarray_size(darray) == 1);
    ASSERT_EQUAL(bbzheap_aseg_elem_get(bbzheap_aseg_at(bbzheap_obj_at(darray)->t.value)->values[0]), o2);
    ASSERT_EQUAL(bbzheap_obj_at(bbzheap_aseg_elem_get(bbzheap_aseg_at(bbzheap_obj_at(darray)->t.value)->values[0]))->i.value, bbzheap_obj_at(o2)->i.value);

    io2->value = 15;
    ASSERT_EQUAL(bbzheap_obj_at(bbzheap_aseg_elem_get(bbzheap_aseg_at(bbzheap_obj_at(darray)->t.value)->values[0]))->i.value, 15);
}

TEST(da_push15x) {
    bbzvm_t vmObj;
    vm = &vmObj;
    bbzheap_clear();

    uint16_t darray;
    REQUIRE(bbzdarray_new(&darray));

    uint16_t o3;
    bbzint_t* io3;
    for (uint16_t i = 0; i < 15; ++i) {
        REQUIRE(bbzheap_obj_alloc(BBZTYPE_INT, &o3));
        io3 = (bbzint_t*)bbzheap_obj_at(o3);
        io3->value = i;
        REQUIRE(bbzdarray_push(darray, o3));
    }
    ASSERT_EQUAL(bbzdarray_size(darray), 15);
}

TEST(da_pop7x) {
    bbzvm_t vmObj;
    vm = &vmObj;
    bbzheap_clear();

    uint16_t darray;
    REQUIRE(bbzdarray_new(&darray));

    uint16_t o3;
    bbzint_t* io3;
    for (uint16_t i = 0; i < 15; ++i) {
        REQUIRE(bbzheap_obj_alloc(BBZTYPE_INT, &o3));
        io3 = (bbzint_t*)bbzheap_obj_at(o3);
        io3->value = i;
        REQUIRE(bbzdarray_push(darray, o3));
    }
    REQUIRE(bbzdarray_size(darray) == 15);

    for (int i = 0; i < 7; ++i) {
        ASSERT(bbzdarray_pop(darray));
    }
    ASSERT_EQUAL(bbzdarray_size(darray), 8);

    uint16_t stack[] = {darray};
    bbzheap_gc(stack, 1);
}

TEST(da_clear) {
    bbzvm_t vmObj;
    vm = &vmObj;
    bbzheap_clear();

    uint16_t darray;
    REQUIRE(bbzdarray_new(&darray));

    uint16_t o3;
    bbzint_t* io3;
    for (uint16_t i = 0; i < 15; ++i) {
        REQUIRE(bbzheap_obj_alloc(BBZTYPE_INT, &o3));
        io3 = (bbzint_t*)bbzheap_obj_at(o3);
        io3->value = i;
        REQUIRE(bbzdarray_push(darray, o3));
    }
    REQUIRE(bbzdarray_size(darray) == 15);

    bbzdarray_clear(darray);
    ASSERT_EQUAL(bbzdarray_size(darray), 0);

    uint16_t stack[] = {darray};
    bbzheap_gc(stack, 1);
}

TEST(da_clone) {
    bbzvm_t vmObj;
    vm = &vmObj;
    bbzheap_clear();

    uint16_t darray;
    REQUIRE(bbzdarray_new(&darray));

    uint16_t o3;
    bbzint_t* io3;
    for (uint16_t i = 0; i < 22; ++i) {
        REQUIRE(bbzheap_obj_alloc(BBZTYPE_INT, &o3));
        io3 = (bbzint_t*)bbzheap_obj_at(o3);
        io3->value = i;
        REQUIRE(bbzdarray_push(darray, o3));
    }
    REQUIRE(bbzdarray_size(darray) == 22);

    uint16_t darray2 = darray;
    ASSERT(bbzdarray_clone(darray, &darray2));
    ASSERT_EQUAL(bbzdarray_size(darray2), 22);
    bbzheap_idx_t o1, o2;
    for (uint16_t i = 0; i < bbzdarray_size(darray); ++i) {
        REQUIRE(bbzdarray_get(darray, i, &o1));
        REQUIRE(bbzdarray_get(darray2, i, &o2));
        ASSERT_EQUAL(o1, o2);
    }
}

void foreach(bbzheap_idx_t darray, bbzheap_idx_t pos, void* params) {
    RM_UNUSED_WARN(darray);
    RM_UNUSED_WARN(params);
    ((bbzint_t*)bbzheap_obj_at(pos))->value += 20;
}

TEST(da_foreach) {
    bbzvm_t vmObj;
    vm = &vmObj;
    bbzheap_clear();

    uint16_t darray;
    REQUIRE(bbzdarray_new(&darray));

    uint16_t o3;
    bbzint_t* io3;
    for (uint16_t i = 0; i < 22; ++i) {
        REQUIRE(bbzheap_obj_alloc(BBZTYPE_INT, &o3));
        io3 = (bbzint_t*)bbzheap_obj_at(o3);
        io3->value = i;
        REQUIRE(bbzdarray_push(darray, o3));
    }
    REQUIRE(bbzdarray_size(darray) == 22);

    bbzdarray_foreach(darray, foreach, NULL);
    bbzheap_idx_t o1;
    for (uint16_t i = 0; i < bbzdarray_size(darray); ++i) {
        REQUIRE(bbzdarray_get(darray, i, &o1));
        ASSERT_EQUAL(bbzheap_obj_at(o1)->i.value, i+20);
    }
}

TEST(da_destroy) {
    bbzvm_t vmObj;
    vm = &vmObj;
    bbzheap_clear();

    uint16_t darray;
    REQUIRE(bbzdarray_new(&darray));

    uint16_t o3;
    bbzint_t* io3;
    for (uint16_t i = 0; i < 22; ++i) {
        REQUIRE(bbzheap_obj_alloc(BBZTYPE_INT, &o3));
        io3 = (bbzint_t*)bbzheap_obj_at(o3);
        io3->value = i;
        REQUIRE(bbzdarray_push(darray, o3));
    }
    REQUIRE(bbzdarray_size(darray) == 22);

    bbzdarray_destroy(darray);
    ASSERT(!bbzheap_obj_isvalid(*bbzheap_obj_at(darray)));
}

TEST_LIST {
    ADD_TEST(da_new);
    ADD_TEST(da_push);
    ADD_TEST(da_find);
    ADD_TEST(da_set);
    ADD_TEST(da_push15x);
    ADD_TEST(da_pop7x);
    ADD_TEST(da_clear);
    ADD_TEST(da_clone);
    ADD_TEST(da_foreach);
    ADD_TEST(da_destroy);
};