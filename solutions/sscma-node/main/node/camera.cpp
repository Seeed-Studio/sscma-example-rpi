#include "camera.h"

namespace ma::node {

static constexpr char TAG[] = "ma::node::camera";

CameraNode::CameraNode(std::string id) : Node("camera", std::move(id)), thread_(nullptr) {}

CameraNode::~CameraNode() {
    onDestroy();
};

ma_err_t CameraNode::onCreate(const json& config) {
    Guard guard(mutex_);

    if (config.contains("preview") && config["preview"].is_boolean()) {
        preview_ = config["preview"].get<bool>();
    }

    const std::string pipeline =
        "libcamerasrc camera-name=/base/axi/pcie@120000/rp1/i2c@88000/ov5647@36 ! video/x-raw,width=1920,height=1080,framerate=30/1,format=RGBx ! videoconvert ! videoscale ! appsink";
    capture_ = new cv2::VideoCapture(pipeline);

    if (capture_->isOpened()) {
        server_->response(id_, json::object({{"type", MA_MSG_TYPE_RESP}, {"name", "create"}, {"code", MA_OK}, {"data", {"width", 1280, "height", 960, "fps", 30}}}));
    } else {
        MA_THROW(Exception(MA_EINVAL, "camera open failed"));
    }

    thread_ = new Thread((type_ + "#" + id_).c_str(), &CameraNode::threadEntryStub, this);
    if (thread_ == nullptr) {
        MA_THROW(Exception(MA_ENOMEM, "Thread create failed"));
    }


    created_ = true;

    return MA_OK;
}
ma_err_t CameraNode::onControl(const std::string& control, const json& data) {
    Guard guard(mutex_);
    return MA_OK;
}

ma_err_t CameraNode::onDestroy() {
    Guard guard(mutex_);

    if (!created_) {
        return MA_OK;
    }

    onStop();

    if (thread_ != nullptr) {
        delete thread_;
        thread_ = nullptr;
    }

    created_ = false;

    return MA_OK;
}

ma_err_t CameraNode::onStart() {
    Guard guard(mutex_);
    if (started_) {
        return MA_OK;
    }
    started_ = true;
    if (thread_ != nullptr) {
        thread_->start(this);
    }
    return MA_OK;
}
ma_err_t CameraNode::onStop() {
    Guard guard(mutex_);
    if (!started_) {
        return MA_OK;
    }
    started_ = false;

    if (thread_ != nullptr) {
        thread_->join();
    }

    capture_->release();
    return MA_OK;
}

void CameraNode::threadEntry() {
    cv2::Mat frame;

    while (started_) {
        if (capture_->read(frame)) {

            for (auto& msgbox : msgboxes) {
                msgbox->post(&frame, Tick::fromMilliseconds(static_cast<int>(30)));
            }

            if (preview_) {
                cv2::Mat preview;
                std::vector<uchar> buffer_;
                cv2::resize(frame, preview, cv2::Size(320, 240), 0, 0, cv2::INTER_LINEAR);
                std::vector<int> params_ = {cv2::IMWRITE_JPEG_QUALITY, 50};
                cv2::imencode(".jpg", preview, buffer_, params_);
                // convert to base64
                char* base64_data = new char[4 * ((buffer_.size() + 2) / 3) + 2];
                int base64_len    = buffer_.size() * 4 / 3 + 10;
                ma::utils::base64_encode(reinterpret_cast<unsigned char*>(buffer_.data()), buffer_.size(), base64_data, &base64_len);
                json reply             = json::object({{"type", MA_MSG_TYPE_EVT}, {"name", "sample"}, {"code", MA_OK}, {"data", {{"count", count_}}}});
                reply["data"]["image"] = std::string(base64_data, base64_len);
                delete[] base64_data;
                server_->response(id_, reply);
            }
        }
    }
}
void CameraNode::threadEntryStub(void* obj) {
    CameraNode* node = static_cast<CameraNode*>(obj);
    node->threadEntry();
}

ma_err_t CameraNode::attach(MessageBox* msgbox) {
    Guard guard(mutex_);
    msgboxes.push_back(msgbox);
    return MA_OK;
}
ma_err_t CameraNode::detach(MessageBox* msgbox) {
    Guard gurad(mutex_);
    auto it = std::find(msgboxes.begin(), msgboxes.end(), msgbox);
    if (it != msgboxes.end()) {
        msgboxes.erase(it);
    }
    return MA_OK;
}

REGISTER_NODE("camera", CameraNode);

}  // namespace ma::node