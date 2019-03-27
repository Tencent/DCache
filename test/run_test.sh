#!/bin/bash

if [ ! -n "$1" ] ;then
    echo -e "please input a module name. Example : \n\t$0 Router"
    exit 1
fi

module_name=$1
if [ ! -d $module_name ] ;then
    echo "directory "$module_name" doesn't exist"
    exit 1
fi

test_case_num=0
failed_test=""
# 执行测试用例
for t in ./$module_name/*Test ; do
    ((test_case_num=test_case_num+1))
    echo "run test case : $t"
    ./$t
    if [ $? -ne 0 ]; then
        failed_test="$t"
        break;
    fi
done

if [ $test_case_num -eq 0 ]; then
    echo "can't find test in folder $module_name"
    exit 1
fi

if [ -z "failed_test" ]; then
    echo "all test run successed"
    exit 1
else
    echo "$failed_test test failed"
fi