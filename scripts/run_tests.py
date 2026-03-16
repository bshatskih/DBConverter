import subprocess
import xml.etree.ElementTree as ET
import sys
import os

# ====================================================================== #
#  Конфигурация                                                          #
# ====================================================================== #

BUILD_DIR = os.path.join(os.path.dirname(__file__), "..", "build")
TEST_EXE   = os.path.join(BUILD_DIR, "bin", "DBConverter_tests.exe")
XML_OUTPUT = os.path.join(BUILD_DIR, "test_results.xml")

# Цвета для терминала
GREEN  = "\033[92m"
RED    = "\033[91m"
YELLOW = "\033[93m"
CYAN   = "\033[96m"
BOLD   = "\033[1m"
RESET  = "\033[0m"


# ====================================================================== #
#  Запуск тестов                                                         #
# ====================================================================== #

def run_tests():
    if not os.path.exists(TEST_EXE):
        print(f"{RED}Error: test executable not found: {TEST_EXE}{RESET}")
        print("Build the project first.")
        sys.exit(1)

    print(f"{CYAN}{BOLD}Running tests...{RESET}\n")

    result = subprocess.run(
        [TEST_EXE, f"--gtest_output=xml:{XML_OUTPUT}"],
        capture_output=True,
        text=True
    )

    return result.returncode


# ====================================================================== #
#  Парсинг XML                                                           #
# ====================================================================== #

def parse_results():
    if not os.path.exists(XML_OUTPUT):
        print(f"{RED}Error: XML output not found. Tests may have crashed.{RESET}")
        sys.exit(1)

    tree = ET.parse(XML_OUTPUT)
    root = tree.getroot()

    suites = []
    for suite in root.findall("testsuite"):
        suite_name = suite.get("name")
        cases = []
        for case in suite.findall("testcase"):
            name      = case.get("name")
            time      = float(case.get("time", 0))
            failure   = case.find("failure")
            cases.append({
                "name":    name,
                "time":    time,
                "passed":  failure is None,
                "message": failure.get("message", "") if failure is not None else ""
            })
        suites.append({"name": suite_name, "cases": cases})

    return suites


# ====================================================================== #
#  Вывод результатов                                                     #
# ====================================================================== #

def print_results(suites):
    total_passed = 0
    total_failed = 0
    failed_tests = []

    for suite in suites:
        suite_passed = sum(1 for c in suite["cases"] if c["passed"])
        suite_failed = sum(1 for c in suite["cases"] if not c["passed"])
        total_passed += suite_passed
        total_failed += suite_failed

        # Заголовок группы
        status_color = GREEN if suite_failed == 0 else RED
        print(f"{BOLD}{status_color}[ {suite['name']} ]{RESET}  "
              f"{GREEN}{suite_passed} passed{RESET}  "
              f"{RED}{suite_failed} failed{RESET}" if suite_failed > 0 else
              f"{BOLD}{status_color}[ {suite['name']} ]{RESET}  "
              f"{GREEN}{suite_passed} passed{RESET}")

        # Упавшие тесты
        for case in suite["cases"]:
            if not case["passed"]:
                print(f"  {RED}✗ {case['name']}{RESET}")
                # Выводим первую строку сообщения об ошибке
                first_line = case["message"].split("\n")[0] if case["message"] else ""
                if first_line:
                    print(f"    {YELLOW}{first_line}{RESET}")
                failed_tests.append(f"{suite['name']}.{case['name']}")

    # Итог
    total = total_passed + total_failed
    print()
    print("─" * 50)

    if total_failed == 0:
        print(f"{BOLD}{GREEN}✓ All {total_passed} tests passed{RESET}")
    else:
        print(f"{BOLD}{RED}✗ {total_failed} of {total} tests failed{RESET}")
        print()
        print(f"{BOLD}Failed tests:{RESET}")
        for t in failed_tests:
            print(f"  {RED}• {t}{RESET}")

    print("─" * 50)
    return total_failed


# ====================================================================== #
#  Точка входа                                                           #
# ====================================================================== #

if __name__ == "__main__":
    run_tests()
    suites   = parse_results()
    failures = print_results(suites)
    sys.exit(0 if failures == 0 else 1)