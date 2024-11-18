#include <unistd.h>

#include "model.h"

#include "ma_model_yolov8_nms.h"

namespace ma::node {

using namespace ma::engine;
using namespace ma::model;

static constexpr char TAG[] = "ma::node::model";


#define DEFAULT_MODEL "/home/pi/hailo-rpi5-examples/resources/yolov8s_h8l.hef"

ModelNode::ModelNode(std::string id)
    : Node("model", id), uri_(""), debug_(true), trace_(false), counting_(false), count_(0), engine_(nullptr), model_(nullptr), thread_(nullptr), camera_(nullptr), frame_(30) {}

ModelNode::~ModelNode() {
    onDestroy();
}


void ModelNode::threadEntry() {

    ma_err_t err           = MA_OK;
    Detector* detector     = nullptr;
    Classifier* classifier = nullptr;
    int32_t width          = 0;
    int32_t height         = 0;
    int32_t take           = 0;
    cv::Mat* frame         = nullptr;

    switch (model_->getType()) {
        case MA_MODEL_TYPE_IMCLS:
            classifier = static_cast<Classifier*>(model_);
            break;
        default:
            detector = static_cast<Detector*>(model_);
            break;
    }

    while (started_) {
        if (!frame_.fetch(reinterpret_cast<void**>(&frame), Tick::fromSeconds(2))) {
            continue;
        }

        cv::Mat image = frame->clone();

        Thread::enterCritical();
        json reply = json::object({{"type", MA_MSG_TYPE_EVT}, {"name", "invoke"}, {"code", MA_OK}, {"data", {{"count", ++count_}}}});

        width  = 640;
        height = 640;

        // resize & letterbox
        int ih              = image.rows;
        int iw              = image.cols;
        int oh              = height;
        int ow              = width;
        double resize_scale = std::min((double)oh / ih, (double)ow / iw);
        int nh              = (int)(ih * resize_scale);
        int nw              = (int)(iw * resize_scale);
        cv::resize(image, image, cv::Size(nw, nh));
        int top    = (oh - nh) / 2;
        int bottom = (oh - nh) - top;
        int left   = (ow - nw) / 2;
        int right  = (ow - nw) - left;
        cv::copyMakeBorder(image, image, top, bottom, left, right, cv::BORDER_CONSTANT, cv::Scalar::all(0));
        cv::cvtColor(image, image, cv::COLOR_BGR2RGB);

        ma_img_t img;
        img.data   = (uint8_t*)image.data;
        img.size   = image.rows * image.cols * image.channels();
        img.width  = image.cols;
        img.height = image.rows;
        img.format = MA_PIXEL_FORMAT_RGB888;
        img.rotate = MA_PIXEL_ROTATE_0;

        reply["data"]["resolution"] = json::array({width, height});

        detector->run(&img);

        if (detector != nullptr) {
            err                    = detector->run(nullptr);
            auto _perf             = detector->getPerf();
            auto _results          = detector->getResults();
            take                   = _perf.postprocess + _perf.inference + _perf.preprocess;
            reply["data"]["boxes"] = json::array();
            std::vector<ma_bbox_t> _bboxes;
            _bboxes.assign(_results.begin(), _results.end());
            if (trace_) {
                auto tracks             = tracker_.inplace_update(_bboxes);
                reply["data"]["tracks"] = tracks;
                for (int i = 0; i < _bboxes.size(); i++) {
                    reply["data"]["boxes"].push_back({static_cast<int16_t>(_bboxes[i].x * width),
                                                      static_cast<int16_t>(_bboxes[i].y * height),
                                                      static_cast<int16_t>(_bboxes[i].w * width),
                                                      static_cast<int16_t>(_bboxes[i].h * height),
                                                      static_cast<int8_t>(_bboxes[i].score * 100),
                                                      _bboxes[i].target});
                    if (counting_) {
                        counter_.update(tracks[i], _bboxes[i].x * 100, _bboxes[i].y * 100);
                    }
                }
                if (counting_ && _bboxes.size() == 0) {
                    counter_.update(-1, 0, 0);
                }
            } else {
                for (int i = 0; i < _bboxes.size(); i++) {
                    reply["data"]["boxes"].push_back({static_cast<int16_t>(_bboxes[i].x * width),
                                                      static_cast<int16_t>(_bboxes[i].y * height),
                                                      static_cast<int16_t>(_bboxes[i].w * width),
                                                      static_cast<int16_t>(_bboxes[i].h * height),
                                                      static_cast<int8_t>(_bboxes[i].score * 100),
                                                      _bboxes[i].target});
                }
            }
            if (counting_) {
                reply["data"]["counts"] = counter_.get();
                reply["data"]["lines"]  = json::array();
                reply["data"]["lines"].push_back(counter_.getSplitter());
            }

            reply["data"]["perf"].push_back({_perf.preprocess, _perf.inference, _perf.postprocess});
        }

        if (labels_.size() > 0) {
            reply["data"]["labels"] = labels_;
        }

        if (debug_) {
            std::vector<uchar> buffer_;
            std::vector<int> params_ = {cv::IMWRITE_JPEG_QUALITY, 90};
            cv::cvtColor(image, image, cv::COLOR_RGB2BGR);
            cv::imencode(".jpg", image, buffer_, params_);
            // convert to base64
            char* base64_data = new char[4 * ((buffer_.size() + 2) / 3) + 2];
            int base64_len    = buffer_.size() * 4 / 3 + 10;
            ma::utils::base64_encode(reinterpret_cast<unsigned char*>(buffer_.data()), buffer_.size(), base64_data, &base64_len);
            reply["data"]["image"] = std::string(base64_data, base64_len);
            delete[] base64_data;
        }

        server_->response(id_, reply);

        Thread::exitCritical();
    }
}

void ModelNode::threadEntryStub(void* obj) {
    reinterpret_cast<ModelNode*>(obj)->threadEntry();
}

ma_err_t ModelNode::onCreate(const json& config) {
    ma_err_t err = MA_OK;
    Guard guard(mutex_);

    labels_.clear();

    if (config.contains("uri") && config["uri"].is_string()) {
        uri_ = config["uri"].get<std::string>();
    }

    if (uri_.empty()) {
        uri_ = DEFAULT_MODEL;
    }

    if (access(uri_.c_str(), R_OK) != 0) {
        MA_THROW(Exception(MA_ENOENT, "model file not found: " + uri_));
    }

    // find model.json
    size_t pos = uri_.find_last_of(".");
    if (pos != std::string::npos) {
        std::string path = uri_.substr(0, pos) + ".json";
        if (access(path.c_str(), R_OK) == 0) {
            std::ifstream ifs(path);
            if (!ifs.is_open()) {
                MA_THROW(Exception(MA_EINVAL, "model json not found: " + path));
            }
            ifs >> info_;
            if (info_.is_object()) {
                if (info_.contains("classes") && info_["classes"].is_array()) {
                    labels_ = info_["classes"].get<std::vector<std::string>>();
                }
            }
        }
    }

    // override classes
    if (labels_.size() == 0 && config.contains("labels") && config["labels"].is_array() && config["labels"].size() > 0) {
        labels_ = config["labels"].get<std::vector<std::string>>();
    }

    MA_TRY {
        engine_ = new EngineHailoRT();

        if (engine_ == nullptr) {
            MA_THROW(Exception(MA_ENOMEM, "Engine create failed"));
        }
        if (engine_->init() != MA_OK) {
            MA_THROW(Exception(MA_EINVAL, "Engine init failed"));
        }
        if (engine_->load(uri_) != MA_OK) {
            MA_THROW(Exception(MA_EINVAL, "Engine load failed"));
        }
        model_ = new YoloV8NMS(engine_);
        if (model_ == nullptr) {
            MA_THROW(Exception(MA_ENOTSUP, "Model Not Supported"));
        }

        MA_LOGI(TAG, "model: %s %s", uri_.c_str(), model_->getName());
        {  // extra config
            if (config.contains("tscore")) {
                model_->setConfig(MA_MODEL_CFG_OPT_THRESHOLD, config["tscore"].get<float>());
            }
            if (config.contains("tiou")) {
                model_->setConfig(MA_MODEL_CFG_OPT_NMS, config["tiou"].get<float>());
            }
            if (config.contains("topk")) {
                model_->setConfig(MA_MODEL_CFG_OPT_TOPK, config["tiou"].get<float>());
            }
            if (config.contains("debug")) {
                debug_ = config["debug"].get<bool>();
            }
            if (config.contains("trace")) {
                trace_ = config["trace"].get<bool>();
            }
            if (config.contains("counting")) {
                counting_ = config["counting"].get<bool>();
            }
            if (config.contains("splitter") && config["splitter"].is_array()) {
                counter_.setSplitter(config["splitter"].get<std::vector<int16_t>>());
            }
        }

        thread_ = new Thread((type_ + "#" + id_).c_str(), &ModelNode::threadEntryStub, this);
        if (thread_ == nullptr) {
            MA_THROW(Exception(MA_ENOMEM, "Thread create failed"));
        }
    }
    MA_CATCH(ma::Exception & e) {
        if (engine_ != nullptr) {
            delete engine_;
            engine_ = nullptr;
        }
        if (model_ != nullptr) {
            delete model_;
            model_ = nullptr;
        }
        if (thread_ != nullptr) {
            delete thread_;
            thread_ = nullptr;
        }
        MA_THROW(e);
    }
    MA_CATCH(std::exception & e) {
        if (engine_ != nullptr) {
            delete engine_;
            engine_ = nullptr;
        }
        if (model_ != nullptr) {
            delete model_;
            model_ = nullptr;
        }
        if (thread_ != nullptr) {
            delete thread_;
            thread_ = nullptr;
        }
        MA_THROW(Exception(MA_EINVAL, e.what()));
    }

    created_ = true;

    server_->response(id_, json::object({{"type", MA_MSG_TYPE_RESP}, {"name", "create"}, {"code", MA_OK}, {"data", info_}}));

    return MA_OK;
}

ma_err_t ModelNode::onControl(const std::string& control, const json& data) {
    Guard guard(mutex_);
    ma_err_t err = MA_OK;
    if (control == "config") {
        if (data.contains("tscore") && data["tscore"].is_number_float()) {
            model_->setConfig(MA_MODEL_CFG_OPT_THRESHOLD, data["tscore"].get<float>());
        }
        if (data.contains("tiou") && data["tiou"].is_number_float()) {
            model_->setConfig(MA_MODEL_CFG_OPT_NMS, data["tiou"].get<float>());
        }
        if (data.contains("topk") && data["topk"].is_number_integer()) {
            model_->setConfig(MA_MODEL_CFG_OPT_TOPK, data["tiou"].get<int32_t>());
        }
        if (data.contains("debug") && data["debug"].is_boolean()) {
            debug_ = data["debug"].get<bool>();
        }
        if (data.contains("trace") && data["trace"].is_boolean()) {
            trace_ = data["trace"].get<bool>();
            tracker_.clear();
        }
        if (data.contains("counting") && data["counting"].is_boolean()) {
            counting_ = data["counting"].get<bool>();
            counter_.clear();
        }
        if (data.contains("splitter") && data["splitter"].is_array()) {
            counter_.setSplitter(data["splitter"].get<std::vector<int16_t>>());
        }
        server_->response(id_, json::object({{"type", MA_MSG_TYPE_RESP}, {"name", control}, {"code", MA_OK}, {"data", data}}));
    } else {
        server_->response(id_, json::object({{"type", MA_MSG_TYPE_RESP}, {"name", control}, {"code", MA_ENOTSUP}, {"data", ""}}));
    }
    return MA_OK;
}

ma_err_t ModelNode::onDestroy() {
    Guard guard(mutex_);

    if (!created_) {
        return MA_OK;
    }

    onStop();

    if (thread_ != nullptr) {
        delete thread_;
        thread_ = nullptr;
    }
    if (engine_ != nullptr) {
        delete engine_;
        engine_ = nullptr;
    }
    if (model_ != nullptr) {
        delete model_;
        model_ = nullptr;
    }

    created_ = false;

    return MA_OK;
}

ma_err_t ModelNode::onStart() {
    Guard guard(mutex_);
    if (started_) {
        return MA_OK;
    }

    const ma_img_t* img = nullptr;

    for (auto& dep : dependencies_) {
        if (dep.second->type() == "camera") {
            camera_ = static_cast<CameraNode*>(dep.second);
            break;
        }
    }

    if (camera_ == nullptr) {
        MA_THROW(Exception(MA_ENOTSUP, "camera not found"));
        return MA_ENOTSUP;
    }

    camera_->attach(&frame_);

    switch (model_->getType()) {
        case MA_MODEL_TYPE_IMCLS:
            img = static_cast<Classifier*>(model_)->getInputImg();
        default:
            img = static_cast<Detector*>(model_)->getInputImg();
    }

    MA_LOGI(TAG, "start model: %s(%s)", type_.c_str(), id_.c_str());
    started_ = true;

    thread_->start(this);

    return MA_OK;
}

ma_err_t ModelNode::onStop() {
    Guard guard(mutex_);
    if (!started_) {
        return MA_OK;
    }
    started_ = false;

    if (thread_ != nullptr) {
        thread_->join();
    }

    if (camera_ != nullptr) {
        camera_->detach(&frame_);
    }

    return MA_OK;
}

REGISTER_NODE_SINGLETON("model", ModelNode);

}  // namespace ma::node