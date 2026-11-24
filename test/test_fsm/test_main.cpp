#include <unity.h>
#include "people_counter.h"
#include "room_fsm.h"
#include "alarm_fsm.h"

void test_people_counter_enter_and_exit(void) {
    people_counter_t pc;
    pc_init(&pc);

    /* Mô phỏng 1 người đi vào: Trống -> Che A -> Che A&B -> Che B -> Trống */
    pc_update_door(&pc, 2000, 2000, 25);
    pc_update_door(&pc, 800,  2000, 25);
    pc_update_door(&pc, 800,  800,  25);
    pc_update_door(&pc, 2000, 800,  25);
    app_event_type_t ev = pc_update_door(&pc, 2000, 2000, 25);

    TEST_ASSERT_EQUAL(EVT_PERSON_ENTERED, ev);
    TEST_ASSERT_EQUAL_INT16(1, pc.count);

    /* Mô phỏng 1 người đi ra: Che B -> Che A&B -> Che A -> Trống */
    pc_update_door(&pc, 2000, 800,  25);
    pc_update_door(&pc, 800,  800,  25);
    pc_update_door(&pc, 800,  2000, 25);
    ev = pc_update_door(&pc, 2000, 2000, 25);

    TEST_ASSERT_EQUAL(EVT_PERSON_EXITED, ev);
    TEST_ASSERT_EQUAL_INT16(0, pc.count);
}

void test_people_counter_turn_around_and_timeout(void) {
    people_counter_t pc;
    pc_init(&pc);

    /* Đi tới vùng A rồi quay đầu lui ra -> không đếm */
    pc_update_door(&pc, 800,  2000, 25);
    pc_update_door(&pc, 2000, 2000, 25);
    TEST_ASSERT_EQUAL_INT16(0, pc.count);

    /* Đứng kẹt giữa cửa quá 3000 ms -> reset về IDLE */
    pc_update_door(&pc, 800, 800, 3100);
    TEST_ASSERT_EQUAL(CNT_IDLE, pc.state);
}

void test_room_fsm_hysteresis_and_grace(void) {
    room_fsm_t rfsm;
    rfsm_init(&rfsm);

    /* Phòng có 1 người, tối (250 lux), nóng (28.5 C) -> Bật đèn & quạt */
    rfsm_step(&rfsm, 1, true, 250.0f, 28.5f, 1000);
    TEST_ASSERT_EQUAL(ROOM_ST_OCCUPIED, rfsm.state);
    TEST_ASSERT_TRUE(rfsm.light_on);
    TEST_ASSERT_TRUE(rfsm.fan_on);

    /* Hết người -> Vào GRACE, thiết bị vẫn bật */
    rfsm_step(&rfsm, 0, false, 250.0f, 28.5f, 1000);
    TEST_ASSERT_EQUAL(ROOM_ST_GRACE, rfsm.state);
    TEST_ASSERT_TRUE(rfsm.light_on);

    /* Hết 5 phút GRACE -> Về EMPTY, tắt hết */
    rfsm_step(&rfsm, 0, false, 250.0f, 28.5f, 300000);
    TEST_ASSERT_EQUAL(ROOM_ST_EMPTY, rfsm.state);
    TEST_ASSERT_FALSE(rfsm.light_on);
    TEST_ASSERT_FALSE(rfsm.fan_on);
}

void test_alarm_fsm_fire_override_and_ack(void) {
    alarm_fsm_t afsm;
    room_state_t st = {0};
    afsm_init(&afsm);

    st.temperature_c = 55.0f; /* Quá nhiệt > 50 C */
    afsm_step(&afsm, &st, 1000);
    TEST_ASSERT_EQUAL(ALARM_ST_ALARM, afsm.state);

    bool light = false, fan = true;
    afsm_apply_safety_override(&afsm, &light, &fan);
    TEST_ASSERT_TRUE(light);  /* Đèn bật thoát hiểm */
    TEST_ASSERT_FALSE(fan);   /* Quạt tắt chống cháy */

    /* Nhiệt độ giảm về 25 C nhưng chưa nhấn ACK -> Vẫn giữ ALARM */
    st.temperature_c = 25.0f;
    afsm_step(&afsm, &st, 1000);
    TEST_ASSERT_EQUAL(ALARM_ST_ALARM, afsm.state);

    /* Nhấn ACK -> Về NORMAL */
    afsm_ack_alarm(&afsm);
    afsm_step(&afsm, &st, 1000);
    TEST_ASSERT_EQUAL(ALARM_ST_NORMAL, afsm.state);
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_people_counter_enter_and_exit);
    RUN_TEST(test_people_counter_turn_around_and_timeout);
    RUN_TEST(test_room_fsm_hysteresis_and_grace);
    RUN_TEST(test_alarm_fsm_fire_override_and_ack);
    return UNITY_END();
}