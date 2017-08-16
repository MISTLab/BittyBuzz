#define NUM_TEST_CASES 12
#define TEST_MODULE swarm
#include "testingconfig.h"

#include <bittybuzz/bbzswarm.h>

#ifndef BBZ_DISABLE_SWARMLIST_BROADCASTS
#define SUBSWARM_TBL_SIZE 7 // 6 closures + 'id'
#else // !BBZ_DISABLE_SWARMLIST_BROADCASTS
#define SUBSWARM_TBL_SIZE 6 // 5 closures + 'id'
#endif // !BBZ_DISABLE_SWARMLIST_BROADCASTS

#define TEST_END ((uint16_t)~0)
#define RBT 0 /**< @brief Current robot */

/**
 * @brief Resets the VM's state and error.
 */
void bbzvm_reset_state() {
    vm->state = BBZVM_STATE_READY;
    vm->error = BBZVM_ERROR_NONE;
}

/**
 * @brief Checks if the wrong number of arguments of a closure fail.
 * @details This checks for one argument less than min as
 * well as one argument more than max.
 */
void test_wrong_num_params(bbzheap_idx_t closure, uint16_t min_num, uint16_t max_num) {
    uint16_t num_params[2] = {min_num - 1, max_num + 1};
    uint8_t should_try[2]  = {min_num > 0, max_num < UINT16_MAX};
    for (uint8_t i = 0; i < 2; ++i) {
        if (should_try[i]) {
            bbzvm_push(closure);
            for (uint16_t j = 0; j < num_params[i]; ++j) {
                bbzvm_pushi(0);
            }
            bbzvm_closure_call(num_params[i]);
            ASSERT_EQUAL(vm->error, BBZVM_ERROR_LNUM);
            bbzvm_reset_state();
            for (uint16_t j = 0; j < num_params[i]; ++j) {
                bbzvm_pop();
            }
        }
    }
}

/**
 * Tests whether <0 or >7 swarm IDs fail.
 * @details This checks for -1 as 8 swarm IDs.
 */
void test_wrong_swarm_ids(bbzheap_idx_t closure) {
    int16_t swarm_ids[2] = {-1, 8*sizeof(bbzswarmlist_t)};
    for (uint8_t i = 0; i < 2; ++i) {
        bbzvm_push(closure);
        bbzvm_pushi(swarm_ids[i]);
        bbzvm_closure_call(1);
        ASSERT_EQUAL(vm->error, BBZVM_ERROR_SWARM);
        bbzvm_reset_state();
        bbzvm_pop();
    }
}

/**
 * @brief Gets and returns a subfield of 'swarm'.
 * @param[in] strid String id of the subfield.
 */
bbzheap_idx_t get_swarm_subfield(uint16_t strid) {
    bbzvm_push(vm->swarm.hpos);
    bbzheap_idx_t ret = bbztable_get_subfield(strid);
    bbzvm_pop();
    return ret;
}

/**
 * Creates a subswarm structure using 'swarm.create'.
 * @param[in] swarm The ID of the subswarm to create.
 * @return The created subswarm
 */
bbzheap_idx_t create_subswarm_structure(bbzswarm_id_t swarm) {
    bbzvm_push(get_swarm_subfield(__BBZSTRID_create));
    bbzvm_pushi(swarm);
    bbzvm_closure_call(1); // 'swarm.create(<id>)'
    bbzheap_idx_t subswarm = bbzvm_stack_at(0); // 'subswarm = swarm.create(<id>)'
    bbzheap_obj_make_permanent(*bbzheap_obj_at(subswarm)); // Do not garbage-collect the table ;
                                                           // we are using it for our unit tests!
    bbzvm_pop();
    return subswarm;
}

/**
 * Gets the swarmlist of a robot.
 * @note This function relies on 'bbzswarm_isrobotin'.
 */
