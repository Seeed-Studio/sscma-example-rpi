#ifndef _MA_ENGINE_HAILORT_H_
#define _MA_ENGINE_HAILORT_H_

#include <cstddef>
#include <cstdint>

#include "core/ma_common.h"

#define MA_USE_ENGINE_HAILORT 1

#if MA_USE_ENGINE_HAILORT

#include "core/engine/ma_engine_base.h"
#include <hailo/hailort.hpp>

namespace ma::engine {
using namespace hailort;
class EngineHailoRT final : public Engine {

public:
    EngineHailoRT();
    ~EngineHailoRT();

    ma_err_t init() override;
    ma_err_t init(size_t size) override;
    ma_err_t init(void* pool, size_t size) override;

    ma_err_t run() override;

    ma_err_t load(const void* model_data, size_t model_size) override;

#if MA_USE_FILESYSTEM
    ma_err_t load(const char* model_path) override;
    ma_err_t load(const std::string& model_path) override;
#endif

    int32_t getInputSize() override;
    int32_t getOutputSize() override;
    ma_tensor_t getInput(int32_t index) override;
    ma_tensor_t getOutput(int32_t index) override;
    ma_shape_t getInputShape(int32_t index) override;
    ma_shape_t getOutputShape(int32_t index) override;
    ma_quant_param_t getInputQuantParam(int32_t index) override;
    ma_quant_param_t getOutputQuantParam(int32_t index) override;

    ma_err_t setInput(int32_t index, const ma_tensor_t& tensor) override;

    // int32_t getInputNum(const char* name) override;
    // int32_t getOutputNum(const char* name) override;

private:
    std::unique_ptr<VDevice> _vdevice;
    std::shared_ptr<InferModel> _model;
    std::shared_ptr<ConfiguredInferModel> _configured_model;
    std::shared_ptr<ConfiguredInferModel::Bindings> _bindings;

    std::vector<ma_tensor_t> input_tensors;
    std::vector<ma_tensor_t> output_tensors;
};
}  // namespace ma::engine


#endif

#endif
