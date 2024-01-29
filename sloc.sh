#!/usr/bin/env bash
wc -l $(git ls-files '*.cpp' '*.c' '*.h' | grep -v "^gl3w" | grep -v "^gb_" | grep -v "^im" | grep -v "stb_")
