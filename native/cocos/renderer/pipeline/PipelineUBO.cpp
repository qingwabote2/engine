/****************************************************************************
 Copyright (c) 2020-2022 Xiamen Yaji Software Co., Ltd.

 http://www.cocos.com

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated engine source code (the "Software"), a limited,
 worldwide, royalty-free, non-assignable, revocable and non-exclusive license
 to use Cocos Creator solely to develop games on your target platforms. You shall
 not use Cocos Creator software for developing other software or tools that's
 used for developing games. You are not granted to publish, distribute,
 sublicense, and/or sell copies of Cocos Creator.

 The software or tools in this License Agreement are licensed, not sold.
 Xiamen Yaji Software Co., Ltd. reserves all rights not expressly granted to you.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
****************************************************************************/

#include "PipelineUBO.h"
#include "GlobalDescriptorSetManager.h"
#include "PipelineSceneData.h"
#include "RenderPipeline.h"
#include "SceneCulling.h"
#include "application/ApplicationManager.h"
#include "core/Root.h"
#include "forward/ForwardPipeline.h"
#include "gfx-base/GFXDevice.h"
#include "scene/Camera.h"
#include "scene/DirectionalLight.h"
#include "scene/Fog.h"
#include "scene/RenderScene.h"
#include "scene/Shadow.h"
#include "scene/SpotLight.h"

