#pragma once
#include <array>
#include <stdexcept>
#include "PluginFrameUtils.hpp"

namespace jdk_nodes {
// VO/IVPS need local physical memory. Keep this owner through HDMI submission.
inline std::shared_ptr<AXVideoFrame> prepare_hdmi_frame(
    const std::shared_ptr<AXVideoFrame>& source,
    std::shared_ptr<AXVideoFrame>& staging) {
    if (!source || !jdk_plugin::raw_frame_layout_reasonable(source->raw()))
        throw std::runtime_error("invalid image layout");
    const auto& src = *source->raw();
    if (source->backend() == VFrameBackend::AxLocal && src.u64PhyAddr[0])
        return source; // CPU mapping is optional for AX decoder frames.
    if (source->backend() != VFrameBackend::Axcl || source->device_id() < 0 || !src.u64PhyAddr[0])
        throw std::runtime_error("HDMI requires AX local or AXCL physical frames");
    if (src.stCompressInfo.enCompressMode != AX_COMPRESS_MODE_NONE)
        throw std::runtime_error("compressed AXCL HDMI input is unsupported");
    if (source->size() < static_cast<int>(src.u32FrameSize))
        throw std::runtime_error("image exceeds source allocation");
    std::array<AX_U64, 3> offsets{};
    for (int i = 1; i < 3; ++i) {
        if (!src.u64PhyAddr[i]) continue;
        if (src.u64PhyAddr[i] <= src.u64PhyAddr[0] ||
            src.u64PhyAddr[i] - src.u64PhyAddr[0] >= src.u32FrameSize)
            throw std::runtime_error("non-contiguous AXCL image planes");
        offsets[i] = src.u64PhyAddr[i] - src.u64PhyAddr[0];
    }
    if (src.enImgFormat == AX_FORMAT_YUV420_SEMIPLANAR || src.enImgFormat == AX_FORMAT_YUV420_SEMIPLANAR_VU) {
        const AX_U64 y_bytes = AX_U64(src.u32PicStride[0]) * src.u32Height;
        const AX_U64 uv_bytes = AX_U64(src.u32PicStride[1]) * ((src.u32Height + 1) / 2);
        if (!offsets[1] || offsets[1] < y_bytes || !uv_bytes || offsets[1] + uv_bytes > src.u32FrameSize)
            throw std::runtime_error("invalid NV12/NV21 plane extent");
    }
    if (!staging || staging->width() != src.u32Width || staging->height() != static_cast<int>(src.u32Height) ||
        staging->size() != static_cast<int>(src.u32FrameSize))
        staging = std::make_shared<AXVideoFrame>(src.u32Width, src.u32Height, -1,
            src.u32FrameSize, AXVideoFrame::MemoryPolicy::Reusable);
    if (staging->backend() != VFrameBackend::AxLocal || !staging->raw()->u64PhyAddr[0])
        throw std::runtime_error("AX local HDMI allocation unavailable");
    // CopyFrom uses DEVICE_TO_HOST_PHY for a local AX hardware destination.
    if (staging->CopyFrom(reinterpret_cast<const uint8_t*>(src.u64PhyAddr[0]),
            src.u32FrameSize, 0, source->device_id()) != 0)
        throw std::runtime_error("AXCL to AX physical copy failed");
    auto& dst = *staging->raw();
    const AX_U64 phy = dst.u64PhyAddr[0], vir = dst.u64VirAddr[0];
    const auto block = dst.u32BlkId[0];
    dst = src; // Preserve stride, crop, format, PTS and padded UV offset.
    for (int i = 0; i < 3; ++i) {
        const bool present = i == 0 || offsets[i] != 0;
        dst.u64PhyAddr[i] = present ? phy + offsets[i] : 0;
        dst.u64VirAddr[i] = present && vir ? vir + offsets[i] : 0;
        dst.u32BlkId[i] = present ? block : AX_INVALID_BLOCKID;
    }
    staging->pts = source->pts;
    return staging;
}
} // namespace jdk_nodes
