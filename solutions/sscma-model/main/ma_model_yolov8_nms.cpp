#include <algorithm>
#include <forward_list>
#include <math.h>
#include <vector>

#include "core/math/ma_math.h"
#include "core/utils/ma_nms.h"

#include "hailo/hailort.h"
#include "ma_model_yolov8_nms.h"

#include "hailo/tappas/common/structures.hpp"

namespace ma::model {

constexpr char TAG[] = "ma::model::yolov8";

YoloV8NMS::YoloV8NMS(Engine* p_engine_) : Detector(p_engine_, "YoloV8NMS", MA_MODEL_TYPE_YOLOV8) {
    MA_ASSERT(p_engine_ != nullptr);

    output_ = p_engine_->getOutput(0);

    num_class_ = output_.shape.dims[1];
}

YoloV8NMS::~YoloV8NMS() {}

bool YoloV8NMS::isValid(Engine* engine) {
    return true;
}


ma_err_t YoloV8NMS::postprocess() {
    ma_err_t err = MA_OK;
    results_.clear();
    size_t buffer_offset = 0;
    uint8_t* buffer      = output_.data.u8;

    MA_LOGI(TAG, "num_class: %d", num_class_);

    for (decltype(num_class_) i = 0; i < num_class_; i++) {
        float32_t bbox_count = 0;
        memcpy(&bbox_count, buffer + buffer_offset, sizeof(bbox_count));
        buffer_offset += sizeof(bbox_count);
        if (bbox_count == 0)  // No detections
            continue;

        for (size_t bbox_index = 0; bbox_index < static_cast<uint32_t>(bbox_count); bbox_index++) {
            common::hailo_bbox_float32_t* bbox = reinterpret_cast<common::hailo_bbox_float32_t*>(&buffer[buffer_offset]);
            buffer_offset += sizeof(common::hailo_bbox_float32_t);
            ma_bbox_t box;
            box.score  = bbox->score;
            box.target = i;
            box.w      = bbox->x_max - bbox->x_min;
            box.h      = bbox->y_max - bbox->y_min;
            box.x      = bbox->x_min + box.w / 2;
            box.y      = bbox->y_min + box.h / 2;
            results_.emplace_front(std::move(box));
        }
    }

    results_.sort([](const ma_bbox_t& a, const ma_bbox_t& b) { return a.x < b.x; });

    return err;
}
}  // namespace ma::model
