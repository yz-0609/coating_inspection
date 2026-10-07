#include "output_policy.h"
#include "protocol.h"
uint16_t OutputPolicy_Resolve(uint16_t request, bool gate)
{
    /* 故障门控优先；只接受单独 PASS 或 FAIL，混合位及未知请求全部关闭结果输出。 */
    if (gate || request==OUTPUT_FAULT) return OUTPUT_FAULT;
    if (request==OUTPUT_PASS || request==OUTPUT_FAIL) return request;
    return 0;
}
