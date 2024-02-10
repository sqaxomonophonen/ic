#!/usr/bin/env bash
wc -l $(git ls-files '*.cpp' '*.c' '*.h' '*.py' | grep -v "^gl3w" | grep -v "^gb_" | grep -v "^imgui/" | grep -v "stb_")
echo -n "Python: "
cat $(git ls-files '*.py') | wc -l
