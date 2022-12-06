#!/bin/bash

set -eux

python3 "$(dirname $0)/generate_test_data.py"

: ${top_build_dir:="$(cd .; pwd)"}

source_dir="$(cd $(dirname $0); pwd)"
top_source_dir="${source_dir}/.."
my_quiver_test_dir="${top_source_dir}/mysql-test/my-quiver"

n_processors=$(nproc)

ninja -C "${top_build_dir}"
. "${top_build_dir}/config.sh"

source_mysql_test_dir="${MYSQL_SOURCE_DIR}/mysql-test"
build_mysql_test_dir="${MYSQL_BUILD_DIR}/mysql-test"
source_test_suites_dir="${source_mysql_test_dir}/suite"
source_test_include_dir="${source_mysql_test_dir}/include"
build_test_suites_dir="${build_mysql_test_dir}/suite"
build_test_include_dir="${build_mysql_test_dir}/include"

if [ ! -d "${build_test_suites_dir}" ]; then
  ln -s "${source_test_suites_dir}" "${build_test_suites_dir}"
fi

if [ -d "${MYSQL_BUILD_DIR}/plugin_output_directory" ]; then
  plugins_dir="${MYSQL_BUILD_DIR}/plugin_output_directory"
else
  plugins_dir="${MYSQL_BUILD_DIR}/lib/plugin"
fi

my_quiver_mysql_test_suite_dir="${build_test_suites_dir}/my-quiver"
if [ ! -e "${my_quiver_mysql_test_suite_dir}" ]; then
  ln -s "${my_quiver_test_dir}" "${my_quiver_mysql_test_suite_dir}"
fi

all_test_suite_names=()
suite_dir="${my_quiver_test_dir}/.."
cd "${suite_dir}"
suite_dir="$(pwd)" 
find_conditions=(-type d)
find_conditions+=('(')
find_conditions+=(-name 'include')
find_conditions+=(')')
find_conditions+=(-prune -o)
find_conditions+=(-type d)
find_conditions+=('!' -name '[tr]')
find_conditions+=(-print)
for test_suite_name in $(find my-quiver "${find_conditions[@]}"); do
  all_test_suite_names+=("${test_suite_name}")
done
cd -


mkdir -p "${plugins_dir}"
cp "${top_build_dir}/ha_my_quiver.so" "${plugins_dir}"

 
mysql_test_run_options=()
test_suite_names=()
test_names=()
while [ $# -gt 0 ]; do
  arg="$1"
  shift
  case "$arg" in
    --manual-*|--gdb|--lldb|--client-*|--boot-*|--debug|--valgrind)
      n_processors=1
      mysql_test_run_options+=("${arg}")
      ;;
    --record)
      mysql_test_run_options+=("${arg}" "$1")
      shift
      ;;
    --*)
      mysql_test_run_options+=("${arg}")
      ;;
    *)
      case "$arg" in
	      */t/*.test)
          test_suite_name=$(cd "${arg%/t/*.test}" && pwd)
          test_name="${arg##*/}"
          test_name="${test_name%.test}"
          ;;
        *)
          if [ -d "$arg" ]; then
            test_suite_name=$(cd "$arg" && pwd)
          else
            test_suite_name="$arg"
          fi
          test_name=""
        ;;
      esac

      if [ -n "${test_name}" ]; then
        test_names+=("${test_name}")
      fi

      test_suite_name="${test_suite_name#${suite_dir}/}"
      if echo "${test_suite_names}" | grep --quiet "${test_suite_name}"; then
	      continue
      fi
      test_suite_names+=("${test_suite_name}")
      ;;
  esac
done

if [ ${#test_suite_names[@]} -eq 0 ]; then
  echo "${all_test_suite_names}"
  test_suite_names=("${all_test_suite_names[@]}")
fi

mysql_test_run_args=()
mysql_test_run_args+=("--mem")
mysql_test_run_args+=("--parallel=${n_processors}")
mysql_test_run_args+=("--retry=1")
mysql_test_run_args+=("--suite=$(IFS=","; echo "${test_suite_names[*]}")")
mysql_test_run_args+=("--force")
if [ ${#test_names[@]} -ne 0 ]; then
  mysql_test_run_args+=("--do-test=$(IFS="|"; echo "${test_names[*]}")")
fi
if [ ${#mysql_test_run_options[@]} -ne 0 ]; then
  mysql_test_run_args+=("${mysql_test_run_options[@]}")
fi

(cd "$build_mysql_test_dir" && \
    perl -I . ./mysql-test-run.pl \
      "${mysql_test_run_args[@]}")
