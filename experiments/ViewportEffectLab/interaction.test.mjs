import assert from 'node:assert/strict';
import { test } from 'node:test';
import { createScalarGizmo } from './scalar-gizmo.mjs';

const config = { min: 0, max: 100, step: 1, unitsPerPixel: .25, initial: 10 };
test('drag and numeric input produce the same value', () => {
  const values = [];
  const gizmo = createScalarGizmo({ ...config, preview: value => values.push(value) });
  assert.equal(gizmo.begin(1, 10), true);
  gizmo.move(1, 110);
  gizmo.finish(1, true);
  gizmo.set(35);
  assert.deepEqual(values, [35, 35]);
});

test('cancel restores the baseline and ignores other pointers', () => {
  const gizmo = createScalarGizmo({ ...config, preview() {} });
  gizmo.begin(1, 0);
  gizmo.move(2, 400);
  assert.equal(gizmo.value, 10);
  gizmo.move(1, 400);
  assert.equal(gizmo.value, 100);
  assert.equal(gizmo.finish(2, true), false);
  gizmo.finish(1, false);
  assert.equal(gizmo.value, 10);
});

test('independent instances can bind different parameters', () => {
  const blur = createScalarGizmo({ ...config, preview() {} });
  const opacity = createScalarGizmo({ ...config, initial: 100, preview() {} });
  opacity.begin(1, 100);
  opacity.move(1, 0);
  opacity.finish(1, true);
  assert.equal(opacity.value, 75);
  assert.equal(blur.value, 10);
});