bbzswarmlist_t get_swarmlist(bbzrobot_id_t robot) {
    bbzswarmlist_t swarmlist = 0;
    for (uint8_t i = 0; i < 8*sizeof(bbzswarmlist_t); ++i) {
        swarmlist |= bbzswarm_isrobotin(robot, i) << i;
    }
    return swarmlist;
}

/**
 * Initializes a unit test.
 */
void init_test(bbzvm_t* vm_ptr) {
    vm = vm_ptr;
    bbzvm_construct(RBT);
}

/****************************************/
/****************************************/

// ========================================
// =              UNIT TESTS              =
// ========================================

TEST(addrmmember) {
    bbzvm_t vmObj;
    init_test(&vmObj);

    // Add robots and memberships
    {
#ifndef BBZ_DISABLE_SWARMLIST_BROADCASTS
        bbzrobot_id_t new_robots[]    = { RBT,  RBT,  RBT,    1,    1,    1, TEST_END};
        bbzswarm_id_t new_swarms[]    = {   5,    0,    0,    0,    1,    1};
        bbzswarmlist_t expected_swl[] = {0x20, 0x21, 0x21, 0x01, 0x03, 0x03};
#else // !BBZ_DISABLE_SWARMLIST_BROADCASTS
        bbzrobot_id_t new_robots[]    = { RBT,  RBT,  RBT, TEST_END};
        bbzswarm_id_t new_swarms[]    = {   5,    0,    0};
        bbzswarmlist_t expected_swl[] = {0x20, 0x21, 0x21};
#endif // !BBZ_DISABLE_SWARMLIST_BROADCASTS

        uint16_t i = 0;
        while(new_robots[i] != TEST_END) {
            bbzvm_gc(); // Call garbage-collector
            bbzswarm_addmember(new_robots[i], new_swarms[i]);
            REQUIRE(vm->state != BBZVM_STATE_ERROR);
            uint16_t entry;
            ASSERT_EQUAL(get_swarmlist(new_robots[i]), expected_swl[i]);

            ++i;
        }
    }

    // Remove memberships
    {
#ifndef BBZ_DISABLE_SWARMLIST_BROADCASTS
        bbzrobot_id_t new_robots[]    = { RBT,  RBT,  RBT,    1,    1, TEST_END};
        bbzswarm_id_t new_swarms[]    = {   5,    0,    0,    1,    0};
        bbzswarmlist_t expected_swl[] = {0x01, 0x00, 0x00, 0x01, 0x00};
#else // !BBZ_DISABLE_SWARMLIST_BROADCASTS
        bbzrobot_id_t new_robots[]    = { RBT,  RBT,  RBT, TEST_END};
        bbzswarm_id_t new_swarms[]    = {   5,    0,    0};
        bbzswarmlist_t expected_swl[] = {0x01, 0x00, 0x00};
#endif // !BBZ_DISABLE_SWARMLIST_BROADCASTS

        uint16_t i = 0;
        while(new_robots[i] != TEST_END) {
            bbzvm_gc(); // Call garbage-collector
            bbzswarm_rmmember(new_robots[i], new_swarms[i]);
            REQUIRE(vm->state != BBZVM_STATE_ERROR);
            ASSERT_EQUAL(get_swarmlist(new_robots[i]), expected_swl[i]);

            ++i;
        }
    }
}

/****************************************/
/****************************************/

TEST(refresh) {
    bbzvm_t vmObj;
    init_test(&vmObj);

    {
#ifndef BBZ_DISABLE_SWARMLIST_BROADCASTS
        bbzrobot_id_t new_robots[] = { RBT,  RBT,    1, TEST_END};
        bbzswarmlist_t new_swl[]   = {0xAA, 0x99, 0xF0};
#else // !BBZ_DISABLE_SWARMLIST_BROADCASTS
        bbzrobot_id_t new_robots[] = { RBT,  RBT, TEST_END};
        bbzswarmlist_t new_swl[]   = {0xAA, 0x99};
#endif // !BBZ_DISABLE_SWARMLIST_BROADCASTS

        uint16_t i = 0;
        while (new_robots[i] != TEST_END) {
            bbzvm_gc(); // Call garbage-collector
            bbzswarm_refresh(new_robots[i], new_swl[i]);
            REQUIRE(vm->state != BBZVM_STATE_ERROR);
            ASSERT_EQUAL(get_swarmlist(new_robots[i]), new_swl[i]);

            ++i;
        }
    }

}

