/**
 * 文件功能：未配置云适配器的明确返回行为
 *
 * 职责说明：实现 DisabledCloudPublisher::publish，返回本地运行模式的适配器状态。
 * 调用关系：云接口调用 publish → 返回 NotConfigured；本地 Outbox 保留待传记录。
 * 阅读提示：此处没有网络请求，也不会调用成功回调或改变数据库上传状态。
 */
#include "inspection/cloud.hpp"
namespace inspection {
// 模块：未配置的发布入口。省略参数名表示此实现不使用事件、请求号或回调。
PublishSubmission DisabledCloudPublisher::publish(const Json&, const std::string&,
                                                  std::function<void(CloudReply)>) {
    // 本地运行模式返回 NotConfigured，保留 Outbox 状态与发送计数。
    return PublishSubmission::NotConfigured;
}
} // namespace inspection
