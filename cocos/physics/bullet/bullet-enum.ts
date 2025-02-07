/*
 Copyright (c) 2020 Xiamen Yaji Software Co., Ltd.

 https://www.cocos.com/

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
 */



export enum EBtSharedBodyDirty {
    BODY_RE_ADD = 1,
    GHOST_RE_ADD = 2,
}

export enum btCollisionFlags {
    CF_STATIC_OBJECT = 1,
    CF_KINEMATIC_OBJECT = 2,
    CF_NO_CONTACT_RESPONSE = 4,
    CF_CUSTOM_MATERIAL_CALLBACK = 8, // this allows per-triangle material (friction/restitution)
    CF_CHARACTER_OBJECT = 16,
    CF_DISABLE_VISUALIZE_OBJECT = 32, // disable debug drawing
    CF_DISABLE_SPU_COLLISION_PROCESSING = 64// disable parallel/SPU processing
}

export enum btCollisionObjectTypes {
    CO_COLLISION_OBJECT = 1,
    CO_RIGID_BODY = 2,
    /// CO_GHOST_OBJECT keeps track of all objects overlapping its AABB and that pass its collision filter
    /// It is useful for collision sensors, explosion objects, character controller etc.
    CO_GHOST_OBJECT = 4,
    CO_SOFT_BODY = 8,
    CO_HF_FLUID = 16,
    CO_USER_TYPE = 32,
    CO_FEATHERSTONE_LINK = 64
}

export enum btCollisionObjectStates {
    ACTIVE_TAG = 1,
    ISLAND_SLEEPING = 2,
    WANTS_DEACTIVATION = 3,
    DISABLE_DEACTIVATION = 4,
    DISABLE_SIMULATION = 5,
}

export enum btRigidBodyFlags {
    BT_DISABLE_WORLD_GRAVITY = 1,
    /// The BT_ENABLE_GYROPSCOPIC_FORCE can easily introduce instability
    /// So generally it is best to not enable it.
    /// If really needed, run at a high frequency like 1000 Hertz:
    /// See Demos/GyroscopicDemo for an example use
    BT_ENABLE_GYROPSCOPIC_FORCE = 2
}