/****************************************/
/****************************************/

TEST(isrobotin) {
    bbzvm_t vmObj;
    init_test(&vmObj);

    // Add some subswarm memberships.
    bbzswarm_addmember(0, 0);
#ifndef BBZ_DISABLE_SWARMLIST_BROADCASTS
    bbzswarm_addmember(1, 1);
    bbzswarm_addmember(2, 1);
    bbzswarm_addmember(2, 2);
#endif // !BBZ_DISABLE_SWARMLIST_BROADCASTS

    {
#ifndef BBZ_DISABLE_SWARMLIST_BROADCASTS
        bbzrobot_id_t robots[]  = {RBT, RBT, 1, 1, 1, 2, 2, 2, 2, TEST_END};
        bbzswarm_id_t swarms[]  = {  0,   1, 0, 1, 2, 0, 1, 2, 3};
        uint8_t expected_ret[]  = {  1,   0, 0, 1, 0, 0, 1, 1, 0};
#else // !BBZ_DISABLE_SWARMLIST_BROADCASTS
        bbzrobot_id_t robots[]  = {RBT, RBT, TEST_END};
        bbzswarm_id_t swarms[]  = {  0,   1};
        uint8_t expected_ret[]  = {  1,   0};
#endif // !BBZ_DISABLE_SWARMLIST_BROADCASTS

        uint16_t i = 0;
        while(robots[i] != (TEST_END)) {
            bbzvm_gc(); // Call garbage-collector
            uint8_t ret = bbzswarm_isrobotin(robots[i], swarms[i]) != 0;
            REQUIRE(vm->state != BBZVM_STATE_ERROR);
            ASSERT_EQUAL(ret, expected_ret[i]);

            ++i;
        }
    }
}

/****************************************/
/****************************************/

#ifndef BBZ_DISABLE_SWARMLIST_BROADCASTS

// To those who implement swarmlist broadcasts, beware: the tests for
// them were written and do compile but were never actually tried. You
// will likely find some logic errors in them.
make_devs_look_at_the_above_comment();

TEST(rmentry) {
    bbzvm_t vmObj;
    vm = &vmObj;
    init_test(&vmObj);

    // Add some subswarm memberships.
    bbzswarm_addmember(0, 1);
    bbzswarm_addmember(1, 1);
    bbzswarm_addmember(1, 2);

    {
        bbzswarm_rmentry(1);
        REQUIRE(vm->state != BBZVM_STATE_ERROR);
        ASSERT_EQUAL(bbztable_size(vm->swarm.hpos), INITIAL_SIZE-1);
    }

}
#endif // !BBZ_DISABLE_SWARMLIST_BROADCASTS

/****************************************/
/****************************************/

TEST(create) {
    bbzvm_t vmObj;
    init_test(&vmObj);

    // Create a swarm.
    const bbzheap_idx_t CREATE = get_swarm_subfield(__BBZSTRID_create);
    bbzvm_push(CREATE);
    bbzvm_pushi(0); // Swarm ID
    bbzvm_closure_call(1); // swarm.create(0)
    bbzheap_obj_make_permanent(*bbzheap_obj_at(bbzvm_stack_at(0))); // Do not garbage-collect the subswarm table ;
                                                                    // We use it for our unit tests!
    bbzvm_gc(); // Call garbage-collector
    REQUIRE(vm->state != BBZVM_STATE_ERROR);

    // Check if memberships haven't changed
    bbzheap_idx_t subswarm = bbzvm_stack_at(0);
    bbzvm_pop();
    for (uint8_t i = 0; i < 8 * sizeof(bbzswarmlist_t); ++i) {
        if (bbzswarm_isrobotin(0, i)) {
            ASSERT(0);
        }
    }
    ASSERT_EQUAL(bbztable_size(subswarm), SUBSWARM_TBL_SIZE);

    // Check if <0 and >7 swarm IDs fail.
    test_wrong_swarm_ids(CREATE);

    // Check if wrong number of parameters fails
    test_wrong_num_params(CREATE, 1, 1);
}

