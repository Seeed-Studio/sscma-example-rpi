#include "ma_engine_hailort.h"

#if defined(__unix__)
#include <sys/mman.h>
#endif

#include <chrono>
#include <future>

#include <cstring>

#if MA_USE_ENGINE_HAILORT

namespace ma::engine {

constexpr char TAG[] = "ma::engine::hailort";
using namespace std::chrono_literals;


EngineHailoRT::EngineHailoRT() : _vdevice(nullptr), _model(nullptr), _configured_model(nullptr), _bindings(nullptr), input_tensors(), output_tensors() {}

EngineHailoRT::~EngineHailoRT() {
    // TODO: recycle tensors

    for (auto& tensor : input_tensors) {
        munmap(tensor.data.data, tensor.size);
    }
    for (auto& tensor : output_tensors) {
        munmap(tensor.data.data, tensor.size);
    }
}

ma_err_t EngineHailoRT::init() {
    auto vdevice = VDevice::create();
    if (!vdevice) {
        MA_LOGE(TAG, "Failed to create VDevice %d", vdevice.status());
        return MA_EIO;
    }
    _vdevice = move(vdevice.value());

    return MA_OK;
}


ma_err_t EngineHailoRT::init(size_t size) {
    return init();
}

ma_err_t EngineHailoRT::init(void* pool, size_t size) {
    return init();
}

ma_err_t EngineHailoRT::load(const std::string& model_path) {
    return load(model_path.c_str());
}

ma_err_t EngineHailoRT::load(const void* model_data, size_t model_size) {
    return MA_ENOTSUP;
}

ma_err_t EngineHailoRT::load(const char* model_path) {
    if (!_vdevice) {
        return MA_EIO;
    }

    auto hef = Hef::create(model_path);

    if (!hef) {
        return MA_EINVAL;
    }

    auto model = _vdevice->create_infer_model(model_path);

    if (!model) {
        return MA_EIO;
    }

    _model = move(model.value());

    _model->set_hw_latency_measurement_flags(HAILO_LATENCY_NONE);

    const auto inputs  = _model->inputs();
    const auto outputs = _model->outputs();

    auto configured_model = _model->configure();

    if (!configured_model) {
        return MA_EIO;
    }

    {
        auto shared       = new ConfiguredInferModel(configured_model.value());
        _configured_model = std::shared_ptr<ConfiguredInferModel>(shared, [dep_ref = _model](ConfiguredInferModel* ptr) {
            delete ptr;
            ptr = nullptr;
            dep_ref.~shared_ptr();
        });
    }

    auto bindings = configured_model.value().create_bindings();
    if (!bindings) {
        return MA_EIO;
    }

    {
        auto shared = new ConfiguredInferModel::Bindings(bindings.value());
        _bindings   = std::shared_ptr<ConfiguredInferModel::Bindings>(shared, [dep_ref = _configured_model](ConfiguredInferModel::Bindings* ptr) {
            delete ptr;
            ptr = nullptr;
            dep_ref.~shared_ptr();
        });
    }

    for (auto& tsr : inputs) {
        ma_tensor_t tensor{0};
        tensor.data.data = mmap(NULL, tsr.get_frame_size(), PROT_WRITE | PROT_READ, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
        tensor.type      = MA_TENSOR_TYPE_U8;
        tensor.size      = tsr.get_frame_size();
        // tensor.name          = tsr.name().c_str();
        tensor.shape.size    = 4;
        tensor.shape.dims[0] = 1;
        tensor.shape.dims[1] = tsr.shape().height;
        tensor.shape.dims[2] = tsr.shape().width;
        tensor.shape.dims[3] = tsr.shape().features;
        if (tsr.get_quant_infos().size() == 1) {
            tensor.quant_param.scale      = tsr.get_quant_infos()[0].qp_scale;
            tensor.quant_param.zero_point = tsr.get_quant_infos()[0].qp_zp;
        }
        tensor.is_variable = true;
        input_tensors.emplace_back(std::move(tensor));

        _bindings->input(tsr.name())->set_buffer(MemoryView(tensor.data.data, tensor.size));
    }

    for (auto& tsr : outputs) {
        ma_tensor_t tensor{0};

        tensor.data.data = mmap(NULL, tsr.get_frame_size(), PROT_WRITE | PROT_READ, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
        tensor.type      = MA_TENSOR_TYPE_U8;
        tensor.size      = tsr.get_frame_size();
        // tensor.name          = tsr.name().c_str();
        tensor.shape.size    = 4;
        tensor.shape.dims[0] = 1;
        tensor.shape.dims[1] = tsr.shape().height;
        tensor.shape.dims[2] = tsr.shape().width;
        tensor.shape.dims[3] = tsr.shape().features;
        if (tsr.get_quant_infos().size() == 1) {
            tensor.quant_param.scale      = tsr.get_quant_infos()[0].qp_scale;
            tensor.quant_param.zero_point = tsr.get_quant_infos()[0].qp_zp;
        }
        tensor.is_variable = true;
        output_tensors.emplace_back(std::move(tensor));
        _bindings->output(tsr.name())->set_buffer(MemoryView(tensor.data.data, tensor.size));
    }

    return MA_OK;
}

ma_err_t EngineHailoRT::run() {

    if (!_configured_model || !_bindings) {
        return MA_EINVAL;
    }

    auto sta = _configured_model->wait_for_async_ready(1000ms);
    if (sta != HAILO_SUCCESS) {
        return MA_EINVAL;
    }

    auto job = _configured_model->run_async(*_bindings, [](const AsyncInferCompletionInfo& info) { MA_LOGD(TAG, "AsyncInferCompletionInfo %d", info.status); });

    do {
        std::this_thread::yield();
    } while (job->wait(1000ms) != HAILO_SUCCESS);

    return MA_OK;
}

ma_tensor_t EngineHailoRT::getInput(int32_t index) {
    return input_tensors[index];
}

ma_tensor_t EngineHailoRT::getOutput(int32_t index) {
    return output_tensors[index];
}

ma_shape_t EngineHailoRT::getInputShape(int32_t index) {
    return input_tensors[index].shape;
}

ma_shape_t EngineHailoRT::getOutputShape(int32_t index) {
    return output_tensors[index].shape;
}

ma_quant_param_t EngineHailoRT::getInputQuantParam(int32_t index) {
    return input_tensors[index].quant_param;
}

ma_quant_param_t EngineHailoRT::getOutputQuantParam(int32_t index) {
    return output_tensors[index].quant_param;
}

int32_t EngineHailoRT::getInputSize() {
    return input_tensors.size();
}

int32_t EngineHailoRT::getOutputSize() {
    return output_tensors.size();
}

// int32_t EngineHailoRT::getInputNum(const char* name) {
//     return 0;
// }

// int32_t EngineHailoRT::getOutputNum(const char* name) {
//     return 0;
// }

ma_err_t EngineHailoRT::setInput(int32_t index, const ma_tensor_t& tensor) {
    std::memcpy(input_tensors[index].data.data, tensor.data.data, tensor.size);
    return MA_OK;
}

}  // namespace ma::engine

#endif