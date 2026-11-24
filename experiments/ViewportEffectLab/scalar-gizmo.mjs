export function createScalarGizmo({ min, max, step, unitsPerPixel, initial, preview }) {
  let value = initial;
  let drag;
  function publish(next) {
    value = next;
    preview(value);
  }
  function update(next) {
    if (!Number.isFinite(next)) return;
    const clamped = Math.max(min, Math.min(max, next));
    publish(Math.max(min, Math.min(max, min + Math.round((clamped - min) / step) * step)));
  }
  return {
    get value() { return value; },
    set(next) { if (!drag) update(next); },
    begin(id, x) {
      if (drag) return false;
      drag = { id, x, value };
      return true;
    },
    move(id, x) {
      if (drag?.id !== id) return;
      const delta = x - drag.x;
      if (delta === 0) {
        publish(drag.value);
      } else {
        update(drag.value + delta * unitsPerPixel);
      }
    },
    finish(id, commit) {
      if (!drag || (id != null && id !== drag.id)) return false;
      const baseline = drag.value;
      drag = undefined;
      if (!commit) {
        publish(baseline);
      }
      return true;
    },
  };
}