/****************************************/
/****************************************/

#ifndef BBZ_DISABLE_SWARMLIST_BROADCASTS

TEST(intersection_union_difference) {
#if 0 // TODO The functions are not implemented.
    bbzvm_t vmObj;
    init_test(&vmObj);

    // Get some closures.
    const bbzheap_idx_t
        INTERSECTION = get_swarm_subfield(__BBZSTRID_intersection),
        UNION = get_swarm_subfield(__BBZSTRID_union),
        DIFFERENCE = get_swarm_subfield(__BBZSTRID_difference);
    bbzvm_gc(); // Call garbage-collector

    // Create some subswarm structures
    bbzheap_idx_t s0, s1, s2;
    s0 = create_subswarm_structure(0);
    bbzvm_gc(); // Call garbage-collector
    s1 = create_subswarm_structure(1);
    bbzvm_gc(); // Call garbage-collector
    s2 = create_subswarm_structure(2);
    bbzvm_gc(); // Call garbage-collector

    // Add some subswarm memberships.
    bbzswarm_addmember(0, 1);
    bbzswarm_addmember(0, 2);
    bbzvm_gc(); // Call garbage-collector

    // Do the checks.
    bbzheap_idx_t closures[3] = {INTERSECTION, UNION, DIFFERENCE};
    {
        bbzswarm_id_t swarms[3] = {3, 4, 5};
        bbzrobot_id_t robots[7] =        {RBT, 1, 2, 3, 4, 5, TEST_END};
        uint8_t expected_rets[3][7-1] = {{  1, 1, 0, 0, 0, 0},  // intersection
                                         {  1, 1, 1, 1, 0, 0},  // union
                                         {  0, 0, 1, 0, 0, 0}}; // difference
        for (uint8_t i = 0; i < 3; ++i) {
            bbzvm_push(closures[i]);
            bbzvm_pushi(swarms[i]);
            bbzvm_push(s1);
            bbzvm_push(s2);
            bbzvm_closure_call(3); // 'swarm.<closure>(<swarm ID>, s1, s2)'
            REQUIRE(vm->state != BBZVM_STATE_ERROR);
            bbzheap_idx_t sX = bbzvm_stack_at(0); // 'sX = swarm.<closure>(<swarm ID>, s1, s2)'
            bbzvm_pop();
            bbzvm_gc(); // Call garbage-collector ; this also removes the subswarm structure

            REQUIRE(vm->state != BBZVM_STATE_ERROR);

            uint16_t j = 0;
            while (robots[j] != TEST_END) {
                uint8_t ret = bbzswarm_isrobotin(robots[j], swarms[i]) != 0;
                ASSERT_EQUAL(ret, expected_rets[i][j]);

                ++j;
            }
        }
    }

    // Check if wrong swarm IDs fail
    {
        for (uint8_t i = 0; i < 3; ++i) {
            int16_t swarms[2] = {-1, 8*sizeof(bbzswarmlist_t)};
            for (uint8_t j = 0; j < 2; ++j) {
                bbzvm_push(closures[i]);
                bbzvm_pushi(swarms[j]);
                bbzvm_push(s1);
                bbzvm_push(s2);
                bbzvm_closure_call(3);
                ASSERT_EQUAL(vm->error, BBZVM_ERROR_SWARM);
                bbzvm_reset_state();
                bbzvm_pop();
            }
        }
    }
#endif // 0
}

