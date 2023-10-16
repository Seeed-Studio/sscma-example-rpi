if(USE_SYSTEM_NCNN)
    set(GLSLANG_TARGET_DIR "GLSLANG-NOTFOUND" CACHE PATH "Absolute path to glslangTargets.cmake directory")
    if(NOT GLSLANG_TARGET_DIR AND NOT DEFINED ENV{GLSLANG_TARGET_DIR})
        message(WARNING "GLSLANG_TARGET_DIR must be defined! USE_SYSTEM_NCNN will be turned off.")
        set(USE_SYSTEM_NCNN OFF)
    else()
        message(STATUS "Using glslang install located at ${GLSLANG_TARGET_DIR}")

        find_package(Threads)

        include("${GLSLANG_TARGET_DIR}/OSDependentTargets.cmake")
        include("${GLSLANG_TARGET_DIR}/OGLCompilerTargets.cmake")
        if(EXISTS "${GLSLANG_TARGET_DIR}/HLSLTargets.cmake")
            # hlsl support can be optional
            include("${GLSLANG_TARGET_DIR}/HLSLTargets.cmake")
        endif()
        include("${GLSLANG_TARGET_DIR}/glslangTargets.cmake")
        include("${GLSLANG_TARGET_DIR}/SPIRVTargets.cmake")

        if (NOT TARGET glslang OR NOT TARGET SPIRV)
            message(WARNING "glslang or SPIRV target not found! USE_SYSTEM_NCNN will be turned off.")
            set(USE_SYSTEM_NCNN OFF)
        endif()
    endif()
endif()

if(USE_SYSTEM_NCNN)
    find_package(ncnn)
    if(NOT TARGET ncnn)
        message(WARNING "ncnn target not found! USE_SYSTEM_NCNN will be turned off.")
        set(USE_SYSTEM_NCNN OFF)
    endif()
endif()

