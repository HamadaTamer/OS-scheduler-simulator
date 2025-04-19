#!/usr/bin/env bash
set -euo pipefail

# ———————————————————————————————————————————
# patch_main <PROGRAM_FILE> <COUNT>
#   Rewrite main.c → main_test.c so:
#     - programList has exactly COUNT entries, all pointing to PROGRAM_FILE
#     - scheduler(programList, ...) invoked with COUNT
#———————————————————————————————————————————
patch_main() {
  local file="$1" cnt="$2"
  sed -E \
    -e "s|struct program programList\[.*\]|static struct program programList\[${cnt}\] = { {\"${file}\", 0, 0} };|" \
    -e "s|scheduler\(programList, *[0-9]+\)|scheduler(programList, ${cnt})|" \
    main.c > main_test.c
}

# A little helper to print PASS/FAIL
check() {
  if [[ $1 -eq 0 ]]; then echo "    → PASS"; else echo "    → FAIL"; exit 1; fi
}

echo "=== Test 1: Program_1.txt (printFromTo a→b) ==="
patch_main Program_1.txt 1
gcc -std=c11 -O2 -o scheduler main_test.c

# Program_1.txt does:
#   semWait userInput
#   assign a input  → prompt (0=int) then value
#   assign b input  → prompt (0=int) then value
#   semSignal...
#   semWait userOutput
#   printFromTo a b → should print "2 3 4 5"
#   semSignal userOutput
printf "0\n2\n0\n5\n" | ./scheduler > out1.txt

echo -n "  Got   : "
grep -oP '(?<=printFromTo )[0-9 ]+' out1.txt || true
echo -n "  Expect: 2 3 4 5 — "
grep -Fxq "2 3 4 5" out1.txt
check $?

echo
echo "=== Test 2: Program_2.txt (writeFile a→b) ==="
patch_main Program_2.txt 1
gcc -std=c11 -O2 -o scheduler main_test.c

# Program_2.txt:
#   semWait userInput
#   assign a input   → (0=int) + value
#   assign b input   → (0=int) + value
#   semSignal userInput
#   semWait file
#   writeFile a b    → writes file named "a" containing literal "b"
#   semSignal file
printf "0\n0\n0\n0\n" | ./scheduler >/dev/null

echo -n "  File 'a' contains: "
head -n1 a || true
echo -n "  Expect literal 'b' — "
grep -Fxq "b" a
check $?

echo
echo "=== Test 3: Program_3.txt (readFile a→b→print b) ==="
# Prepare an on‑disk file called "a" for readFile
echo "MAGIC" > a

patch_main Program_3.txt 1
gcc -std=c11 -O2 -o scheduler main_test.c

# Program_3.txt:
#   semWait userInput
#   assign a input   → (1=string) + "a"
#   semSignal userInput
#   semWait file
#   assign b readFile a
#   semSignal file
#   semWait userOutput
#   print b         → should show "MAGIC"
#   semSignal userOutput
printf "1\na\n" | ./scheduler > out3.txt

echo -n "  Got print: "
grep -oP '(?<=print )[A-Za-z]+' out3.txt || true
echo -n "  Expect   : MAGIC — "
grep -Fxq "MAGIC" out3.txt
check $?

echo
echo "🎉 All tests passed!"
