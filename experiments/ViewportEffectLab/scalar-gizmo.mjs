export function createScalarGizmo({ min, max, step, unitsPerPixel, initial, preview }) {
  let value = initial;
  let drag;
  function update(next) {
    if (!Number.isFinite(next)) return;
    value = Math.max(min, Math.min(max, min + Math.round((next - min) / step) * step));
    preview(value);
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
      if (drag?.id === id) update(drag.value + (x - drag.x) * unitsPerPixel);
    },
    finish(id, commit) {
      if (!drag || (id != null && id !== drag.id)) return false;
      const baseline = drag.value;
      drag = undefined;
      if (!commit) {
        value = baseline;
        preview(value);
      }
      return true;
    },
  };
}
