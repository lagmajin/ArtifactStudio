// Collaboration Server Prototype
// npm init -y && npm install ws
// node server.js

const WebSocket = require('ws');
const http = require('http');
const fs = require('fs');
const path = require('path');
const crypto = require('crypto');
const { isDeepStrictEqual } = require('util');

const PORT = process.env.PORT ? Number(process.env.PORT) : 8080;
const HOST = process.env.HOST || '127.0.0.1';
const DATA_DIR = process.env.COLLAB_DATA_DIR || path.join(__dirname, 'data');
const MAX_HISTORY_BYTES = readPositiveIntegerEnv('COLLAB_MAX_HISTORY_BYTES', 16 * 1024 * 1024);
const MAX_HISTORY_OPERATIONS = readPositiveIntegerEnv('COLLAB_MAX_HISTORY_OPERATIONS', 100000);
const MAX_WS_MESSAGE_BYTES = 2 * 1024 * 1024;
const MAX_PRESENCE_BYTES = 64 * 1024;
// Keep aligned with ArtifactCore::InterpolationType::CircularInOut.
const MAX_KEYFRAME_INTERPOLATION = 48;
const MAX_TOTAL_CLIENTS = readPositiveIntegerEnv('COLLAB_MAX_TOTAL_CLIENTS', 256);
const MAX_ROOM_CLIENTS = readPositiveIntegerEnv('COLLAB_MAX_ROOM_CLIENTS', 64);
const MAX_LOCKS_PER_ROOM = readPositiveIntegerEnv('COLLAB_MAX_LOCKS_PER_ROOM', 10000);
const MAX_LOCKS_PER_CLIENT = readPositiveIntegerEnv('COLLAB_MAX_LOCKS_PER_CLIENT', 256);
const LOCK_LEASE_MS = readPositiveIntegerEnv('COLLAB_LOCK_LEASE_MS', 90000);
const COLLAB_ACCESS_TOKEN_BYTES = Buffer.from(process.env.COLLAB_ACCESS_TOKEN || '', 'utf8');
const COLLAB_VIEW_TOKEN_BYTES = Buffer.from(process.env.COLLAB_VIEW_TOKEN || '', 'utf8');
const LOCK_PROTECTED_OPERATION_TYPES = new Set([
    'property.set', 'property.batch', 'property.keyframes', 'property.expression',
    'layer.components', 'layer.stack', 'layer.animationStack', 'layer.audioDeClickRanges', 'layer.deformation2D', 'layer.solidSize', 'layer.sourceCrop', 'layer.shapePolygon', 'layer.shapePath', 'layer.shapeOperator', 'layer.shapeContents', 'layer.text', 'layer.rename', 'layer.variant', 'layer.blendMode', 'layer.opacity', 'layer.parent', 'layer.visibility', 'layer.flag', 'layer.editLock',
    'layer.transform', 'layer.moveAtFrame', 'layer.add', 'layer.remove', 'layer.reorder'
]);
const KNOWN_OPERATION_TYPES = new Set([
    'property.set', 'property.batch', 'property.keyframes', 'property.expression',
    'layer.components', 'layer.stack', 'layer.animationStack', 'layer.audioDeClickRanges', 'layer.deformation2D', 'layer.solidSize', 'layer.sourceCrop', 'layer.shapePolygon', 'layer.shapePath', 'layer.shapeOperator', 'layer.shapeContents', 'layer.text', 'layer.rename', 'layer.variant', 'layer.blendMode', 'layer.opacity', 'layer.parent', 'layer.visibility', 'layer.flag', 'layer.editLock', 'layer.transform',
    'layer.moveAtFrame', 'layer.add', 'layer.remove', 'layer.reorder',
    'review.comment.add', 'review.comment.resolve',
    'review.comment.edit', 'review.comment.remove'
]);
const LAYER_SCOPED_OPERATION_TYPES = new Set([
    'property.set', 'property.keyframes', 'property.expression', 'layer.components', 'layer.stack', 'layer.animationStack', 'layer.audioDeClickRanges', 'layer.deformation2D', 'layer.solidSize', 'layer.sourceCrop', 'layer.shapePolygon', 'layer.shapePath', 'layer.shapeOperator', 'layer.shapeContents', 'layer.text', 'layer.rename', 'layer.variant', 'layer.blendMode', 'layer.opacity', 'layer.parent', 'layer.visibility', 'layer.flag', 'layer.editLock',
    'layer.transform', 'layer.moveAtFrame',
    'layer.add', 'layer.remove', 'layer.reorder'
]);
if (!Number.isInteger(PORT) || PORT < 1 || PORT > 65535) {
    throw new Error('PORT must be an integer between 1 and 65535');
}
if (LOCK_LEASE_MS < 60000) {
    throw new Error('COLLAB_LOCK_LEASE_MS must be at least 60000 ms to exceed the heartbeat interval');
}
if (COLLAB_ACCESS_TOKEN_BYTES.length > 4096 || COLLAB_VIEW_TOKEN_BYTES.length > 4096) {
    throw new Error('COLLAB_ACCESS_TOKEN and COLLAB_VIEW_TOKEN must each be at most 4096 bytes');
}
if (COLLAB_ACCESS_TOKEN_BYTES.length > 0 &&
    COLLAB_ACCESS_TOKEN_BYTES.equals(COLLAB_VIEW_TOKEN_BYTES)) {
    throw new Error('COLLAB_ACCESS_TOKEN and COLLAB_VIEW_TOKEN must be different');
}

function readPositiveIntegerEnv(name, fallback) {
    if (process.env[name] === undefined || process.env[name] === '') return fallback;
    const value = Number(process.env[name]);
    if (!Number.isSafeInteger(value) || value <= 0) {
        throw new Error(`${name} must be a positive safe integer`);
    }
    return value;
}

function roleForAccessToken(candidate) {
    if (COLLAB_ACCESS_TOKEN_BYTES.length === 0 &&
        COLLAB_VIEW_TOKEN_BYTES.length === 0) return 'editor';
    const candidateBytes = Buffer.from(candidate, 'utf8');
    const matches = expected => expected.length > 0 &&
        candidateBytes.length === expected.length &&
        crypto.timingSafeEqual(candidateBytes, expected);
    if (matches(COLLAB_ACCESS_TOKEN_BYTES)) return 'editor';
    if (matches(COLLAB_VIEW_TOKEN_BYTES)) return 'viewer';
    return '';
}

function isExternalPathFieldKey(fieldKey) {
    const key = fieldKey.toLowerCase();
    return ['sourcepath', 'filepath', 'sequencepaths', 'depthmappath',
        'texturepath', 'proxypath', 'assetpath', 'mediapath', 'sourceuri']
        .some(token => key.includes(token));
}

function hasExternalPathString(value, parentKey = '') {
    if (typeof value === 'string') {
        return isExternalPathFieldKey(parentKey) && value.trim().length > 0;
    }
    if (Array.isArray(value)) {
        return value.some(item => typeof item === 'string'
            ? isExternalPathFieldKey(parentKey) && item.trim().length > 0
            : hasExternalPathString(item, parentKey));
    }
    if (!value || typeof value !== 'object') return false;
    return Object.entries(value).some(([key, child]) =>
        hasExternalPathString(child, key));
}

function isAssetPathPropertyPath(propertyPath) {
    return isExternalPathFieldKey(propertyPath);
}

