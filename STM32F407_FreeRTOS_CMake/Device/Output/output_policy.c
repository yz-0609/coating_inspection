#include "output_policy.h"
#include "protocol.h"
uint16_t OutputPolicy_Resolve(uint16_t request, bool gate)
{
    if (gate || request==OUTPUT_FAULT) return OUTPUT_FAULT;
    if (request==OUTPUT_PASS || request==OUTPUT_FAIL) return request;
    return 0;
}
