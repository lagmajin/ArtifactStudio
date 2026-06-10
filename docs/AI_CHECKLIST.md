# AI Quick Checklist

This repo has a few recurring failure modes. Check these first when editing or debugging:

- Qt types should be included directly in the file that uses them. Do not rely on transitive includes.
- Keep module interfaces and implementations separate. Public `.ixx` files should be self-contained.
- If you add or change `W_OBJECT`, verify the matching `W_OBJECT_IMPL(...)` and `W_SIGNAL(...)` declarations.
- For link errors, suspect one of these first:
  - declaration added, implementation missing
  - implementation exists, but the file is not part of the build
  - `W_OBJECT` / meta-object glue is incomplete
- Common Qt include-miss hotspots:
  - `QApplication`
  - `QRegularExpression`
  - `QPainterPath`
  - `QMetaObject`