function validateKnownOperation(operation, authenticatedClientId) {
    if (!KNOWN_OPERATION_TYPES.has(operation.type)) return null;
    const payload = operation.payload;
    if (!payload || typeof payload !== 'object' || Array.isArray(payload)) {
        return `${operation.type} requires an object payload`;
    }
    const nonEmptyString = (value) => typeof value === 'string' && value.trim().length > 0;
    const finiteNumber = (value) => typeof value === 'number' && Number.isFinite(value);
    const requiresLayer = LAYER_SCOPED_OPERATION_TYPES.has(operation.type);
    if (requiresLayer && !nonEmptyString(operation.layerId)) {
        return `${operation.type} requires a non-empty layerId`;
    }
    if (operation.type === 'property.set') {
        if (!nonEmptyString(payload.propertyPath) ||
            !Object.prototype.hasOwnProperty.call(payload, 'value')) {
            return 'property.set requires propertyPath and value';
        }
        if (isAssetPathPropertyPath(payload.propertyPath) ||
            hasExternalPathString(payload.value) ||
            hasExternalPathString(payload.expectedValue)) {
            return 'property.set cannot synchronize local asset paths';
        }
    } else if (operation.type === 'property.batch') {
        if (!Array.isArray(payload.changes) || payload.changes.length < 1 ||
            payload.changes.length > 128) {
            return 'property.batch requires 1 to 128 changes';
        }
        const targets = new Set();
        for (const change of payload.changes) {
            if (!change || typeof change !== 'object' || Array.isArray(change) ||
                !nonEmptyString(change.layerId) ||
                !nonEmptyString(change.propertyPath)) {
                return 'property.batch entries require layerId and propertyPath';
            }
            if (isAssetPathPropertyPath(change.propertyPath) ||
                hasExternalPathString(change.expectedValue) ||
                hasExternalPathString(change.value) ||
                hasExternalPathString(change.expectedKeyframes) ||
                hasExternalPathString(change.keyframes) ||
                hasExternalPathString(change.expectedExpression) ||
                hasExternalPathString(change.expression)) {
                return 'property.batch cannot synchronize local asset paths';
            }
            const target = `${change.layerId}\u001f${change.propertyPath}`;
            if (targets.has(target)) {
                return 'property.batch cannot change the same property twice';
            }
            const kind = change.kind === undefined ? 'value' : change.kind;
            if (kind === 'value') {
                if (!Object.prototype.hasOwnProperty.call(change, 'expectedValue') ||
                    !Object.prototype.hasOwnProperty.call(change, 'value')) {
                    return 'property.batch value entries require expectedValue and value';
                }
            } else if (kind === 'keyframes') {
                const hasExpectedAnimatable = Object.prototype.hasOwnProperty.call(change, 'expectedAnimatable');
                const hasAnimatable = Object.prototype.hasOwnProperty.call(change, 'animatable');
                const validBatchKeyframe = keyframe => keyframe &&
                    typeof keyframe === 'object' && !Array.isArray(keyframe) &&
                    Number.isSafeInteger(keyframe.frame) &&
                    Object.prototype.hasOwnProperty.call(keyframe, 'value') &&
                    Number.isSafeInteger(keyframe.interpolation) &&
                    keyframe.interpolation >= 0 && keyframe.interpolation <= MAX_KEYFRAME_INTERPOLATION &&
                    ['cp1_x', 'cp1_y', 'cp2_x', 'cp2_y'].every(key => finiteNumber(keyframe[key])) &&
                    (Object.prototype.hasOwnProperty.call(keyframe, 'timeValue') ===
                        Object.prototype.hasOwnProperty.call(keyframe, 'timeScale')) &&
                    (!Object.prototype.hasOwnProperty.call(keyframe, 'timeValue') ||
                        (Number.isSafeInteger(keyframe.timeValue) &&
                         Number.isSafeInteger(keyframe.timeScale) && keyframe.timeScale > 0)) &&
                    (!Object.prototype.hasOwnProperty.call(keyframe, 'roving') || typeof keyframe.roving === 'boolean') &&
                    (!Object.prototype.hasOwnProperty.call(keyframe, 'anchor') ||
                        (Number.isSafeInteger(keyframe.anchor) && keyframe.anchor >= 0 && keyframe.anchor <= 3)) &&
                    (!Object.prototype.hasOwnProperty.call(keyframe, 'colorLabel') ||
                        (Number.isSafeInteger(keyframe.colorLabel) && keyframe.colorLabel >= 0 && keyframe.colorLabel <= 6));
                if (!Array.isArray(change.expectedKeyframes) || !Array.isArray(change.keyframes) ||
                    change.expectedKeyframes.length > 100000 || change.keyframes.length > 100000 ||
                    !change.expectedKeyframes.every(validBatchKeyframe) ||
                    !change.keyframes.every(validBatchKeyframe) ||
                    Buffer.byteLength(JSON.stringify(change.expectedKeyframes), 'utf8') > 262144 ||
                    Buffer.byteLength(JSON.stringify(change.keyframes), 'utf8') > 262144 ||
                    hasExpectedAnimatable !== hasAnimatable ||
                    (hasExpectedAnimatable && (typeof change.expectedAnimatable !== 'boolean' ||
                        typeof change.animatable !== 'boolean'))) {
                    return 'property.batch keyframe entries require bounded keyframe arrays and paired animatable flags';
                }
            } else if (kind === 'expression') {
                if (typeof change.expectedExpression !== 'string' ||
                    typeof change.expression !== 'string' ||
                    Buffer.byteLength(change.expectedExpression, 'utf8') > 262144 ||
                    Buffer.byteLength(change.expression, 'utf8') > 262144) {
                    return 'property.batch expression entries require bounded expected and next strings';
                }
            } else {
                return 'property.batch entries require a supported kind';
            }
            targets.add(target);
        }
        if (Buffer.byteLength(JSON.stringify(payload), 'utf8') > 1048576) {
            return 'property.batch payload exceeds the 1 MiB limit';
        }
    } else if (operation.type === 'property.keyframes') {
        const validKeyframe = keyframe => {
            if (!keyframe || typeof keyframe !== 'object' || Array.isArray(keyframe) ||
                !Number.isSafeInteger(keyframe.frame) ||
                !Object.prototype.hasOwnProperty.call(keyframe, 'value') ||
                !Number.isSafeInteger(keyframe.interpolation) ||
                keyframe.interpolation < 0 ||
                keyframe.interpolation > MAX_KEYFRAME_INTERPOLATION ||
                !['cp1_x', 'cp1_y', 'cp2_x', 'cp2_y'].every(
                    key => finiteNumber(keyframe[key]))) return false;
            const hasTimeValue = Object.prototype.hasOwnProperty.call(keyframe, 'timeValue');
            const hasTimeScale = Object.prototype.hasOwnProperty.call(keyframe, 'timeScale');
            if (hasTimeValue !== hasTimeScale ||
                (hasTimeValue && (!Number.isSafeInteger(keyframe.timeValue) ||
                    !Number.isSafeInteger(keyframe.timeScale) || keyframe.timeScale <= 0))) {
                return false;
            }
            if (Object.prototype.hasOwnProperty.call(keyframe, 'roving') &&
                typeof keyframe.roving !== 'boolean') return false;
            if (Object.prototype.hasOwnProperty.call(keyframe, 'anchor') &&
                (!Number.isSafeInteger(keyframe.anchor) ||
                 keyframe.anchor < 0 || keyframe.anchor > 3)) return false;
            if (Object.prototype.hasOwnProperty.call(keyframe, 'colorLabel') &&
                (!Number.isSafeInteger(keyframe.colorLabel) ||
                 keyframe.colorLabel < 0 || keyframe.colorLabel > 6)) return false;
            return true;
        };
        if (!nonEmptyString(payload.propertyPath) ||
            isAssetPathPropertyPath(payload.propertyPath) ||
            !Array.isArray(payload.expectedKeyframes) ||
            !Array.isArray(payload.keyframes) ||
            payload.expectedKeyframes.length > 100000 ||
            payload.keyframes.length > 100000 ||
            !payload.expectedKeyframes.every(validKeyframe) ||
            !payload.keyframes.every(validKeyframe)) {
            return 'property.keyframes has an invalid propertyPath or keyframe array';
        }
        if (hasExternalPathString(payload.expectedKeyframes) ||
            hasExternalPathString(payload.keyframes)) {
            return 'property.keyframes cannot synchronize local asset paths';
        }
        const hasExpectedAnimatable =
            Object.prototype.hasOwnProperty.call(payload, 'expectedAnimatable');
        const hasAnimatable =
            Object.prototype.hasOwnProperty.call(payload, 'animatable');
        if (hasExpectedAnimatable !== hasAnimatable ||
            (hasExpectedAnimatable &&
             (typeof payload.expectedAnimatable !== 'boolean' ||
              typeof payload.animatable !== 'boolean'))) {
            return 'property.keyframes animatable flags must be paired booleans';
        }
    } else if (operation.type === 'property.expression') {
        if (!nonEmptyString(payload.propertyPath) ||
            isAssetPathPropertyPath(payload.propertyPath) ||
            typeof payload.expectedExpression !== 'string' ||
            typeof payload.expression !== 'string') {
            return 'property.expression requires propertyPath and expected/next expression strings';
        }
    } else if (operation.type === 'layer.components') {
        if (!payload.expected || typeof payload.expected !== 'object' ||
            Array.isArray(payload.expected) || !payload.value ||
            typeof payload.value !== 'object' || Array.isArray(payload.value) ||
            hasExternalPathString(payload.expected) ||
            hasExternalPathString(payload.value) ||
            Buffer.byteLength(JSON.stringify(payload.expected), 'utf8') > 262144 ||
            Buffer.byteLength(JSON.stringify(payload.value), 'utf8') > 262144) {
            return 'layer.components requires bounded expected and next component snapshots';
        }
    } else if (operation.type === 'layer.sourceCrop') {
        const validSnapshot = snapshot => {
            if (!snapshot || typeof snapshot !== 'object' || Array.isArray(snapshot) ||
                Object.keys(snapshot).length !== 7 || typeof snapshot.enabled !== 'boolean' ||
                typeof snapshot.preserveAspect !== 'boolean') return false;
            const inRange = (value, min, max) => typeof value === 'number' &&
                Number.isFinite(value) && value >= min && value <= max;
            const pair = (value, min, max) => Array.isArray(value) && value.length === 2 &&
                value.every(item => inRange(item, min, max));
            return Array.isArray(snapshot.cropRect) && snapshot.cropRect.length === 4 &&
                inRange(snapshot.cropRect[0], -1000000, 1000000) &&
                inRange(snapshot.cropRect[1], -1000000, 1000000) &&
                inRange(snapshot.cropRect[2], 0, 1000000) &&
                inRange(snapshot.cropRect[3], 0, 1000000) &&
                pair(snapshot.pan, -1000000, 1000000) &&
                inRange(snapshot.zoom, 0.001, 1000) &&
                inRange(snapshot.rotation, -360000, 360000) &&
                pair(snapshot.anchor, 0, 1);
        };
        if (!validSnapshot(payload.expected) || !validSnapshot(payload.value) ||
            Buffer.byteLength(JSON.stringify(payload.expected), 'utf8') > 32768 ||
            Buffer.byteLength(JSON.stringify(payload.value), 'utf8') > 32768) {
            return 'layer.sourceCrop requires bounded canonical expected/value snapshots';
        }
    } else if (operation.type === 'layer.shapePolygon') {
        const validSnapshot = snapshot => snapshot && typeof snapshot === 'object' &&
            !Array.isArray(snapshot) && Object.keys(snapshot).length === 2 &&
            typeof snapshot.closed === 'boolean' && Array.isArray(snapshot.points) &&
            snapshot.points.length <= 100000 && snapshot.points.every(point =>
                Array.isArray(point) && point.length === 2 && point.every(value =>
                    typeof value === 'number' && Number.isFinite(value) &&
                    Math.abs(value) <= 1000000));
        if (!validSnapshot(payload.expected) || !validSnapshot(payload.value) ||
            Buffer.byteLength(JSON.stringify(payload.expected), 'utf8') > 262144 ||
            Buffer.byteLength(JSON.stringify(payload.value), 'utf8') > 262144) {
            return 'layer.shapePolygon requires bounded point snapshots';
        }
    } else if (operation.type === 'layer.shapePath') {
        const validSnapshot = snapshot => snapshot && typeof snapshot === 'object' &&
            !Array.isArray(snapshot) && Object.keys(snapshot).length === 2 &&
            snapshot.polygon && typeof snapshot.polygon === 'object' &&
            !Array.isArray(snapshot.polygon) && Object.keys(snapshot.polygon).length === 2 &&
            typeof snapshot.polygon.closed === 'boolean' && Array.isArray(snapshot.polygon.points) &&
            snapshot.path && typeof snapshot.path === 'object' &&
            !Array.isArray(snapshot.path) && Object.keys(snapshot.path).length === 2 &&
            typeof snapshot.path.closed === 'boolean' && Array.isArray(snapshot.path.vertices) &&
            snapshot.polygon.points.length <= 100000 && snapshot.polygon.points.every(point =>
                Array.isArray(point) && point.length === 2 && point.every(value =>
                    typeof value === 'number' && Number.isFinite(value) &&
                    Math.abs(value) <= 1000000)) &&
            snapshot.path.vertices.length <= 100000 && snapshot.path.vertices.every(vertex =>
                Array.isArray(vertex) && vertex.length === 7 &&
                vertex.slice(0, 6).every(value => typeof value === 'number' &&
                    Number.isFinite(value) && Math.abs(value) <= 1000000) &&
                typeof vertex[6] === 'boolean') &&
            (snapshot.polygon.points.length === 0 || snapshot.path.vertices.length === 0);
        if (!validSnapshot(payload.expected) || !validSnapshot(payload.value) ||
            Buffer.byteLength(JSON.stringify(payload.expected), 'utf8') > 262144 ||
            Buffer.byteLength(JSON.stringify(payload.value), 'utf8') > 262144) {
            return 'layer.shapePath requires bounded vertex snapshots';
        }
    } else if (operation.type === 'layer.shapeOperator') {
        if (!Number.isSafeInteger(payload.operatorIndex) || payload.operatorIndex < 0 ||
            payload.operatorIndex > 100000 || typeof payload.field !== 'string' ||
            !/^[A-Za-z][A-Za-z0-9]{0,63}$/.test(payload.field) ||
            Buffer.byteLength(payload.field, 'utf8') > 64 ||
            !finiteNumber(payload.expectedValue) || !finiteNumber(payload.value)) {
            return 'layer.shapeOperator requires a bounded index, field, and finite expected/value numbers';
        }
    } else if (operation.type === 'layer.shapeContents') {
        const validSnapshot = snapshot => snapshot && typeof snapshot === 'object' &&
            !Array.isArray(snapshot) && Object.keys(snapshot).length === 3 &&
            Number.isInteger(snapshot.activeContentIndex) && snapshot.activeContentIndex >= -1 &&
            Array.isArray(snapshot.contents) && snapshot.contents.length <= 256 &&
            snapshot.activeContentIndex < snapshot.contents.length &&
            snapshot.contents.every(content => content && typeof content === 'object' && !Array.isArray(content)) &&
            Array.isArray(snapshot.stackNodes) && snapshot.stackNodes.length <= 1024 &&
            snapshot.stackNodes.every(node => node && typeof node === 'object' && !Array.isArray(node)) &&
            Buffer.byteLength(JSON.stringify(snapshot), 'utf8') <= 262144;
        if (!validSnapshot(payload.expected) || !validSnapshot(payload.value) ||
            hasExternalPathString(payload.expected) || hasExternalPathString(payload.value))
            return 'layer.shapeContents requires bounded content and stack snapshots';
    } else if (operation.type === 'layer.solidSize') {
        const validSize = size => size && typeof size === 'object' && !Array.isArray(size) &&
            Number.isInteger(size.width) && size.width >= 1 && size.width <= 16384 &&
            Number.isInteger(size.height) && size.height >= 1 && size.height <= 16384;
        if (!validSize(payload.expected) || !validSize(payload.value))
            return 'layer.solidSize requires expected/value integer dimensions in [1, 16384]';
    } else if (operation.type === 'layer.deformation2D') {
        if (!payload.expected || typeof payload.expected !== 'object' || Array.isArray(payload.expected) ||
            !payload.value || typeof payload.value !== 'object' || Array.isArray(payload.value) ||
            hasExternalPathString(payload.expected) || hasExternalPathString(payload.value) ||
            Buffer.byteLength(JSON.stringify(payload.expected), 'utf8') > 262144 ||
            Buffer.byteLength(JSON.stringify(payload.value), 'utf8') > 262144) {
            return 'layer.deformation2D requires bounded path-free state snapshots';
        }
    } else if (operation.type === 'layer.audioDeClickRanges') {
        const validRanges = ranges => Array.isArray(ranges) && ranges.length <= 8192 &&
            Buffer.byteLength(JSON.stringify(ranges), 'utf8') <= 262144 && ranges.every((range, index) => {
                if (!Array.isArray(range) || range.length !== 2 ||
                    !range.every(value => typeof value === 'string' && /^(0|[1-9][0-9]*)$/.test(value))) return false;
                try {
                    const start = BigInt(range[0]), end = BigInt(range[1]);
                    const max = 9223372036854775807n;
                    if (start > max || end > max || start >= end) return false;
                    if (index > 0 && start <= BigInt(ranges[index - 1][1])) return false;
                    return true;
                } catch { return false; }
            });
        if (!validRanges(payload.expected) || !validRanges(payload.value))
            return 'layer.audioDeClickRanges requires bounded normalized decimal-string ranges';
    } else if (operation.type === 'layer.stack') {
        if (!['clonerTransforms', 'cloneEffectors', 'textAnimators'].includes(payload.stackKind) ||
            !Array.isArray(payload.expected) || !Array.isArray(payload.value) ||
            hasExternalPathString(payload.expected) ||
            hasExternalPathString(payload.value) ||
            Buffer.byteLength(JSON.stringify(payload.expected), 'utf8') > 262144 ||
            Buffer.byteLength(JSON.stringify(payload.value), 'utf8') > 262144) {
            return 'layer.stack requires a supported stack kind and bounded array snapshots';
        }
    } else if (operation.type === 'layer.animationStack') {
        if (!payload.expected || typeof payload.expected !== 'object' ||
            Array.isArray(payload.expected) || !payload.value ||
            typeof payload.value !== 'object' || Array.isArray(payload.value) ||
            hasExternalPathString(payload.expected) ||
            hasExternalPathString(payload.value) ||
            Buffer.byteLength(JSON.stringify(payload.expected), 'utf8') > 262144 ||
            Buffer.byteLength(JSON.stringify(payload.value), 'utf8') > 262144) {
            return 'layer.animationStack requires bounded path-free expected and next snapshots';
        }
    } else if (operation.type === 'layer.text') {
        if (typeof payload.expected !== 'string' || typeof payload.value !== 'string' ||
            Buffer.byteLength(payload.expected, 'utf8') > 131072 ||
            Buffer.byteLength(payload.value, 'utf8') > 131072) {
            return 'layer.text requires bounded expected and next text values';
        }
    } else if (operation.type === 'layer.rename') {
        if (typeof payload.expected !== 'string' || typeof payload.value !== 'string' ||
            Buffer.byteLength(payload.expected, 'utf8') > 4096 ||
            Buffer.byteLength(payload.value, 'utf8') > 4096) {
            return 'layer.rename requires bounded expected and next names';
        }
    } else if (operation.type === 'layer.variant') {
        if (!Number.isSafeInteger(payload.expected) || payload.expected < 0 || payload.expected > 100000 ||
            !Number.isSafeInteger(payload.value) || payload.value < 0 || payload.value > 100000) {
            return 'layer.variant requires non-negative integer expected and next indices';
        }
    } else if (operation.type === 'layer.blendMode') {
        if (!Number.isSafeInteger(payload.expected) || payload.expected < 0 || payload.expected > 33 ||
            !Number.isSafeInteger(payload.value) || payload.value < 0 || payload.value > 33) {
            return 'layer.blendMode requires valid expected and next mode indices';
        }
    } else if (operation.type === 'layer.opacity') {
        if (!finiteNumber(payload.expected) || payload.expected < 0 || payload.expected > 1 ||
            !finiteNumber(payload.value) || payload.value < 0 || payload.value > 1) {
            return 'layer.opacity requires finite expected and next values in [0, 1]';
        }
    } else if (operation.type === 'layer.parent') {
        const validParentId = value => typeof value === 'string' && value.length <= 64 &&
            /^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/i.test(value);
        if (!validParentId(payload.expectedParentId) || !validParentId(payload.parentId)) {
            return 'layer.parent requires canonical expected and next parent IDs';
        }
        if (payload.parentId.toLowerCase() === operation.layerId.toLowerCase()) {
            return 'layer.parent cannot assign a layer as its own parent';
        }
    } else if (operation.type === 'layer.visibility') {
        if (typeof payload.expected !== 'boolean' || typeof payload.value !== 'boolean') {
            return 'layer.visibility requires boolean expected and next values';
        }
    } else if (operation.type === 'layer.flag') {
        if (!['solo', 'shy'].includes(payload.flag) ||
            typeof payload.expected !== 'boolean' || typeof payload.value !== 'boolean') {
            return 'layer.flag requires a supported flag and boolean expected/next values';
        }
    } else if (operation.type === 'layer.editLock') {
        if (typeof payload.expected !== 'boolean' || typeof payload.value !== 'boolean') {
            return 'layer.editLock requires boolean expected and next values';
        }
    } else if (operation.type === 'layer.transform') {
        for (const key of ['positionX', 'positionY', 'rotationDegrees', 'scaleX', 'scaleY']) {
            if (!finiteNumber(payload[key])) return `layer.transform requires finite ${key}`;
        }
    } else if (operation.type === 'layer.add') {
        if (!nonEmptyString(payload.layerType) ||
            !nonEmptyString(payload.compositionId) ||
            !Number.isSafeInteger(payload.index) || payload.index < 0 || payload.index > 100000 ||
            typeof payload.leftNeighborId !== 'string' ||
            typeof payload.rightNeighborId !== 'string' ||
            typeof payload.anchorLayerId !== 'string' ||
            payload.anchorLayerId !== (payload.leftNeighborId || payload.rightNeighborId) ||
            !payload.layerJson || typeof payload.layerJson !== 'object' ||
            Array.isArray(payload.layerJson) ||
            payload.layerJson.id !== operation.layerId ||
            payload.layerJson.layerType !== payload.layerType ||
            hasExternalPathString(payload.layerJson) ||
            Buffer.byteLength(JSON.stringify(payload), 'utf8') > 786432) {
            return 'layer.add requires a bounded path-free layer snapshot, matching identity, composition, and index';
        }
    } else if (operation.type === 'layer.remove') {
        if (!nonEmptyString(payload.compositionId) ||
            !Number.isSafeInteger(payload.index) || payload.index < 0 || payload.index > 100000 ||
            !payload.expectedLayerJson || typeof payload.expectedLayerJson !== 'object' ||
            Array.isArray(payload.expectedLayerJson) ||
            payload.expectedLayerJson.id !== operation.layerId ||
            hasExternalPathString(payload.expectedLayerJson) ||
            Buffer.byteLength(JSON.stringify(payload), 'utf8') > 786432) {
            return 'layer.remove requires a bounded path-free expected snapshot, composition, and index';
        }
    } else if (operation.type === 'layer.reorder') {
        if (!nonEmptyString(payload.compositionId) ||
            payload.compositionId.length > 256 ||
            !Number.isSafeInteger(payload.expectedIndex) || payload.expectedIndex < 0 || payload.expectedIndex > 100000 ||
            !Number.isSafeInteger(payload.index) || payload.index < 0 || payload.index > 100000) {
            return 'layer.reorder requires a composition and bounded integer indices';
        }
    } else if (operation.type === 'layer.moveAtFrame') {
        if (!Number.isSafeInteger(payload.frame) || Math.abs(payload.frame) > 1000000000 ||
            !Number.isInteger(payload.timeScale) || payload.timeScale < 1 || payload.timeScale > 10000 ||
            !['expectedX', 'expectedY', 'x', 'y'].every(key =>
                finiteNumber(payload[key]) && Math.abs(payload[key]) <= 1000000000)) {
            return 'layer.moveAtFrame requires a bounded frame and finite position values';
        }
    } else if (operation.type === 'review.comment.add') {
        if (!nonEmptyString(payload.commentId) ||
            !nonEmptyString(payload.authorClientId) ||
            !nonEmptyString(payload.compositionId) ||
            !nonEmptyString(payload.text) || payload.text.length > 4096 ||
            payload.authorClientId !== authenticatedClientId ||
            (payload.layerId ?? '') !== (operation.layerId ?? '') ||
            !finiteNumber(payload.createdAtMs) ||
            (Object.prototype.hasOwnProperty.call(payload, 'frame') && !finiteNumber(payload.frame))) {
            return 'review.comment.add has an invalid identity, anchor, timestamp, or text';
        }
    } else if (operation.type === 'review.comment.resolve') {
        if (!nonEmptyString(payload.commentId) ||
            payload.actorClientId !== authenticatedClientId ||
            typeof payload.resolved !== 'boolean' || !finiteNumber(payload.createdAtMs)) {
            return 'review.comment.resolve has an invalid identity or decision';
        }
    } else if (operation.type === 'review.comment.edit') {
        if (!nonEmptyString(payload.commentId) ||
            payload.actorClientId !== authenticatedClientId ||
            !nonEmptyString(payload.text) || payload.text.length > 4096 ||
            (Object.prototype.hasOwnProperty.call(payload, 'actorName') &&
                (typeof payload.actorName !== 'string' || payload.actorName.length > 128)) ||
            !finiteNumber(payload.createdAtMs)) {
            return 'review.comment.edit has an invalid identity or text';
        }
    } else if (operation.type === 'review.comment.remove') {
        if (!nonEmptyString(payload.commentId) ||
            payload.actorClientId !== authenticatedClientId ||
            !finiteNumber(payload.createdAtMs)) {
            return 'review.comment.remove has an invalid identity';
        }
    }
    return null;
}