#endif // !BBZ_DISABLE_SWARMLIST_BROADCASTS

/****************************************/
/****************************************/

TEST(id) {
    bbzvm_t vmObj;
    init_test(&vmObj);

    // Get closure
    const bbzheap_idx_t ID = get_swarm_subfield(__BBZSTRID_id);
    bbzvm_gc(); // Call garbage-collector

    // Push to the swarmstack
    const uint8_t NUM_PUSHES = 5;
    for (int8_t i = NUM_PUSHES - 1; i >= 0; --i) {
        bbzvm_pushi(i);
        bbzheap_idx_t swarm = bbzvm_stack_at(0);
        bbzvm_pop();
        bbzdarray_push(vm->swarm.swarmstack, swarm);
    }

    REQUIRE(vm->state != BBZVM_STATE_ERROR);

    // Check without argument
    {
        bbzvm_push(ID);
        bbzvm_closure_call(0); // 'swarm.id()'
        REQUIRE(vm->state != BBZVM_STATE_ERROR);
        int16_t ret = bbzheap_obj_at(bbzvm_stack_at(0))->i.value;
        bbzvm_pop();
        ASSERT_EQUAL(ret, 0);
    }

    // Check with argument
    {
        for (uint8_t i = 0; i <= NUM_PUSHES - 1; ++i) {
            bbzvm_push(ID);
            bbzvm_pushi(i);
            bbzvm_closure_call(1); // 'swarm.id(i)'
            REQUIRE(vm->state != BBZVM_STATE_ERROR);
            int16_t ret = bbzheap_obj_at(bbzvm_stack_at(0))->i.value;
            bbzvm_pop();
            ASSERT_EQUAL(ret, i);
        }
    }

    // Check erroneous arguments
    {
        int16_t args[2] = {-1, NUM_PUSHES};
        for (uint8_t i = 0; i < 2; ++i) {
            bbzvm_push(ID);
            bbzvm_pushi(args[i]);
            bbzvm_closure_call(1); // 'swarm.id(i)'
            ASSERT_EQUAL(vm->error, BBZVM_ERROR_OUTOFRANGE);
            bbzvm_reset_state();
        }
    }

    // Check if wrong number of arguments fails.
    {
        test_wrong_num_params(ID, 0, 1);
    }

}

/****************************************/
/****************************************/

#ifndef BBZ_DISABLE_SWARMLIST_BROADCASTS

TEST(others) {
#if 0 // TODO The function is not implemented.
    bbzvm_t vmObj;
    init_test(&vmObj);

    // Create subswarm structures
    bbzheap_idx_t s0 = create_subswarm_structure(0);
    bbzheap_idx_t s1 = create_subswarm_structure(1);

    // Get closures
    bbzvm_push(s0);
    const bbzheap_idx_t
        OTHERS0 = bbztable_get_subfield(__BBZSTRID_others); // 's0.others'
    bbzvm_pop();
    bbzvm_push(s1);
    const bbzheap_idx_t
        OTHERS1 = bbztable_get_subfield(__BBZSTRID_others); // 's1.others'
    bbzvm_pop();

    // Add subswarm memberships.
    bbzswarm_addmember(0, 0);
    bbzswarm_addmember(1, 1);
    bbzswarm_rmmember (1, 1);

    bbzvm_push(OTHERS0);
    bbzvm_pushi(2);
    bbzvm_closure_call(1); // 's0.others(2)'
    bbzheap_idx_t s2 = bbzvm_stack_at(0); // 's2 = s0.others(2)'
    bbzvm_pop();
    REQUIRE(vm->state != BBZVM_STATE_ERROR);
    bbzvm_push(OTHERS1);
    bbzvm_pushi(3);
    bbzvm_closure_call(1); // 's1.others(3)'
    bbzheap_idx_t s3 = bbzvm_stack_at(0); // 's3 = s1.others(3)'
    bbzvm_pop();
    REQUIRE(vm->state != BBZVM_STATE_ERROR);

    // Check memberships
    {
        bbzswarm_id_t swarms[] = {2, 3, TEST_END};
        bbzrobot_id_t robots[4]    =  {RBT, 1, 2, TEST_END};
        uint8_t expected_ret[2][3] = {{  0, 1, 0},  // swarms[0]
                                      {  1, 1, 0}}; // swarms[1]
        uint16_t i = 0;
        while (swarms[i] != (bbzswarm_id_t)TEST_END) {
            uint16_t j = 0;
            while (robots[j] != TEST_END) {
                uint8_t ret = bbzswarm_isrobotin(robots[j], swarms[i]) != 0;
                ASSERT_EQUAL(ret, expected_ret[i][j]);
                ++j;
            }
            ++i;
        }
    }

    // Check if wrong swarm ID fails.
    {
        test_wrong_swarm_ids(OTHERS0);
    }

    // Check if wrong number of params fails.
    {
        test_wrong_num_params(OTHERS0, 1, 1);
    }
#endif // 0
}