if(NOT USE_SYSTEM_NCNN)
    # build ncnn library
    set(NCNN_DIR ${CMAKE_CURRENT_SOURCE_DIR}/components/ncnn CACHE PATH "Path to ncnn source directory")
    message(STATUS "Using ncnn source located at ${NCNN_DIR}")
    if(NOT EXISTS "${NCNN_DIR}/CMakeLists.txt")
        message(FATAL_ERROR "The submodules were not downloaded! Please update submodules with \"git submodule update --init --recursive\" and try again.")
    endif()

    option(NCNN_INSTALL_SDK "" OFF)
    option(NCNN_PIXEL_ROTATE "" OFF)
    option(NCNN_PIXEL_AFFINE "" OFF)
    option(NCNN_PIXEL_DRAWING "" OFF)
    option(NCNN_VULKAN "" ON)
    option(NCNN_VULKAN_ONLINE_SPIRV "" ON)
    option(NCNN_BUILD_BENCHMARK "" OFF)
    option(NCNN_BUILD_TESTS "" OFF)
    option(NCNN_BUILD_TOOLS "" OFF)
    option(NCNN_BUILD_EXAMPLES "" OFF)
    option(NCNN_DISABLE_RTTI "" ON)
    option(NCNN_DISABLE_EXCEPTION "" ON)
    option(NCNN_INT8 "" ON)

    option(WITH_LAYER_absval "" ON)
    option(WITH_LAYER_argmax "" ON)
    option(WITH_LAYER_batchnorm "" ON)
    option(WITH_LAYER_bias "" ON)
    option(WITH_LAYER_bnll "" ON)
    option(WITH_LAYER_concat "" ON)
    option(WITH_LAYER_convolution "" ON)
    option(WITH_LAYER_crop "" ON)
    option(WITH_LAYER_deconvolution "" ON)
    option(WITH_LAYER_dropout "" ON)
    option(WITH_LAYER_eltwise "" ON)
    option(WITH_LAYER_elu "" ON)
    option(WITH_LAYER_embed "" ON)
    option(WITH_LAYER_exp "" ON)
    option(WITH_LAYER_flatten "" ON)
    option(WITH_LAYER_innerproduct "" ON)
    option(WITH_LAYER_input "" ON)
    option(WITH_LAYER_log "" ON)
    option(WITH_LAYER_lrn "" ON)
    option(WITH_LAYER_memorydata "" ON)
    option(WITH_LAYER_mvn "" ON)
    option(WITH_LAYER_pooling "" ON)
    option(WITH_LAYER_power "" ON)
    option(WITH_LAYER_prelu "" ON)
    option(WITH_LAYER_proposal "" ON)
    option(WITH_LAYER_reduction "" ON)
    option(WITH_LAYER_relu "" ON)
    option(WITH_LAYER_reshape "" ON)
    option(WITH_LAYER_roipooling "" ON)
    option(WITH_LAYER_scale "" ON)
    option(WITH_LAYER_sigmoid "" ON)
    option(WITH_LAYER_slice "" ON)
    option(WITH_LAYER_softmax "" ON)
    option(WITH_LAYER_split "" ON)
    option(WITH_LAYER_spp "" ON)
    option(WITH_LAYER_tanh "" ON)
    option(WITH_LAYER_threshold "" ON)
    option(WITH_LAYER_tile "" ON)
    option(WITH_LAYER_rnn "" ON)
    option(WITH_LAYER_lstm "" ON)
    option(WITH_LAYER_binaryop "" ON)
    option(WITH_LAYER_unaryop "" ON)
    option(WITH_LAYER_convolutiondepthwise "" ON)
    option(WITH_LAYER_padding "" ON)
    option(WITH_LAYER_squeeze "" ON)
    option(WITH_LAYER_expanddims "" ON)
    option(WITH_LAYER_normalize "" ON)
    option(WITH_LAYER_permute "" ON)
    option(WITH_LAYER_priorbox "" ON)
    option(WITH_LAYER_detectionoutput "" ON)
    option(WITH_LAYER_interp "" ON)
    option(WITH_LAYER_deconvolutiondepthwise "" ON)
    option(WITH_LAYER_shufflechannel "" ON)
    option(WITH_LAYER_instancenorm "" ON)
    option(WITH_LAYER_clip "" ON)
    option(WITH_LAYER_reorg "" ON)
    option(WITH_LAYER_yolodetectionoutput "" ON)
    option(WITH_LAYER_quantize "" ON)
    option(WITH_LAYER_dequantize "" ON)
    option(WITH_LAYER_yolov3detectionoutput "" ON)
    option(WITH_LAYER_psroipooling "" ON)
    option(WITH_LAYER_roialign "" ON)
    option(WITH_LAYER_packing "" ON)
    option(WITH_LAYER_requantize "" ON)
    option(WITH_LAYER_cast "" ON)
    option(WITH_LAYER_hardsigmoid "" ON)
    option(WITH_LAYER_selu "" ON)
    option(WITH_LAYER_hardswish "" ON)
    option(WITH_LAYER_noop "" ON)
    option(WITH_LAYER_pixelshuffle "" ON)
    option(WITH_LAYER_deepcopy "" ON)
    option(WITH_LAYER_mish "" ON)
    option(WITH_LAYER_statisticspooling "" ON)
    option(WITH_LAYER_swish "" ON)
    option(WITH_LAYER_gemm "" ON)
    option(WITH_LAYER_groupnorm "" ON)
    option(WITH_LAYER_layernorm "" ON)
    option(WITH_LAYER_softplus "" ON)
    option(WITH_LAYER_gru "" ON)
    option(WITH_LAYER_multiheadattention "" ON)
    option(WITH_LAYER_gelu "" ON)
    option(WITH_LAYER_convolution1d "" ON)
    option(WITH_LAYER_pooling1d "" ON)
    option(WITH_LAYER_convolutiondepthwise1d "" ON)
    option(WITH_LAYER_convolution3d "" ON)
    option(WITH_LAYER_convolutiondepthwise3d "" ON)
    option(WITH_LAYER_pooling3d "" ON)
    option(WITH_LAYER_matmul "" ON)
    option(WITH_LAYER_deconvolution1d "" ON)
    option(WITH_LAYER_deconvolutiondepthwise1d "" ON)
    option(WITH_LAYER_deconvolution3d "" ON)
    option(WITH_LAYER_deconvolutiondepthwise3d "" ON)
    option(WITH_LAYER_einsum "" ON)
    option(WITH_LAYER_deformableconv2d "" ON)

    add_subdirectory(${NCNN_DIR} ncnn)
endif()