// Create HTTP server for health checks
const server = http.createServer((req, res) => {
    if (req.url === '/health') {
        res.writeHead(200, { 'Content-Type': 'application/json' });
        res.end(JSON.stringify({ status: 'ok', clients: clients.size }));
    } else {
        res.writeHead(404);
        res.end();
    }
});

const wss = new WebSocket.Server({ server, maxPayload: MAX_WS_MESSAGE_BYTES });

// Session storage: projectId -> { clients, operations, locks, presences }
const sessions = new Map();
const clients = new Map(); // ws -> { projectId, clientId, userId, userName, userColor }

wss.on('connection', (ws, req) => {
    console.log('[Server] New connection');
    ws.isAlive = true;
    ws.on('pong', () => {
        ws.isAlive = true;
        renewLocksForSocket(ws);
    });

    ws.on('message', (data) => {
        try {
            const msg = JSON.parse(data.toString());
            handleClientMessage(ws, msg);
        } catch (err) {
            console.error('[Server] Failed to parse message:', err.message);
            ws.send(JSON.stringify({ type: 'error', message: 'Invalid JSON' }));
        }
    });

    ws.on('close', () => {
        handleClientDisconnect(ws);
    });

    ws.on('error', (err) => {
        console.error('[Server] WebSocket error:', err.message);
    });
});