#endif // !BBZ_DISABLE_SWARMLIST_BROADCASTS

/****************************************/
/****************************************/

TEST(join_leave) {
    bbzvm_t vmObj;
    init_test(&vmObj);

    // Create subswarm structures
    bbzheap_idx_t
        s0 = create_subswarm_structure(0), // 's0 = swarm.create(0)'
        s1 = create_subswarm_structure(1); // 's1 = swarm.create(1)'

    // Get closures
    bbzvm_push(s0);
    const bbzheap_idx_t
        JOIN0 = bbztable_get_subfield(__BBZSTRID_join), // 's0.join'
        LEAVE0 = bbztable_get_subfield(__BBZSTRID_leave); // 's0.leave'
    bbzvm_pop();
    bbzvm_push(s1);
    const bbzheap_idx_t
        JOIN1 = bbztable_get_subfield(__BBZSTRID_join), // 's1.join'
        LEAVE1 = bbztable_get_subfield(__BBZSTRID_leave); // 's1.leave'
    bbzvm_pop();

    // -------------
    // - TEST JOIN -
    // -------------

    // Add some subswarm memberships.
    bbzvm_push(JOIN0);
    bbzvm_closure_call(0); // 's0.join()'
    REQUIRE(vm->state != BBZVM_STATE_ERROR);
    bbzvm_push(JOIN1);
    bbzvm_closure_call(0); // 's1.join()'
    REQUIRE(vm->state != BBZVM_STATE_ERROR);
    bbzvm_push(JOIN1);
    bbzvm_closure_call(0); // 's1.join()'
    REQUIRE(vm->state != BBZVM_STATE_ERROR);


    // Check if robot joined the swarms
    {
        ASSERT(bbzswarm_isrobotin(0, 0));
        ASSERT(bbzswarm_isrobotin(0, 1));
    }

    // Check if wrong number of parameters fails.
    {
        test_wrong_num_params(JOIN0, 0, 0);
    }

    // --------------
    // - TEST LEAVE -
    // --------------

    // Remove some subswarm memberships.
    bbzvm_push(LEAVE0);
    bbzvm_closure_call(0); // 's0.leave()'
    REQUIRE(vm->state != BBZVM_STATE_ERROR);

    // Check if robot leaved the swarm
    {
        ASSERT(!bbzswarm_isrobotin(0, 0));
        ASSERT(bbzswarm_isrobotin(0, 1));
    }

    // Check if wrong number of parameters fails.
    {
        test_wrong_num_params(LEAVE0, 0, 0);
    }
}

/****************************************/
/****************************************/