namespace cc {

namespace pipeline {

#define TO_VEC3(dst, src, offset)  \
    (dst)[(offset) + 0] = (src).x; \
    (dst)[(offset) + 1] = (src).y; \
    (dst)[(offset) + 2] = (src).z;
#define TO_VEC4(dst, src, offset)  \
    (dst)[(offset) + 0] = (src).x; \
    (dst)[(offset) + 1] = (src).y; \
    (dst)[(offset) + 2] = (src).z; \
    (dst)[(offset) + 3] = (src).w;

Mat4 matShadowViewProj;

void PipelineUBO::updateGlobalUBOView(const scene::Camera *camera, ccstd::array<float, UBOGlobal::COUNT> *bufferView) {
    auto *const                            root          = Root::getInstance();
    const gfx::Device *                    device        = gfx::Device::getInstance();
    ccstd::array<float, UBOGlobal::COUNT> &uboGlobalView = *bufferView;

    const auto shadingWidth  = std::floor(camera->getWindow()->getWidth());
    const auto shadingHeight = std::floor(camera->getWindow()->getHeight());

    // update UBOGlobal
    uboGlobalView[UBOGlobal::TIME_OFFSET + 0] = root->getCumulativeTime();
    uboGlobalView[UBOGlobal::TIME_OFFSET + 1] = root->getFrameTime();
    uboGlobalView[UBOGlobal::TIME_OFFSET + 2] = static_cast<float>(CC_CURRENT_ENGINE()->getTotalFrames());

    uboGlobalView[UBOGlobal::SCREEN_SIZE_OFFSET + 0] = static_cast<float>(shadingWidth);
    uboGlobalView[UBOGlobal::SCREEN_SIZE_OFFSET + 1] = static_cast<float>(shadingHeight);
    uboGlobalView[UBOGlobal::SCREEN_SIZE_OFFSET + 2] = 1.0F / uboGlobalView[UBOGlobal::SCREEN_SIZE_OFFSET];
    uboGlobalView[UBOGlobal::SCREEN_SIZE_OFFSET + 3] = 1.0F / uboGlobalView[UBOGlobal::SCREEN_SIZE_OFFSET + 1];

    uboGlobalView[UBOGlobal::NATIVE_SIZE_OFFSET + 0] = static_cast<float>(shadingWidth);
    uboGlobalView[UBOGlobal::NATIVE_SIZE_OFFSET + 1] = static_cast<float>(shadingHeight);
    uboGlobalView[UBOGlobal::NATIVE_SIZE_OFFSET + 2] = 1.0F / uboGlobalView[UBOGlobal::NATIVE_SIZE_OFFSET];
    uboGlobalView[UBOGlobal::NATIVE_SIZE_OFFSET + 3] = 1.0F / uboGlobalView[UBOGlobal::NATIVE_SIZE_OFFSET + 1];
}

void PipelineUBO::updateCameraUBOView(const RenderPipeline *pipeline, float *output, const scene::Camera *camera) {
    const auto *const              scene      = camera->getScene();
    const scene::DirectionalLight *mainLight  = scene->getMainLight();
    const auto *                   sceneData  = pipeline->getPipelineSceneData();
    const scene::Shadows *const    shadowInfo = sceneData->getShadows();
    const auto *                   ambient    = sceneData->getAmbient();
    auto *                         fog        = sceneData->getFog();
    const auto                     isHDR      = sceneData->isHDR();

    auto *device = gfx::Device::getInstance();

    const auto shadingWidth  = static_cast<float>(std::floor(camera->getWindow()->getWidth()));
    const auto shadingHeight = static_cast<float>(std::floor(camera->getWindow()->getHeight()));

    output[UBOCamera::SCREEN_SCALE_OFFSET + 0] = sceneData->getShadingScale();
    output[UBOCamera::SCREEN_SCALE_OFFSET + 1] = sceneData->getShadingScale();
    output[UBOCamera::SCREEN_SCALE_OFFSET + 2] = 1.0F / output[UBOCamera::SCREEN_SCALE_OFFSET];
    output[UBOCamera::SCREEN_SCALE_OFFSET + 3] = 1.0F / output[UBOCamera::SCREEN_SCALE_OFFSET + 1];

    const auto exposure                    = camera->getExposure();
    output[UBOCamera::EXPOSURE_OFFSET + 0] = exposure;
    output[UBOCamera::EXPOSURE_OFFSET + 1] = 1.0F / exposure;
    output[UBOCamera::EXPOSURE_OFFSET + 2] = isHDR ? 1.0F : 0.0F;
    output[UBOCamera::EXPOSURE_OFFSET + 3] = 0.0F;

    if (mainLight) {
        const float shadowEnable = (mainLight->isShadowEnabled() && shadowInfo->getType() == scene::ShadowType::SHADOW_MAP) ? 1.0F : 0.0F;
        const Vec4  lightDir(mainLight->getDirection().x, mainLight->getDirection().y, mainLight->getDirection().z, shadowEnable);
        TO_VEC4(output, lightDir, UBOCamera::MAIN_LIT_DIR_OFFSET)
        TO_VEC3(output, mainLight->getColor(), UBOCamera::MAIN_LIT_COLOR_OFFSET)
        if (mainLight->isUseColorTemperature()) {
            const auto &colorTempRGB = mainLight->getColorTemperatureRGB();
            output[UBOCamera::MAIN_LIT_COLOR_OFFSET + 0] *= colorTempRGB.x;
            output[UBOCamera::MAIN_LIT_COLOR_OFFSET + 1] *= colorTempRGB.y;
            output[UBOCamera::MAIN_LIT_COLOR_OFFSET + 2] *= colorTempRGB.z;
        }

        if (isHDR) {
            output[UBOCamera::MAIN_LIT_COLOR_OFFSET + 3] = mainLight->getIlluminanceHDR() * exposure;
        } else {
            output[UBOCamera::MAIN_LIT_COLOR_OFFSET + 3] = mainLight->getIlluminanceLDR();
        }
    } else {
        const Vec4 lightDir(0.0F, 0.0F, 1.0F, 0.0F);
        TO_VEC4(output, lightDir, UBOCamera::MAIN_LIT_DIR_OFFSET);
        TO_VEC4(output, Vec4::ZERO, UBOCamera::MAIN_LIT_COLOR_OFFSET);
    }

    if (ambient != nullptr) {
        auto &skyColor = const_cast<scene::Ambient *>(ambient)->getSkyColor();
        if (isHDR) {
            skyColor.w = ambient->getSkyIllum() * exposure;
        } else {
            skyColor.w = ambient->getSkyIllum();
        }

        output[UBOCamera::AMBIENT_SKY_OFFSET + 0] = skyColor.x;
        output[UBOCamera::AMBIENT_SKY_OFFSET + 1] = skyColor.y;
        output[UBOCamera::AMBIENT_SKY_OFFSET + 2] = skyColor.z;
        output[UBOCamera::AMBIENT_SKY_OFFSET + 3] = skyColor.w;

        const auto &groundAlbedo                     = ambient->getGroundAlbedo();
        output[UBOCamera::AMBIENT_GROUND_OFFSET + 0] = groundAlbedo.x;
        output[UBOCamera::AMBIENT_GROUND_OFFSET + 1] = groundAlbedo.y;
        output[UBOCamera::AMBIENT_GROUND_OFFSET + 2] = groundAlbedo.z;
        output[UBOCamera::AMBIENT_GROUND_OFFSET + 3] = ambient->getMipmapCount();
    }

    // cjh TS doesn't have this logic ?    auto *const envmap = descriptorSet->getTexture(static_cast<uint>(PipelineGlobalBindings::SAMPLER_ENVIRONMENT));
    //     if (envmap != nullptr) {
    //         output[UBOCamera::AMBIENT_GROUND_OFFSET + 3] = static_cast<float>(envmap->getLevelCount());
    //     }

    memcpy(output + UBOCamera::MAT_VIEW_OFFSET, camera->getMatView().m, sizeof(cc::Mat4));
    memcpy(output + UBOCamera::MAT_VIEW_INV_OFFSET, camera->getNode()->getWorldMatrix().m, sizeof(cc::Mat4));
    TO_VEC3(output, camera->getPosition(), UBOCamera::CAMERA_POS_OFFSET);

    memcpy(output + UBOCamera::MAT_PROJ_OFFSET, camera->getMatProj().m, sizeof(cc::Mat4));
    memcpy(output + UBOCamera::MAT_PROJ_INV_OFFSET, camera->getMatProjInv().m, sizeof(cc::Mat4));
    memcpy(output + UBOCamera::MAT_VIEW_PROJ_OFFSET, camera->getMatViewProj().m, sizeof(cc::Mat4));
    memcpy(output + UBOCamera::MAT_VIEW_PROJ_INV_OFFSET, camera->getMatViewProjInv().m, sizeof(cc::Mat4));
    output[UBOCamera::CAMERA_POS_OFFSET + 3] = getCombineSignY();

    output[UBOCamera::SURFACE_TRANSFORM_OFFSET + 0] = static_cast<float>(camera->getSurfaceTransform());

    if (fog != nullptr) {
        const auto &colorTempRGB                       = fog->getColorArray();
        output[UBOCamera::GLOBAL_FOG_COLOR_OFFSET]     = colorTempRGB.x;
        output[UBOCamera::GLOBAL_FOG_COLOR_OFFSET + 1] = colorTempRGB.y;
        output[UBOCamera::GLOBAL_FOG_COLOR_OFFSET + 2] = colorTempRGB.z;
        output[UBOCamera::GLOBAL_FOG_COLOR_OFFSET + 3] = colorTempRGB.z;

        output[UBOCamera::GLOBAL_FOG_BASE_OFFSET + 0] = fog->getFogStart();
        output[UBOCamera::GLOBAL_FOG_BASE_OFFSET + 1] = fog->getFogEnd();
        output[UBOCamera::GLOBAL_FOG_BASE_OFFSET + 2] = fog->getFogDensity();

        output[UBOCamera::GLOBAL_FOG_ADD_OFFSET + 0] = fog->getFogTop();
        output[UBOCamera::GLOBAL_FOG_ADD_OFFSET + 1] = fog->getFogRange();
        output[UBOCamera::GLOBAL_FOG_ADD_OFFSET + 2] = fog->getFogAtten();
    }

    output[UBOCamera::GLOBAL_NEAR_FAR_OFFSET + 0] = static_cast<float>(camera->getNearClip());
    output[UBOCamera::GLOBAL_NEAR_FAR_OFFSET + 1] = static_cast<float>(camera->getFarClip());

    output[UBOCamera::GLOBAL_VIEW_PORT_OFFSET + 0] = sceneData->getShadingScale() * static_cast<float>(camera->getWindow()->getWidth()) * camera->getViewport().x;
    output[UBOCamera::GLOBAL_VIEW_PORT_OFFSET + 1] = sceneData->getShadingScale() * static_cast<float>(camera->getWindow()->getHeight()) * camera->getViewport().y;
    output[UBOCamera::GLOBAL_VIEW_PORT_OFFSET + 2] = sceneData->getShadingScale() * static_cast<float>(camera->getWindow()->getWidth()) * camera->getViewport().z;
    output[UBOCamera::GLOBAL_VIEW_PORT_OFFSET + 3] = sceneData->getShadingScale() * static_cast<float>(camera->getWindow()->getHeight()) * camera->getViewport().w;
}

void PipelineUBO::updateShadowUBOView(const RenderPipeline *pipeline, ccstd::array<float, UBOShadow::COUNT> *bufferView, const scene::Camera *camera) {
    const scene::RenderScene *const        scene      = camera->getScene();
    const scene::DirectionalLight *        mainLight  = scene->getMainLight();
    gfx::Device *                          device     = gfx::Device::getInstance();
    const PipelineSceneData *              sceneData  = pipeline->getPipelineSceneData();
    scene::Shadows *const                  shadowInfo = sceneData->getShadows();
    ccstd::array<float, UBOShadow::COUNT> &shadowUBO  = *bufferView;
    const bool                             hFTexture  = supportsR32FloatTexture(device);

    if (shadowInfo->isEnabled()) {
        if (mainLight && shadowInfo->getType() == scene::ShadowType::SHADOW_MAP) {
            const Mat4 &matShadowView     = shadowInfo->getMatShadowView();
            const Mat4 &matShadowProj     = shadowInfo->getMatShadowProj();
            const Mat4 &matShadowViewProj = shadowInfo->getMatShadowViewProj();

            float nearClamp;
            float farClamp;
            if (mainLight->isShadowFixedArea()) {
                nearClamp = mainLight->getShadowNear();
                farClamp  = mainLight->getShadowFar();
            } else {
                nearClamp = 1.0F;
                farClamp  = shadowInfo->getShadowCameraFar();
            }

            memcpy(shadowUBO.data() + UBOShadow::MAT_LIGHT_VIEW_OFFSET, matShadowView.m, sizeof(matShadowView));

            const float shadowProjDepthInfos[4] = {matShadowProj.m[10], matShadowProj.m[14], matShadowProj.m[11], matShadowProj.m[15]};
            memcpy(shadowUBO.data() + UBOShadow::SHADOW_PROJ_DEPTH_INFO_OFFSET, &shadowProjDepthInfos, sizeof(shadowProjDepthInfos));

            const float shadowProjInfos[4] = {matShadowProj.m[00], matShadowProj.m[05], 1.0F / matShadowProj.m[00], 1.0F / matShadowProj.m[05]};
            memcpy(shadowUBO.data() + UBOShadow::SHADOW_PROJ_INFO_OFFSET, &shadowProjInfos, sizeof(shadowProjInfos));

            memcpy(shadowUBO.data() + UBOShadow::MAT_LIGHT_VIEW_PROJ_OFFSET, matShadowViewProj.m, sizeof(matShadowViewProj));

            const float linear             = 0.0F;
            const float shadowNFLSInfos[4] = {nearClamp, farClamp, linear, 1.0F - mainLight->getShadowSaturation()};
            memcpy(shadowUBO.data() + UBOShadow::SHADOW_NEAR_FAR_LINEAR_SATURATION_INFO_OFFSET, &shadowNFLSInfos, sizeof(shadowNFLSInfos));

            const float shadowWHPBInfos[4] = {shadowInfo->getSize().x, shadowInfo->getSize().y, mainLight->getShadowPcf(), mainLight->getShadowBias()};
            memcpy(shadowUBO.data() + UBOShadow::SHADOW_WIDTH_HEIGHT_PCF_BIAS_INFO_OFFSET, &shadowWHPBInfos, sizeof(shadowWHPBInfos));

            const float packing            = hFTexture ? 0.0F : 1.0F;
            const float shadowLPNNInfos[4] = {0.0F, packing, mainLight->getShadowNormalBias(), 0.0F};
            memcpy(shadowUBO.data() + UBOShadow::SHADOW_LIGHT_PACKING_NBIAS_NULL_INFO_OFFSET, &shadowLPNNInfos, sizeof(shadowLPNNInfos));
        } else if (mainLight && shadowInfo->getType() == scene::ShadowType::PLANAR) {
            updateDirLight(shadowInfo, mainLight, &shadowUBO);
            updatePlanarNormalAndDistance(shadowInfo, &shadowUBO);
        }

        memcpy(shadowUBO.data() + UBOShadow::SHADOW_COLOR_OFFSET, shadowInfo->getShadowColor4f().data(), sizeof(float) * 4);
    }
}

void PipelineUBO::updateShadowUBOLightView(const RenderPipeline *pipeline, ccstd::array<float, UBOShadow::COUNT> *bufferView,
                                           const scene::Light *light) {
    const auto *sceneData  = pipeline->getPipelineSceneData();
    const auto *shadowInfo = sceneData->getShadows();
    auto *      device     = gfx::Device::getInstance();
    auto &      shadowUBO  = *bufferView;
    const bool  hFTexture  = supportsR32FloatTexture(device);
    const float linear     = 0.0F;
    const float packing    = hFTexture ? 0.0F : 1.0F;
    switch (light->getType()) {
        case scene::LightType::DIRECTIONAL: {
            const auto *mainLight         = static_cast<const scene::DirectionalLight *>(light);
            const Mat4 &matShadowView     = shadowInfo->getMatShadowView();
            const Mat4 &matShadowProj     = shadowInfo->getMatShadowProj();
            const Mat4 &matShadowViewProj = shadowInfo->getMatShadowViewProj();

            float nearClamp;
            float farClamp;
            if (mainLight->isShadowFixedArea()) {
                nearClamp = mainLight->getShadowNear();
                farClamp  = mainLight->getShadowFar();
            } else {
                nearClamp = 1.0F;
                farClamp  = shadowInfo->getShadowCameraFar();
            }

            memcpy(shadowUBO.data() + UBOShadow::MAT_LIGHT_VIEW_OFFSET, matShadowView.m, sizeof(matShadowView));

            const float shadowProjDepthInfos[4] = {matShadowProj.m[10], matShadowProj.m[14], matShadowProj.m[11], matShadowProj.m[15]};
            memcpy(shadowUBO.data() + UBOShadow::SHADOW_PROJ_DEPTH_INFO_OFFSET, &shadowProjDepthInfos, sizeof(shadowProjDepthInfos));

            const float shadowProjInfos[4] = {matShadowProj.m[00], matShadowProj.m[05], 1.0F / matShadowProj.m[00], 1.0F / matShadowProj.m[05]};
            memcpy(shadowUBO.data() + UBOShadow::SHADOW_PROJ_INFO_OFFSET, &shadowProjInfos, sizeof(shadowProjInfos));

            memcpy(shadowUBO.data() + UBOShadow::MAT_LIGHT_VIEW_PROJ_OFFSET, matShadowViewProj.m, sizeof(matShadowViewProj));

            const float shadowNFLSInfos[4] = {nearClamp, farClamp, linear, 1.0F - mainLight->getShadowSaturation()};
            memcpy(shadowUBO.data() + UBOShadow::SHADOW_NEAR_FAR_LINEAR_SATURATION_INFO_OFFSET, &shadowNFLSInfos, sizeof(shadowNFLSInfos));

            const float shadowLPNNInfos[4] = {0.0F, packing, mainLight->getShadowNormalBias(), 0.0F};
            memcpy(shadowUBO.data() + UBOShadow::SHADOW_LIGHT_PACKING_NBIAS_NULL_INFO_OFFSET, &shadowLPNNInfos, sizeof(shadowLPNNInfos));

            const float shadowWHPBInfos[4] = {shadowInfo->getSize().x, shadowInfo->getSize().y, mainLight->getShadowPcf(), mainLight->getShadowBias()};
            memcpy(shadowUBO.data() + UBOShadow::SHADOW_WIDTH_HEIGHT_PCF_BIAS_INFO_OFFSET, &shadowWHPBInfos, sizeof(shadowWHPBInfos));
        } break;
        case scene::LightType::SPOT: {
            const auto *spotLight       = static_cast<const scene::SpotLight *>(light);
            const auto &matShadowCamera = spotLight->getNode()->getWorldMatrix();
            const auto  matShadowView   = matShadowCamera.getInversed();
            memcpy(shadowUBO.data() + UBOShadow::MAT_LIGHT_VIEW_OFFSET, matShadowView.m, sizeof(matShadowView));

            Mat4::createPerspective(spotLight->getSpotAngle(), spotLight->getAspect(), 0.001F, spotLight->getRange(), &matShadowViewProj);

            matShadowViewProj.multiply(matShadowView);
            memcpy(shadowUBO.data() + UBOShadow::MAT_LIGHT_VIEW_PROJ_OFFSET, matShadowViewProj.m, sizeof(matShadowViewProj));

            const float shadowNFLSInfos[4] = {0.01F, spotLight->getRange(), linear, 0.0F};
            memcpy(shadowUBO.data() + UBOShadow::SHADOW_NEAR_FAR_LINEAR_SATURATION_INFO_OFFSET, &shadowNFLSInfos, sizeof(shadowNFLSInfos));

            const float shadowLPNNInfos[4] = {1.0F, packing, spotLight->getShadowNormalBias(), 0.0F};
            memcpy(shadowUBO.data() + UBOShadow::SHADOW_LIGHT_PACKING_NBIAS_NULL_INFO_OFFSET, &shadowLPNNInfos, sizeof(shadowLPNNInfos));

            const float shadowWHPBInfos[4] = {shadowInfo->getSize().x, shadowInfo->getSize().y, spotLight->getShadowPcf(), spotLight->getShadowBias()};
            memcpy(shadowUBO.data() + UBOShadow::SHADOW_WIDTH_HEIGHT_PCF_BIAS_INFO_OFFSET, &shadowWHPBInfos, sizeof(shadowWHPBInfos));
        } break;
        case scene::LightType::SPHERE: break;
        case scene::LightType::UNKNOWN: break;
        default:
            break;
    }

    memcpy(shadowUBO.data() + UBOShadow::SHADOW_COLOR_OFFSET, shadowInfo->getShadowColor4f().data(), sizeof(float) * 4);
}

static uint8_t combineSignY = 0;
uint8_t        PipelineUBO::getCombineSignY() {
    return combineSignY;
}

void PipelineUBO::initCombineSignY() {
    const float screenSpaceSignY = _device->getCapabilities().screenSpaceSignY * 0.5F + 0.5F;
    const float clipSpaceSignY   = _device->getCapabilities().clipSpaceSignY * 0.5F + 0.5F;
    combineSignY                 = static_cast<uint8_t>(screenSpaceSignY) << 1 | static_cast<uint8_t>(clipSpaceSignY);
}

void PipelineUBO::activate(gfx::Device *device, RenderPipeline *pipeline) {
    _device   = device;
    _pipeline = pipeline;

    auto *descriptorSet = pipeline->getDescriptorSet();
    initCombineSignY();
    auto *globalUBO = _device->createBuffer({
        gfx::BufferUsageBit::UNIFORM | gfx::BufferUsageBit::TRANSFER_DST,
        gfx::MemoryUsageBit::HOST | gfx::MemoryUsageBit::DEVICE,
        UBOGlobal::SIZE,
        UBOGlobal::SIZE,
        gfx::BufferFlagBit::NONE,
    });
    descriptorSet->bindBuffer(UBOGlobal::BINDING, globalUBO);
    _ubos.push_back(globalUBO);

    _alignedCameraUBOSize = utils::alignTo(UBOCamera::SIZE, _device->getCapabilities().uboOffsetAlignment);

    _cameraBuffer = _device->createBuffer({
        gfx::BufferUsageBit::UNIFORM | gfx::BufferUsageBit::TRANSFER_DST,
        gfx::MemoryUsageBit::HOST | gfx::MemoryUsageBit::DEVICE,
        _alignedCameraUBOSize,
        _alignedCameraUBOSize,
    });
    _ubos.push_back(_cameraBuffer);
    _cameraUBOs.resize(_alignedCameraUBOSize / sizeof(float));

    auto *cameraUBO = _device->createBuffer({
        _cameraBuffer,
        0,
        UBOCamera::SIZE,
    });
    descriptorSet->bindBuffer(UBOCamera::BINDING, cameraUBO);
    _ubos.push_back(cameraUBO);

    auto *shadowUBO = _device->createBuffer({
        gfx::BufferUsageBit::UNIFORM | gfx::BufferUsageBit::TRANSFER_DST,
        gfx::MemoryUsageBit::DEVICE,
        UBOShadow::SIZE,
        UBOShadow::SIZE,
        gfx::BufferFlagBit::NONE,
    });
    descriptorSet->bindBuffer(UBOShadow::BINDING, shadowUBO);
    _ubos.push_back(shadowUBO);
}

void PipelineUBO::destroy() {
    for (auto &ubo : _ubos) {
        CC_SAFE_DESTROY_AND_DELETE(ubo)
    }
    _ubos.clear();
}

void PipelineUBO::updateGlobalUBO(const scene::Camera *camera) {
    auto *const globalDSManager = _pipeline->getGlobalDSManager();
    auto *const ds              = _pipeline->getDescriptorSet();
    ds->update();
    PipelineUBO::updateGlobalUBOView(camera, &_globalUBO);
    ds->getBuffer(UBOGlobal::BINDING)->update(_globalUBO.data(), UBOGlobal::SIZE);

    globalDSManager->bindBuffer(UBOGlobal::BINDING, ds->getBuffer(UBOGlobal::BINDING));
    globalDSManager->update();
}

void PipelineUBO::updateCameraUBO(const scene::Camera *camera) {
    auto *const cmdBuffer = _pipeline->getCommandBuffers()[0];
    PipelineUBO::updateCameraUBOView(_pipeline, _cameraUBOs.data(), camera);
    cmdBuffer->updateBuffer(_cameraBuffer, _cameraUBOs.data());
}

void PipelineUBO::updateMultiCameraUBO(const ccstd::vector<scene::Camera *> &cameras) {
    const auto cameraCount  = cameras.size();
    const auto totalUboSize = static_cast<uint>(_alignedCameraUBOSize * cameraCount);

    if (_cameraBuffer->getSize() < totalUboSize) {
        _cameraBuffer->resize(totalUboSize);
        _cameraUBOs.resize(totalUboSize / sizeof(float));
    }

    for (uint cameraIdx = 0; cameraIdx < cameraCount; ++cameraIdx) {
        const auto *camera = cameras[cameraIdx];
        const auto  offset = cameraIdx * _alignedCameraUBOSize / sizeof(float);
        PipelineUBO::updateCameraUBOView(_pipeline, &_cameraUBOs[offset], camera);
    }
    _cameraBuffer->update(_cameraUBOs.data());

    _currentCameraUBOOffset = 0;
}

void PipelineUBO::updateShadowUBO(const scene::Camera *camera) {
    auto *const       ds         = _pipeline->getDescriptorSet();
    auto *const       cmdBuffer  = _pipeline->getCommandBuffers()[0];
    const auto *      sceneData  = _pipeline->getPipelineSceneData();
    const auto *      shadowInfo = sceneData->getShadows();
    const auto *const scene      = camera->getScene();
    if (shadowInfo == nullptr || !shadowInfo->isEnabled()) {
        return;
    }

    const auto &                   shadowFrameBufferMap = sceneData->getShadowFramebufferMap();
    const scene::DirectionalLight *mainLight            = scene->getMainLight();
    if (mainLight && shadowInfo->getType() == scene::ShadowType::SHADOW_MAP) {
        if (shadowFrameBufferMap.count(mainLight) > 0) {
            auto *texture = shadowFrameBufferMap.at(mainLight)->getColorTextures()[0];
            if (texture) {
                ds->bindTexture(SHADOWMAP::BINDING, texture);
            }
        }
    }
    PipelineUBO::updateShadowUBOView(_pipeline, &_shadowUBO, camera);
    ds->update();
    cmdBuffer->updateBuffer(ds->getBuffer(UBOShadow::BINDING), _shadowUBO.data(), UBOShadow::SIZE);
}

void PipelineUBO::updateShadowUBOLight(gfx::DescriptorSet *globalDS, const scene::Light *light) {
    auto *const cmdBuffer = _pipeline->getCommandBuffers()[0];
    PipelineUBO::updateShadowUBOLightView(_pipeline, &_shadowUBO, light);
    globalDS->update();
    cmdBuffer->updateBuffer(globalDS->getBuffer(UBOShadow::BINDING), _shadowUBO.data(), UBOShadow::SIZE);
}

void PipelineUBO::updateShadowUBORange(uint offset, const Mat4 *data) {
    memcpy(_shadowUBO.data() + offset, data->m, sizeof(*data));
}

uint PipelineUBO::getCurrentCameraUBOOffset() const {
    return _currentCameraUBOOffset;
}

void PipelineUBO::incCameraUBOOffset() {
    _currentCameraUBOOffset += _alignedCameraUBOSize;
}

} // namespace pipeline
} // namespace cc