// A half-open TCP connection otherwise keeps a participant, presence, and its
// locks alive indefinitely. QWebSocket answers protocol-level ping frames
// automatically; terminate peers that fail to answer the previous interval.
const heartbeatInterval = setInterval(() => {
    for (const ws of wss.clients) {
        if (ws.readyState !== WebSocket.OPEN) continue;
        if (ws.isAlive === false) {
            ws.terminate();
            continue;
        }
        ws.isAlive = false;
        ws.ping();
    }
}, 30000);
heartbeatInterval.unref();
wss.on('close', () => clearInterval(heartbeatInterval));

const lockExpiryInterval = setInterval(() => {
    expireCollaborationLocks(Date.now());
}, 15000);
lockExpiryInterval.unref();
wss.on('close', () => clearInterval(lockExpiryInterval));

function handleClientMessage(ws, msg) {
    if (!msg || typeof msg !== 'object' || Array.isArray(msg)) {
        ws.send(JSON.stringify({ type: 'error', message: 'Message must be a JSON object' }));
        return;
    }
    const clientInfo = clients.get(ws);

    switch (msg.type) {
        case 'join':
            handleJoin(ws, msg);
            break;

        case 'operation':
            handleOperation(ws, msg);
            break;

        case 'lock_request':
            handleLockRequest(ws, msg);
            break;

        case 'unlock_request':
            handleUnlockRequest(ws, msg);
            break;

        case 'presence':
            handlePresence(ws, msg);
            break;

        case 'ping':
            ws.send(JSON.stringify({
                type: 'pong',
                timestamp: Date.now(),
                clientTimestamp: Number.isFinite(msg.timestamp) ? msg.timestamp : null
            }));
            break;

        default:
            console.warn('[Server] Unknown message type:', msg.type);
    }
}

