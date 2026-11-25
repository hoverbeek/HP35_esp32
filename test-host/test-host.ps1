# g++ available on PATH (for example MSYS2 MinGW)
g++ -std=c++17 -Wall -Wextra -Werror -I include src/bcd.cpp test-host/test_bcd.cpp -o bcd-test.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
.\bcd-test.exe
