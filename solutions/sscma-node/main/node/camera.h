#pragma once

#include <opencv2/opencv.hpp>

namespace cv2 = cv;

#include "node.h"
#include "server.h"

namespace ma::node {

class videoFrame {
public:
    videoFrame() : ref_cnt(0), base64(nullptr), base64_len(0), timestamp(0) {
        memset(&img, 0, sizeof(ma_img_t));
    }
    ~videoFrame() = default;
    inline void ref(int n = 1) {
        ref_cnt.fetch_add(n, std::memory_order_relaxed);
    }
    inline void release() {
        if (ref_cnt.load(std::memory_order_relaxed) == 0 || ref_cnt.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            if (!img.physical) {
                delete[] img.data;
                if (base64) {
                    delete[] base64;
                }
            }
            delete this;
        }
    }
    ma_tick_t timestamp;
    std::atomic<int> ref_cnt;
    char* base64;
    int base64_len;
    ma_img_t img;
};


class CameraNode : public Node {
public:
    CameraNode(std::string id);
    ~CameraNode();

    ma_err_t onCreate(const json& config) override;
    ma_err_t onStart() override;
    ma_err_t onControl(const std::string& control, const json& data) override;
    ma_err_t onStop() override;
    ma_err_t onDestroy() override;

    ma_err_t attach(MessageBox* msgbox);
    ma_err_t detach(MessageBox* msgbox);

protected:
    void threadEntry();
    static void threadEntryStub(void* obj);

private:
    uint32_t count_;
    bool preview_;
    int option_;
    Thread* thread_;
    cv2::VideoCapture* capture_;
    std::vector<MessageBox*> msgboxes;
};

}  // namespace ma::node