function handleJoin(ws, msg) {
    if (msg.accessToken !== undefined && typeof msg.accessToken !== 'string') {
        ws.send(JSON.stringify({ type: 'error', message: 'join accessToken must be a string' }));
        ws.close(1008, 'Invalid join credentials');
        return;
    }
    const accessToken = msg.accessToken || '';
    const role = Buffer.byteLength(accessToken, 'utf8') <= 4096
        ? roleForAccessToken(accessToken)
        : '';
    if (!role) {
        ws.send(JSON.stringify({ type: 'error', message: 'Invalid collaboration access token' }));
        ws.close(1008, 'Invalid join credentials');
        return;
    }

    const projectId = typeof msg.projectId === 'string' ? msg.projectId.trim() : '';
    const clientId = typeof msg.clientId === 'string' ? msg.clientId.trim() : '';
    const projectFingerprint = typeof msg.projectFingerprint === 'string'
        ? msg.projectFingerprint.trim().toLowerCase()
        : '';
    if (typeof projectId !== 'string' || !projectId.trim() ||
        typeof clientId !== 'string' || !clientId.trim() ||
        Buffer.byteLength(projectId, 'utf8') > 256 ||
        Buffer.byteLength(clientId, 'utf8') > 256 ||
        !/^[0-9a-f]{64}$/.test(projectFingerprint)) {
        ws.send(JSON.stringify({ type: 'error', message: 'join requires projectId and clientId of at most 256 bytes and a SHA-256 projectFingerprint' }));
        ws.close(1008, 'Invalid join identity');
        return;
    }

    const userId = msg.userId === undefined ? clientId : msg.userId;
    const userName = msg.userName === undefined ? 'Anonymous' : msg.userName;
    const userColor = msg.userColor === undefined ? '#888888' : msg.userColor;
    if (typeof userId !== 'string' || !userId.trim() ||
        Buffer.byteLength(userId, 'utf8') > 256 ||
        typeof userName !== 'string' || !userName.trim() ||
        Buffer.byteLength(userName, 'utf8') > 128 ||
        typeof userColor !== 'string' || !/^#[0-9a-fA-F]{6}$/.test(userColor)) {
        ws.send(JSON.stringify({ type: 'error', message: 'join identity fields are invalid or exceed their size limit' }));
        ws.close(1008, 'Invalid join identity');
        return;
    }

    const roomMembers = new Set();
    let totalClientsAfterJoin = 0;
    for (const [otherWs, info] of clients) {
        // Reusing this socket or replacing the same identity is one member,
        // not an additional connection for capacity accounting.
        if (otherWs === ws ||
            (info.projectId === projectId && info.clientId === clientId)) {
            continue;
        }
        ++totalClientsAfterJoin;
        if (info.projectId === projectId) roomMembers.add(info.clientId);
    }
    if (totalClientsAfterJoin >= MAX_TOTAL_CLIENTS ||
        roomMembers.size >= MAX_ROOM_CLIENTS) {
        ws.send(JSON.stringify({ type: 'error', message: 'Collaboration server or project is at its connection limit' }));
        ws.close(1013, 'Connection limit reached');
        return;
    }

    let session;
    try {
        session = getOrCreateSession(projectId);
    } catch (err) {
        console.error('[Server] Failed to load collaboration history:', err.message);
        ws.send(JSON.stringify({ type: 'error', message: 'Project collaboration data is unavailable' }));
        return;
    }

    if (!session.projectFingerprint) {
        const historyHasProjectMutations = session.operations.some(
            operation => !String(operation.type || '').startsWith('review.comment.'));
        if (historyHasProjectMutations) {
            ws.send(JSON.stringify({
                type: 'error',
                message: 'Project baseline fingerprint is missing for this room history; create a new room or restore its baseline before joining'
            }));
            ws.close(1008, 'Project baseline is unknown');
            return;
        }
        try {
            persistProjectFingerprint(projectId, projectFingerprint);
            session.projectFingerprint = projectFingerprint;
        } catch (err) {
            console.error('[Server] Failed to persist project fingerprint:', err.message);
            ws.send(JSON.stringify({ type: 'error', message: 'Project baseline could not be stored' }));
            ws.close(1011, 'Project baseline storage failed');
            return;
        }
    } else if (session.projectFingerprint !== projectFingerprint) {
        ws.send(JSON.stringify({
            type: 'error',
            message: 'Project baseline does not match this collaboration room; open the matching project copy before joining'
        }));
        ws.close(1008, 'Project baseline mismatch');
        return;
    }

    // A socket may reconnect or switch projects without first completing its
    // close handshake. Remove its previous membership before assigning a new
    // identity so it cannot remain in an old room's broadcast set.
    if (clients.has(ws)) {
        handleClientDisconnect(ws);
    }

    // Keep one live socket for a client identity in a project. A delayed close
    // from the replaced socket is harmless because its client record is removed
    // before the close event is delivered.
    for (const [existingWs, existingInfo] of clients) {
        if (existingInfo.projectId === projectId && existingInfo.clientId === clientId) {
            handleClientDisconnect(existingWs);
            existingWs.close(4001, 'Client reconnected');
        }
    }

    // Re-resolve after disconnect cleanup: when the replaced socket was the
    // room's last member, cleanup removes the in-memory session from the map.
    session = sessions.get(projectId) || getOrCreateSession(projectId);

    // Register client
    clients.set(ws, { projectId, clientId, userId, userName, userColor, role });

    session.clients.add(ws);

    // Send history to new client
    if (session.operations.length > 0) {
        ws.send(JSON.stringify({
            type: 'history',
            operations: session.operations.map(operation => ({
                type: 'operation',
                clientId: operation.clientId,
                projectId,
                operation,
                version: operation.version
            }))
        }));
    }

    // A joining client needs the current roster; user_joined broadcasts only
    // notify peers that were already in the room.
    for (const existingWs of session.clients) {
        if (existingWs === ws) continue;
        const existing = clients.get(existingWs);
        if (!existing) continue;
        ws.send(JSON.stringify({
            type: 'user_joined',
            clientId: existing.clientId,
            userId: existing.userId,
            userName: existing.userName,
            userColor: existing.userColor
        }));
        const presence = session.presences.get(existing.clientId);
        if (presence) {
            ws.send(JSON.stringify({
                type: 'presence',
                clientId: existing.clientId,
                userId: existing.userId,
                userName: existing.userName,
                userColor: existing.userColor,
                presence
            }));
        }
    }

    // A joining peer needs the current lock roster before making edits.
    for (const [layerId, lock] of session.locks) {
        ws.send(JSON.stringify({
            type: 'lock_updated',
            layerId,
            clientId: lock.clientId,
            userId: lock.userId,
            userName: lock.userName
        }));
    }

    // Notify others
    broadcast(session, {
        type: 'user_joined',
        clientId,
        userId,
        userName,
        userColor
    }, ws);

    // This barrier lets clients gate edits until history, roster, and the
    // initial lock snapshot have all been delivered in WebSocket order.
    ws.send(JSON.stringify({ type: 'room_ready', role }));

    console.log(`[Server] User ${userName} joined project ${projectId}`);
}

