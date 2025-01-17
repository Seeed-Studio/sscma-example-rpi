#include <unistd.h>

#include "model.h"

namespace ma::node {

using namespace ma::engine;
using namespace ma::model;

static constexpr char TAG[] = "ma::node::model";


#define DEFAULT_MODEL "/usr/share/sscma-node/models/model.hef"

ModelNode::ModelNode(std::string id)
    : Node("model", id), uri_(""), debug_(true), trace_(false), counting_(false), count_(0), engine_(nullptr), model_(nullptr), thread_(nullptr), camera_(nullptr), frame_(30) {}

ModelNode::~ModelNode() {
    onDestroy();
}


void ModelNode::threadEntry() {

    ma_err_t err           = MA_OK;
    Detector* detector     = nullptr;
    Classifier* classifier = nullptr;
    int32_t width          = static_cast<const ma_img_t*>(model_->getInput())->width;
    int32_t height         = static_cast<const ma_img_t*>(model_->getInput())->height;
    int32_t take           = 0;
    cv2::Mat* frame        = nullptr;
    ma_tick_t preprocess   = 0;

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

        cv2::Mat image = frame->clone();

        Thread::enterCritical();
        json reply = json::object({{"type", MA_MSG_TYPE_EVT}, {"name", "invoke"}, {"code", MA_OK}, {"data", {{"count", ++count_}}}});

        // resize & letterbox
        preprocess          = Tick::current();
        int ih              = image.rows;
        int iw              = image.cols;
        int oh              = height;
        int ow              = width;
        double resize_scale = std::min((double)oh / ih, (double)ow / iw);
        int nh              = (int)(ih * resize_scale);
        int nw              = (int)(iw * resize_scale);
        cv2::resize(image, image, cv2::Size(nw, nh));
        int top    = (oh - nh) / 2;
        int bottom = (oh - nh) - top;
        int left   = (ow - nw) / 2;
        int right  = (ow - nw) - left;
        cv2::copyMakeBorder(image, image, top, bottom, left, right, cv2::BORDER_CONSTANT, cv2::Scalar::all(114));
        cv2::cvtColor(image, image, cv2::COLOR_BGR2RGB);
        preprocess = Tick::current() - preprocess;

        reply["data"]["resolution"] = json::array({width, height});

        ma_tensor_t tensor = {.is_physical = false, .is_variable = false};
        tensor.size        = height * width * 3;
        tensor.data.data   = reinterpret_cast<void*>(image.data);
        engine_->setInput(0, tensor);

        reply["data"]["labels"] = json::array();

        if (model_->getOutputType() == MA_OUTPUT_TYPE_BBOX) {
            Detector* detector     = static_cast<Detector*>(model_);
            err                    = detector->run(nullptr);
            auto _results          = detector->getResults();
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
                    if (labels_.size() > _bboxes[i].target) {
                        reply["data"]["labels"].push_back(labels_[_bboxes[i].target]);
                    } else {
                        reply["data"]["labels"].push_back(std::string("N/A-" + std::to_string(_bboxes[i].target)));
                    }
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
                    if (labels_.size() > _bboxes[i].target) {
                        reply["data"]["labels"].push_back(labels_[_bboxes[i].target]);
                    } else {
                        reply["data"]["labels"].push_back(std::string("N/A-" + std::to_string(_bboxes[i].target)));
                    }
                }
            }
            if (counting_) {
                reply["data"]["counts"] = counter_.get();
                reply["data"]["lines"]  = json::array();
                reply["data"]["lines"].push_back(counter_.getSplitter());
            }
        } else if (model_->getOutputType() == MA_OUTPUT_TYPE_CLASS) {
            Classifier* classifier   = static_cast<Classifier*>(model_);
            err                      = classifier->run(nullptr);
            auto _results            = classifier->getResults();
            reply["data"]["classes"] = json::array();
            for (auto& result : _results) {
                reply["data"]["classes"].push_back({static_cast<int8_t>(result.score * 100), result.target});
                if (labels_.size() > result.target) {
                    reply["data"]["labels"].push_back(result.target);
                } else {
                    reply["data"]["labels"].push_back(std::string("N/A-" + std::to_string(result.target)));
                }
            }
        } else if (model_->getOutputType() == MA_OUTPUT_TYPE_KEYPOINT) {
            PoseDetector* pose_detector = static_cast<PoseDetector*>(model_);
            err                         = pose_detector->run(nullptr);
            auto _results               = pose_detector->getResults();
            reply["data"]["keypoints"]  = json::array();
            for (auto& result : _results) {
                json pts = json::array();
                for (auto& pt : result.pts) {
                    pts.push_back({static_cast<int16_t>(pt.x * width), static_cast<int16_t>(pt.y * height), static_cast<int8_t>(pt.z * 100)});
                }
                json box = {static_cast<int16_t>(result.box.x * width),
                            static_cast<int16_t>(result.box.y * height),
                            static_cast<int16_t>(result.box.w * width),
                            static_cast<int16_t>(result.box.h * height),
                            static_cast<int8_t>(result.box.score * 100),
                            result.box.target};
                if (labels_.size() > result.box.target) {
                    reply["data"]["labels"].push_back(labels_[result.box.target]);
                } else {
                    reply["data"]["labels"].push_back(std::string("N/A-" + std::to_string(result.box.target)));
                }
                reply["data"]["keypoints"].push_back({box, pts});
            }
        } else if (model_->getOutputType() == MA_OUTPUT_TYPE_SEGMENT) {
            Segmentor* segmentor      = static_cast<Segmentor*>(model_);
            err                       = segmentor->run(nullptr);
            auto _results             = segmentor->getResults();
            reply["data"]["segments"] = json::array();
            for (auto& result : _results) {
                json box = {static_cast<int16_t>(result.box.x * width),
                            static_cast<int16_t>(result.box.y * height),
                            static_cast<int16_t>(result.box.w * width),
                            static_cast<int16_t>(result.box.h * height),
                            static_cast<int8_t>(result.box.score * 100),
                            result.box.target};
                if (labels_.size() > result.box.target) {
                    reply["data"]["labels"].push_back(labels_[result.box.target]);
                } else {
                    reply["data"]["labels"].push_back(std::string("N/A-" + std::to_string(result.box.target)));
                }
                json mask = {static_cast<int16_t>(result.mask.width), static_cast<int16_t>(result.mask.height)};
                reply["data"]["segments"].push_back({box, mask});
            }
        }

        const auto _perf = model_->getPerf();

        reply["data"]["perf"].push_back({_perf.preprocess + Tick::toMilliseconds(preprocess), _perf.inference, _perf.postprocess});


        if (debug_) {
            std::vector<uchar> buffer_;
            std::vector<int> params_ = {cv2::IMWRITE_JPEG_QUALITY, 90};
            cv2::cvtColor(image, image, cv2::COLOR_RGB2BGR);
            cv2::imencode(".jpg", image, buffer_, params_);
            // convert to base64
            char* base64_data = new char[4 * ((buffer_.size() + 2) / 3) + 2];
            int base64_len    = buffer_.size() * 4 / 3 + 10;
            ma::utils::base64_encode(reinterpret_cast<unsigned char*>(buffer_.data()), buffer_.size(), base64_data, &base64_len);
            reply["data"]["image"] = std::string(base64_data, base64_len);
            delete[] base64_data;
        } else {
            reply["data"]["image"] = "";
        }

        server_->response(id_, reply);

        Thread::exitCritical();
    }
}  // namespace ma::node

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
        engine_ = new EngineDefault();

        if (engine_ == nullptr) {
            MA_THROW(Exception(MA_ENOMEM, "Engine create failed"));
        }
        if (engine_->init() != MA_OK) {
            MA_THROW(Exception(MA_EINVAL, "Engine init failed"));
        }
        if (engine_->load(uri_) != MA_OK) {
            MA_THROW(Exception(MA_EINVAL, "Engine load failed"));
        }
        model_ = ModelFactory::create(engine_);
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