# Pattern adoption: tumbleweed

CodeCup 2027 Tumbleweed bot; vendors the bot-kit C++ header. Declared per
`pattern-standards/references/adoption-format.md`.

| Pattern | Status | Evidence |
|---|---|---|
| T13 | partial | tests/conformance.cpp:226 exercises the pure board and sightline logic; protocol IO lives in the same file (tumbleweed.cpp:1) because the CodeCup single-source-file rule forces the merge |
| P46 | partial | BOTKIT_PIN:1 pins the vendored header at vendor/botkit.hpp to a commit; scripts/update-botkit.sh is the refresh-and-repin path |