function handleOperation(ws, msg) {
    const clientInfo = clients.get(ws);
    const sendOperationError = (message) => {
        const response = { type: 'error', message };
        if (clientInfo?.clientId) response.clientId = clientInfo.clientId;
        const sequence = msg.operation?.opSeq;
        if (Number.isSafeInteger(sequence) && sequence >= 0) {
            response.opSeq = sequence;
        }
        ws.send(JSON.stringify(response));
    };

    if (!clientInfo) return;
    if (clientInfo.role !== 'editor') {
        sendOperationError('Read-only collaboration access cannot submit operations');
        return;
    }

    const session = sessions.get(clientInfo.projectId);
    if (!session) return;
    expireSessionLocks(session, Date.now());
    if (session.storageFailure) {
        sendOperationError('Project history storage is unavailable after a failed rollback; restart and inspect the history before retrying');
        return;
    }

    if (!msg.operation || typeof msg.operation !== 'object' ||
        Array.isArray(msg.operation) || typeof msg.operation.type !== 'string' ||
        !msg.operation.type.trim()) {
        sendOperationError('operation requires an object with a non-empty type');
        return;
    }
    if (Buffer.byteLength(JSON.stringify(msg.operation), 'utf8') > 1024 * 1024) {
        sendOperationError('operation exceeds the 1 MiB limit');
        return;
    }
    const schemaError = validateKnownOperation(msg.operation, clientInfo.clientId);
    if (schemaError) {
        sendOperationError(`Operation rejected: ${schemaError}`);
        return;
    }
    const sequence = msg.operation.opSeq;
    if (!Number.isSafeInteger(sequence) || sequence < 0) {
        sendOperationError('operation requires a non-negative integer opSeq');
        return;
    }

    const operationKey = `${clientInfo.clientId}:${sequence}`;
    const previousOperation = session.operationsByKey.get(operationKey);
    if (previousOperation) {
        if (previousOperation.type !== msg.operation.type ||
            previousOperation.layerId !== msg.operation.layerId ||
            !isDeepStrictEqual(previousOperation.payload, msg.operation.payload)) {
            sendOperationError('opSeq was already used for a different operation');
            return;
        }
        // The original acknowledgement may have been lost when the socket
        // closed. Return the already-committed operation without adding a new
        // history entry or replaying it to the other participants.
        ws.send(JSON.stringify({
            type: 'operation',
            clientId: clientInfo.clientId,
            projectId: clientInfo.projectId,
            operation: previousOperation,
            version: previousOperation.version
        }));
        return;
    }

    if (LOCK_PROTECTED_OPERATION_TYPES.has(msg.operation.type)) {
        const requestedLayerIds = msg.operation.type === 'property.batch'
            ? msg.operation.payload.changes.map(change => change.layerId)
            : msg.operation.type === 'layer.add'
                ? (msg.operation.payload.anchorLayerId ? [msg.operation.payload.anchorLayerId] : [])
                : msg.operation.type === 'layer.parent'
                    ? [msg.operation.layerId,
                        msg.operation.payload.expectedParentId,
                        msg.operation.payload.parentId]
                        .filter(layerId => layerId !== '00000000-0000-0000-0000-000000000000')
                : [msg.operation.layerId];
        for (const requestedLayerId of requestedLayerIds) {
            const lock = session.locks.get(requestedLayerId);
            if (lock && lock.clientId === clientInfo.clientId) continue;
            const owner = lock?.userName || 'another collaborator';
            sendOperationError(lock
                ? `Operation rejected: layer is locked by ${owner}`
                : 'Operation rejected: reserve every affected layer before editing');
            return;
        }
    }

    // Apply operation transformation if needed
    // For now, simple broadcast with version tracking.
    // clientTimestamp preserves the sender's wall clock for audit trails;
    // `timestamp` remains the server receive time.
    const operation = {
        ...msg.operation,
        version: session.operations.length,
        clientId: clientInfo.clientId,
        clientTimestamp: msg.timestamp || msg.operation?.timestamp || null,
        timestamp: new Date().toISOString()
    };

    try {
        persistOperation(clientInfo.projectId, operation, session.operations.length);
    } catch (err) {
        console.error('[Server] Failed to persist operation:', err.message);
        if (err.code === 'COLLAB_HISTORY_ROLLBACK_FAILED') {
            session.storageFailure = true;
        }
        sendOperationError(err.code === 'COLLAB_HISTORY_LIMIT'
            ? 'Project operation history reached its configured limit; operation was not stored'
            : err.code === 'COLLAB_HISTORY_ROLLBACK_FAILED'
                ? 'Project history storage was disabled after a failed rollback; restart and inspect the history'
                : 'Operation was not stored; retry after server storage is available');
        return;
    }

    session.operations.push(operation);
    session.operationsByKey.set(operationKey, operation);

    // Return the server-assigned version to the sender as well. The client
    // session uses this echo to reconcile its pending local operation without
    // applying the edit a second time.
    broadcast(session, {
        type: 'operation',
        clientId: clientInfo.clientId,
        projectId: clientInfo.projectId,
        operation,
        version: operation.version
    });
}

