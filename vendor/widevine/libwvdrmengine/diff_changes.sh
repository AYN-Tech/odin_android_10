#!/bin/bash

set -e

log_dir="${PWD}/diff_change_logs"
rm -rf "${log_dir}"
mkdir "${log_dir}"

EXECUTABLE=""
EXEC_ARGS=""
NUM_CHANGES=-1
while getopts "e:a:n:" opt; do
  case "${opt}" in
    e)
      EXECUTABLE="${OPTARG}"
      ;;
    a)
      EXEC_ARGS="${OPTARG}"
      ;;
    n)
      NUM_CHANGES=${OPTARG}
      ;;
    \?)
      echo "Usage: diff_changes.sh -e<build_and_test_executable> -a\"<executable_args>\" -n<number_of_changes>"
      exit 1;
      ;;
  esac
done

if [ ! "${EXECUTABLE}" ] || [ "${NUM_CHANGES}" -lt 0 ]; then
  echo "EXECUTABLE cannot be empty and NUM_CHANGES must be >= 0"
  exit 1;
else
  EXECUTABLE="./${EXECUTABLE}"
fi

# We search for test cases by searching for lines that start with the RUN header
# and FAILED or OK footers. Note that we capture the test name to make sure the
# header and footer refer to the same test. We also exclude the header from
# showing up again so we only match single tests.
header="\[ RUN      \]"
footer="(\[  FAILED  \]|\[       OK \])"
test_regex="/(${header} ([\S]+)((?!${header})[\s\S])+${footer} \2) \([0-9]+ ms\)/g"

while [ "${NUM_CHANGES}" -ge 0 ]; do
  current_change="HEAD"
  for i in $(seq 1 "${NUM_CHANGES}"); do
    current_change="${current_change}^"
  done
  git checkout "${current_change}" >/dev/null
  sha=$(git rev-parse HEAD)
  run_exec="${EXECUTABLE} ${EXEC_ARGS}"
  run_success=true
  # Restore HEAD regardless if executable ran.
  {
    $run_exec |& tee "${log_dir}/build_and_test_results_${sha}.txt"
  } || {
    run_success=false
  }
  if [ "${current_change}" != "HEAD" ]; then
    git checkout 'HEAD@{1}' >/dev/null
  fi
  if [ "${run_success}" = false ]; then
    exit 1
  fi
  NUM_CHANGES=$((NUM_CHANGES - 1))
done

# Compute diffs of test results oldest first.
file_list=$(find "${log_dir}" -type f -print0 | xargs -0 ls -tr)
file_arr=($file_list)
last_index=$((${#file_arr[@]} - 1));

for current_index in "${!file_arr[@]}"; do
  if [ "${current_index}" -lt "${last_index}" ]; then
    next_index=$((current_index + 1))
    first_sha=$(echo "${file_arr[current_index]}" | \
      sed 's/.*build_and_test_results_\(.\+\).txt$/\1/')
    second_sha=$(echo "${file_arr[next_index]}" | \
      sed 's/.*build_and_test_results_\(.\+\).txt$/\1/')
    output_diff="${log_dir}/test_diff_between_${second_sha}_and_${first_sha}"
    perl_exec="perl -0777 -e                                                \
              'open FILE, \$ARGV[0]; while (<FILE>) { \$file_text .= \$_; } \
               while (\$file_text =~ ${test_regex}) {                       \
                 print \"\$1\n\";                                           \
               }'"
    first_test_results=$(eval "${perl_exec} ${file_arr[current_index]}")
    second_test_results=$(eval "${perl_exec} ${file_arr[next_index]}")
    diff <(echo "${first_test_results}") <(echo "${second_test_results}") \
      &> "${output_diff}"
  fi
done