# Taste (Continuously Learned by [CommandCode][cmd])

[cmd]: https://commandcode.ai/

# cppm-modules
- In C++20 module files (.cppm/.ixx), do NOT put `#include` directives in the module purview (after `module X;`). All includes must go in the Global Module Fragment (GMF) between `module;` and `export module X;`. The only exception is TBB includes which should be placed in GMF with all other includes, NOT in the purview. Confidence: 0.80