function handleLockRequest(ws, msg) {
    const clientInfo = clients.get(ws);
    if (!clientInfo) return;
    if (clientInfo.role !== 'editor') {
        ws.send(JSON.stringify({ type: 'error', message: 'Read-only collaboration access cannot reserve layers' }));
        return;
    }

    const session = sessions.get(clientInfo.projectId);
    if (!session) return;

    const layerId = msg.layerId;
    if (typeof layerId !== 'string' || !layerId.trim() ||
        Buffer.byteLength(layerId, 'utf8') > 256) {
        ws.send(JSON.stringify({ type: 'error', message: 'lock_request requires a layerId of at most 256 bytes' }));
        return;
    }

    if (!session.locks.has(layerId)) {
        let clientLockCount = 0;
        for (const lock of session.locks.values()) {
            if (lock.clientId === clientInfo.clientId) ++clientLockCount;
        }
        if (clientLockCount >= MAX_LOCKS_PER_CLIENT ||
            session.locks.size >= MAX_LOCKS_PER_ROOM) {
            ws.send(JSON.stringify({
                type: 'error',
                message: clientLockCount >= MAX_LOCKS_PER_CLIENT
                    ? 'Lock request rejected: participant reached the lock limit'
                    : 'Lock request rejected: project reached the lock limit'
            }));
            return;
        }
        // Grant lock
        session.locks.set(layerId, {
            clientId: clientInfo.clientId,
            userId: clientInfo.userId,
            userName: clientInfo.userName,
            acquiredAt: new Date().toISOString(),
            expiresAtMs: Date.now() + LOCK_LEASE_MS
        });

        ws.send(JSON.stringify({
            type: 'lock_granted',
            layerId,
            clientId: clientInfo.clientId
        }));

        // Notify others
        broadcast(session, {
            type: 'lock_updated',
            layerId,
            clientId: clientInfo.clientId,
            userId: clientInfo.userId,
            userName: clientInfo.userName
        }, ws);

        console.log(`[Server] Lock granted: ${layerId} to ${clientInfo.userName}`);
    } else {
        const existingLock = session.locks.get(layerId);
        if (existingLock.clientId === clientInfo.clientId) {
            existingLock.expiresAtMs = Date.now() + LOCK_LEASE_MS;
            ws.send(JSON.stringify({
                type: 'lock_granted',
                layerId,
                clientId: clientInfo.clientId
            }));
            return;
        }
        ws.send(JSON.stringify({
            type: 'lock_denied',
            layerId,
            reason: `Locked by ${existingLock.userName}`
        }));
    }
}

function renewLocksForSocket(ws) {
    const clientInfo = clients.get(ws);
    if (!clientInfo) return;
    const session = sessions.get(clientInfo.projectId);
    if (!session) return;
    expireSessionLocks(session, Date.now());
    const expiresAtMs = Date.now() + LOCK_LEASE_MS;
    for (const lock of session.locks.values()) {
        if (lock.clientId === clientInfo.clientId) {
            lock.expiresAtMs = expiresAtMs;
        }
    }
}

function expireCollaborationLocks(nowMs) {
    for (const session of sessions.values()) {
        expireSessionLocks(session, nowMs);
    }
}

function expireSessionLocks(session, nowMs) {
    for (const [layerId, lock] of session.locks) {
        if (!Number.isFinite(lock.expiresAtMs) || lock.expiresAtMs > nowMs) continue;
        session.locks.delete(layerId);
        broadcast(session, {
            type: 'lock_released',
            layerId,
            clientId: lock.clientId,
            reason: 'Lock lease expired'
        });
        console.log(`[Server] Lock lease expired: ${layerId} from ${lock.userName}`);
    }
}

function handleUnlockRequest(ws, msg) {
    const clientInfo = clients.get(ws);
    if (!clientInfo) return;
    if (clientInfo.role !== 'editor') {
        ws.send(JSON.stringify({ type: 'error', message: 'Read-only collaboration access cannot release layer locks' }));
        return;
    }

    const session = sessions.get(clientInfo.projectId);
    if (!session) return;

    const layerId = msg.layerId;
    if (typeof layerId !== 'string' || !layerId.trim() ||
        Buffer.byteLength(layerId, 'utf8') > 256) {
        ws.send(JSON.stringify({ type: 'error', message: 'unlock_request requires a layerId of at most 256 bytes' }));
        return;
    }
    const lock = session.locks.get(layerId);

    if (lock && lock.clientId === clientInfo.clientId) {
        session.locks.delete(layerId);

        ws.send(JSON.stringify({
            type: 'lock_released',
            layerId
        }));

        // Notify others
        broadcast(session, {
            type: 'lock_released',
            layerId,
            clientId: clientInfo.clientId
        }, ws);

        console.log(`[Server] Lock released: ${layerId} by ${clientInfo.userName}`);
    }
}

function handlePresence(ws, msg) {
    const clientInfo = clients.get(ws);
    if (!clientInfo) return;

    const session = sessions.get(clientInfo.projectId);
    if (!session) return;
    expireSessionLocks(session, Date.now());

    if (!msg.presence || typeof msg.presence !== 'object' ||
        Array.isArray(msg.presence) ||
        Buffer.byteLength(JSON.stringify(msg.presence), 'utf8') > MAX_PRESENCE_BYTES) {
        ws.send(JSON.stringify({ type: 'error', message: 'presence must be an object of at most 64 KiB' }));
        return;
    }

    const presenceMessage = {
        type: 'presence',
        clientId: clientInfo.clientId,
        userId: clientInfo.userId,
        userName: clientInfo.userName,
        userColor: clientInfo.userColor,
        presence: msg.presence
    };
    session.presences.set(clientInfo.clientId, msg.presence);

    // Presence is ephemeral, but retaining the latest value for the lifetime
    // of this in-memory room lets new participants receive a coherent roster.
    broadcast(session, presenceMessage, ws);
}

function handleClientDisconnect(ws) {
    const clientInfo = clients.get(ws);
    if (!clientInfo) return;

    clients.delete(ws);

    const session = sessions.get(clientInfo.projectId);
    if (session) {
        // A replacement socket may already own this client identity.
        const anotherSocket = Array.from(session.clients).some(otherWs => {
            const other = clients.get(otherWs);
            return other && other.clientId === clientInfo.clientId;
        });
        if (!anotherSocket) {
            session.presences.delete(clientInfo.clientId);
        }
        session.clients.delete(ws);

        // Release all locks held by this client
        for (const [layerId, lock] of session.locks) {
            if (lock.clientId === clientInfo.clientId) {
                session.locks.delete(layerId);
                broadcast(session, {
                    type: 'lock_released',
                    layerId,
                    clientId: clientInfo.clientId,
                    reason: 'User disconnected'
                });
            }
        }

        // Notify others
        broadcast(session, {
            type: 'user_left',
            clientId: clientInfo.clientId,
            userId: clientInfo.userId,
            userName: clientInfo.userName
        });

        // Clean up empty sessions
        if (session.clients.size === 0) {
            sessions.delete(clientInfo.projectId);
        }

        console.log(`[Server] User ${clientInfo.userName} left project ${clientInfo.projectId}`);
    }

}