TEST(in) {
    bbzvm_t vmObj;
    init_test(&vmObj);

    // Create some subswarm structures.
    bbzheap_idx_t s0 = create_subswarm_structure(0); // 's0 = swarm.create(0)'
    bbzheap_idx_t s1 = create_subswarm_structure(1); // 's1 = swarm.create(1)'

    // Get closures
    bbzvm_push(s0);
    const bbzheap_idx_t
        IN0 = bbztable_get_subfield(__BBZSTRID_in); // 's0.in'
    bbzvm_pop();
    bbzvm_push(s1);
    const bbzheap_idx_t
        IN1 = bbztable_get_subfield(__BBZSTRID_in); // 's1.in'
    bbzvm_pop();

    REQUIRE(vm->state != BBZVM_STATE_ERROR);

    // Add some subswarm memberships.
    bbzswarm_addmember(0, 0);
#ifndef BBZ_DISABLE_SWARMLIST_BROADCASTS
    bbzswarm_addmember(1, 0);
#endif // !BBZ_DISABLE_SWARMLIST_BROADCASTS

    REQUIRE(vm->state != BBZVM_STATE_ERROR);

    // Check normal usage
    {
        bbzheap_idx_t closures[] = {IN0, IN1, TEST_END};
        uint8_t expected_ret[]   = {  1,   0};

        uint16_t i = 0;
        while(closures[i] != TEST_END) {
            bbzvm_push(closures[i]);
            bbzvm_closure_call(0); // s<i>.in()
            REQUIRE(vm->state != BBZVM_STATE_ERROR);
            uint8_t ret = bbzheap_obj_at(bbzvm_stack_at(0))->i.value != 0;
            bbzvm_pop();
            ASSERT_EQUAL(ret, expected_ret[i]);

            ++i;
        }
    }

    // Check if wrong number of parameters fails.
    {
        test_wrong_num_params(IN0, 0, 0);
    }
}

/****************************************/
/****************************************/

TEST(select) {
    bbzvm_t vmObj;
    init_test(&vmObj);

    // Create subswarm structures
    bbzheap_idx_t s0 = create_subswarm_structure(0);

    REQUIRE(vm->state != BBZVM_STATE_ERROR);

    // Get closures
    bbzvm_push(s0);
    const bbzheap_idx_t SELECT0 = bbztable_get_subfield(__BBZSTRID_select);
    bbzvm_pop();

    REQUIRE(vm->state != BBZVM_STATE_ERROR);

    // Create parameter objects
    bbzvm_pushi(42);
    bbzvm_pushi(0);
    bbzheap_idx_t
        TRUE_OBJ  = bbzvm_stack_at(1),
        FALSE_OBJ = bbzvm_stack_at(0);

    // Check a 'true' value.
    {
        bbzheap_idx_t params[] = {TRUE_OBJ, FALSE_OBJ, vm->nil, TEST_END};
        uint8_t expected_ret[] = {       1,         0,        0};
        uint16_t i = 0;
        while(params[i] != TEST_END) {
            bbzvm_push(SELECT0);
            bbzvm_push(params[i]);
            bbzvm_closure_call(1); // 's0.select(<param>)'
            REQUIRE(vm->state != BBZVM_STATE_ERROR);
            uint8_t ret = bbzswarm_isrobotin(0, 0) != 0;
            ASSERT_EQUAL(ret, expected_ret[i]);
            bbzswarm_rmmember(0, 0);

            ++i;
        }
    }

    // Check if wrong number of parameters fails.
    {
        test_wrong_num_params(SELECT0, 1, 1);
    }
}

/****************************************/
/****************************************/

const uint16_t NUM_CALLS = 2;
bbzheap_idx_t exec_function_closure;
bbzheap_idx_t exec0; // 's0.exec'
bbzheap_idx_t exec1; // 's1.exec'
uint16_t exec_curr_index;

