#ifndef _MA_MODEL_YOLOV8NMS_H
#define _MA_MODEL_YOLOV8NMS_H

#include <vector>

#include "core/model/ma_model_detector.h"

namespace ma::model {

class YoloV8NMS : public Detector {
private:
    ma_tensor_t output_;
    int32_t num_class_;

protected:
    ma_err_t postprocess() override;

public:
    YoloV8NMS(Engine* engine);
    ~YoloV8NMS();
    static bool isValid(Engine* engine);
};

}  // namespace ma::model

#endif  // _MA_MODEL_YOLO_H