function getOrCreateSession(projectId) {
    let session = sessions.get(projectId);
    if (session) return session;

    const operations = loadOperations(projectId);
    const projectFingerprint = loadProjectFingerprint(projectId);
    const operationsByKey = new Map();
    for (const operation of operations) {
        if (typeof operation.clientId === 'string' &&
            Number.isSafeInteger(operation.opSeq) && operation.opSeq >= 0) {
            operationsByKey.set(`${operation.clientId}:${operation.opSeq}`, operation);
        }
    }
    session = {
        clients: new Set(),
        operations,
        operationsByKey,
        projectFingerprint,
        storageFailure: false,
        locks: new Map(),
        presences: new Map()
    };
    sessions.set(projectId, session);
    return session;
}

function projectFingerprintPath(projectId) {
    const digest = crypto.createHash('sha256').update(projectId, 'utf8').digest('hex');
    return path.join(DATA_DIR, `${digest}.baseline.json`);
}

function loadProjectFingerprint(projectId) {
    const filePath = projectFingerprintPath(projectId);
    if (!fs.existsSync(filePath)) return null;
    const metadata = JSON.parse(fs.readFileSync(filePath, 'utf8'));
    if (!metadata || metadata.schema !== 1 ||
        typeof metadata.projectFingerprint !== 'string' ||
        !/^[0-9a-f]{64}$/.test(metadata.projectFingerprint)) {
        throw new Error('Project baseline metadata is invalid');
    }
    return metadata.projectFingerprint;
}

function persistProjectFingerprint(projectId, projectFingerprint) {
    const filePath = projectFingerprintPath(projectId);
    fs.mkdirSync(path.dirname(filePath), { recursive: true });
    const tempPath = `${filePath}.${process.pid}.${Date.now()}.tmp`;
    let fd = null;
    try {
        fd = fs.openSync(tempPath, 'wx', 0o600);
        fs.writeFileSync(fd, JSON.stringify({ schema: 1, projectFingerprint }) + '\n');
        fs.fsyncSync(fd);
        fs.closeSync(fd);
        fd = null;
        fs.renameSync(tempPath, filePath);
    } catch (err) {
        if (fd !== null) fs.closeSync(fd);
        try { fs.unlinkSync(tempPath); } catch (_) {}
        throw err;
    }
}

function operationLogPath(projectId) {
    // Hash project IDs so untrusted room names cannot escape the configured
    // data directory or create surprising filesystem paths.
    const digest = crypto.createHash('sha256').update(projectId, 'utf8').digest('hex');
    return path.join(DATA_DIR, `${digest}.jsonl`);
}

function loadOperations(projectId) {
    const filePath = operationLogPath(projectId);
    let contents;
    try {
        contents = fs.readFileSync(filePath, 'utf8');
    } catch (err) {
        if (err.code === 'ENOENT') return [];
        throw err;
    }

    const operations = [];
    const lines = contents.split('\n');
    let lastRecordLine = -1;
    for (let i = lines.length - 1; i >= 0; --i) {
        if (lines[i].trim()) {
            lastRecordLine = i;
            break;
        }
    }
    // Normalize a complete final record that lost only its newline before a
    // later append, as well as trimming records torn during a process crash.
    let damagedTail = contents.length > 0 && !contents.endsWith('\n');
    for (let lineIndex = 0; lineIndex < lines.length; ++lineIndex) {
        const line = lines[lineIndex];
        if (!line.trim()) continue;
        let operation;
        try {
            operation = JSON.parse(line);
        } catch (err) {
            if (lineIndex !== lastRecordLine) {
                throw new Error(`Corrupt collaboration history at line ${lineIndex + 1}: ${err.message}`);
            }
            // Only a malformed final append is safe to trim. Interior damage
            // may have valid later operations and must never be rewritten away.
            console.error('[Server] Trimming malformed final collaboration history line:', err.message);
            damagedTail = true;
            break;
        }
        if (!operation || typeof operation !== 'object' ||
            operation.version !== operations.length ||
            typeof operation.type !== 'string' ||
            typeof operation.clientId !== 'string') {
            if (lineIndex !== lastRecordLine) {
                throw new Error(`Corrupt collaboration history at line ${lineIndex + 1}`);
            }
            console.error('[Server] Trimming invalid final collaboration history record');
            damagedTail = true;
            break;
        }
        operations.push(operation);
    }
    if (damagedTail) {
        const repaired = operations.length
            ? `${operations.map(operation => JSON.stringify(operation)).join('\n')}\n`
            : '';
        const repairPath = `${filePath}.repair`;
        fs.writeFileSync(repairPath, repaired, 'utf8');
        fs.renameSync(repairPath, filePath);
    }
    return operations;
}

function persistOperation(projectId, operation, currentOperationCount) {
    fs.mkdirSync(DATA_DIR, { recursive: true });
    const filePath = operationLogPath(projectId);
    const previousSize = fs.existsSync(filePath) ? fs.statSync(filePath).size : 0;
    const line = `${JSON.stringify(operation)}\n`;
    const nextSize = previousSize + Buffer.byteLength(line, 'utf8');
    if (currentOperationCount >= MAX_HISTORY_OPERATIONS || nextSize > MAX_HISTORY_BYTES) {
        const err = new Error('Configured project operation history limit reached');
        err.code = 'COLLAB_HISTORY_LIMIT';
        throw err;
    }
    let fileHandle = null;
    try {
        fileHandle = fs.openSync(filePath, 'a');
        const bytes = Buffer.from(line, 'utf8');
        let offset = 0;
        while (offset < bytes.length) {
            const written = fs.writeSync(fileHandle, bytes, offset, bytes.length - offset, null);
            if (written <= 0) throw new Error('History append made no progress');
            offset += written;
        }
        // Do not acknowledge or broadcast an operation until its file data has
        // been flushed through the operating system's file-sync boundary.
        fs.fsyncSync(fileHandle);
    } catch (err) {
        // Roll back partial writes before allowing later versions to append.
        // If rollback itself fails, the caller marks the room read-only until
        // restart/recovery so a second operation cannot extend a damaged log.
        try {
            if (fileHandle !== null) {
                fs.ftruncateSync(fileHandle, previousSize);
                fs.fsyncSync(fileHandle);
            }
        } catch (rollbackError) {
            const failure = new Error(
                `History append failed and rollback also failed: ${rollbackError.message}`
            );
            failure.code = 'COLLAB_HISTORY_ROLLBACK_FAILED';
            failure.cause = err;
            throw failure;
        }
        throw err;
    } finally {
        if (fileHandle !== null) {
            try {
                fs.closeSync(fileHandle);
            } catch (err) {
                // The preceding fsync is the commit boundary. A close error
                // after it must not turn a committed operation into a retry,
                // which could append the same sequence a second time.
                console.error('[Server] Failed to close synced history file:', err.message);
            }
        }
    }
}

function broadcast(session, msg, excludeWs = null) {
    const data = JSON.stringify(msg);
    for (const client of session.clients) {
        if (client !== excludeWs && client.readyState === WebSocket.OPEN) {
            client.send(data);
        }
    }
}

server.listen(PORT, HOST, () => {
    console.log(`[Server] Collaboration server started on ${HOST}:${PORT}`);
    const healthHost = HOST === '0.0.0.0' || HOST === '::' ? 'localhost' : HOST;
    console.log(`[Server] Health check: http://${healthHost}:${PORT}/health`);
});