void exec_function() {
    bbzvm_assert_lnum(0);
    static uint16_t curr_recursion_depth = 0;

    ++curr_recursion_depth;

    ASSERT(vm->state != BBZVM_STATE_ERROR);
    ASSERT_EQUAL(bbzdarray_size(vm->swarm.swarmstack), curr_recursion_depth);

    uint16_t swarmstack_value;
    switch(exec_curr_index) {
    case 0: {
        bbzdarray_get(vm->swarm.swarmstack, bbzdarray_size(vm->swarm.swarmstack) - 1, &swarmstack_value);
        ASSERT_EQUAL(bbzheap_obj_at(swarmstack_value)->i.value, 0);
        break;
    }
    case 1: {
        if (curr_recursion_depth == 1) {
            bbzdarray_get(vm->swarm.swarmstack, bbzdarray_size(vm->swarm.swarmstack) - 1, &swarmstack_value);
            ASSERT_EQUAL(bbzheap_obj_at(swarmstack_value)->i.value, 0);
            bbzvm_push(exec1);
            bbzvm_push(exec_function_closure);
            bbzvm_closure_call(1); // 's1.exec(exec_function_closure)'
            bbzdarray_get(vm->swarm.swarmstack, bbzdarray_size(vm->swarm.swarmstack) - 1, &swarmstack_value);
            ASSERT_EQUAL(bbzheap_obj_at(swarmstack_value)->i.value, 0);
            break;
        }
        else {
            bbzdarray_get(vm->swarm.swarmstack, bbzdarray_size(vm->swarm.swarmstack) - 1, &swarmstack_value);
            ASSERT_EQUAL(bbzheap_obj_at(swarmstack_value)->i.value, 1);
            bbzdarray_get(vm->swarm.swarmstack, bbzdarray_size(vm->swarm.swarmstack) - 2, &swarmstack_value);
            ASSERT_EQUAL(bbzheap_obj_at(swarmstack_value)->i.value, 0);
            break;
        }
    }
    default: ASSERT(0);
    }

    --curr_recursion_depth;

    bbzvm_ret0();
}

TEST(exec) {
    bbzvm_t vmObj;
    init_test(&vmObj);

    // Create subswarm structure
    bbzheap_idx_t s0 = create_subswarm_structure(0);
    bbzheap_idx_t s1 = create_subswarm_structure(1);

    // Get closure
    bbzvm_push(s0);
    exec0 = bbztable_get_subfield(__BBZSTRID_exec); // 's0.exec'
    bbzvm_pop();
    bbzvm_push(s1);
    exec1 = bbztable_get_subfield(__BBZSTRID_exec); // 's1.exec'
    bbzvm_pop();

    // Create 'exec_function_closure' closure.
    bbzvm_pushcc(exec_function);
    exec_function_closure = bbzvm_stack_at(0);
    bbzvm_pop();

    REQUIRE(vm->state != BBZVM_STATE_ERROR);

    // Check normal usage
    {
        for (exec_curr_index = 0; exec_curr_index < NUM_CALLS; ++exec_curr_index) {
            bbzvm_push(exec0);
            bbzvm_push(exec_function_closure);
            bbzvm_gc(); // Call garbage-collector
            bbzvm_closure_call(1); // 's0.exec(exec_function_closure)'
        }
    }

    // Check if wrong number of parameters fails.
    {
        test_wrong_num_params(exec0, 1, 1);
    }
}

/****************************************/
/****************************************/

TEST_LIST {
    ADD_TEST(isrobotin);
    ADD_TEST(addrmmember);
    ADD_TEST(refresh);
    ADD_TEST(create);
#ifndef BBZ_DISABLE_SWARMLIST_BROADCASTS
    ADD_TEST(rmentry);
    ADD_TEST(intersection_union_difference);
    ADD_TEST(others);
#endif // !BBZ_DISABLE_SWARMLIST_BROADCASTS
    ADD_TEST(id);
    ADD_TEST(join_leave);
    ADD_TEST(in);
    ADD_TEST(select);
    ADD_TEST(exec);
}