#include "ethercat_diag/esc/al_status_decoder.h"

#include <cassert>
#include <iostream>

using namespace std;

struct StateCase
{
    std::uint16_t raw;
    EscAlState expected;
};

struct StateNameCase
{
    EscAlState state;
    std::string_view expected;
};

void testDecodesPreOpState()
{
    const AlStatusInfo status =
        decodeAlStatus(0x0002);

    assert(status.raw == 0x0002);
    assert(status.state == EscAlState::PreOp);
    assert(!status.error_indication);
    assert(!status.explicit_device_id_loaded);
}

void testDecodesAllAlStates()
{
    const StateCase cases[]{
        {0x0001, EscAlState::Init},
        {0x0002, EscAlState::PreOp},
        {0x0003, EscAlState::Boot},
        {0x0004, EscAlState::SafeOp},
        {0x0008, EscAlState::Op},
        {0x0005, EscAlState::Unknown}
    };

    for (const StateCase& test_case : cases)
    {
        const AlStatusInfo status =
            decodeAlStatus(test_case.raw);

        assert(status.state == test_case.expected);
    }
}

void testDecodesAlStatusFlags()
{
    const AlStatusInfo status =
        decodeAlStatus(0x8074);

    assert(status.raw == 0x8074);
    assert(status.state == EscAlState::SafeOp);
    assert(status.error_indication);
    assert(status.explicit_device_id_loaded);
    assert(status.warning_indication);

    const AlStatusInfo status1 =
        decodeAlStatus(0x0012);

    assert(status1.raw == 0x0012);
    assert(status1.state == EscAlState::PreOp);
    assert(status1.error_indication);
    assert(!status1.explicit_device_id_loaded);
    assert(!status1.warning_indication);

    const AlStatusInfo statu2 =
        decodeAlStatus(0x0022);

    assert(statu2.raw == 0x0022);
    assert(statu2.state == EscAlState::PreOp);
    assert(!statu2.error_indication);
    assert(statu2.explicit_device_id_loaded);
    assert(!statu2.warning_indication);

    const AlStatusInfo status3 =
        decodeAlStatus(0x0042);

    assert(status3.raw == 0x0042);
    assert(status3.state == EscAlState::PreOp);
    assert(!status3.error_indication);
    assert(!status3.explicit_device_id_loaded);
    assert(status3.warning_indication);
}

void testFormatsAlStateNames()
{
    const StateNameCase cases[]{
        {EscAlState::Init, "INIT"},
        {EscAlState::PreOp, "PREOP"},
        {EscAlState::Boot, "BOOT"},
        {EscAlState::SafeOp, "SAFEOP"},
        {EscAlState::Op, "OP"},
        {EscAlState::Unknown, "UNKNOWN"}
    };

    for (const StateNameCase& test_case : cases)
    {
        assert(
            alStateName(test_case.state)
            == test_case.expected
        );
    }
}

int main()
{
    testDecodesPreOpState();
    testDecodesAllAlStates();
    testDecodesAlStatusFlags();
    testFormatsAlStateNames();

    cout << "All tests passed" << endl;
    return 0;
}
