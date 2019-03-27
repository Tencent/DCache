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
base_dir=$(dirname `pwd`)
src_dir=$base_dir"/src/"$module_name"/"
test_dir=$base_dir"/test/"$module_name"/"

# 利用gcov编译原代码和测试代码
export INCLUDE="-fprofile-arcs -ftest-coverage"
export LIB="-lgcov -fprofile-arcs"
make -C $src_dir clean
make -C $src_dir
make -C $test_dir clean
make -C $test_dir

cd $test_dir
# 会为所有.cpp文件生成.gcno，并将覆盖率信息初始化为0
lcov -b $src_dir -c -i -d $src_dir -o .coverage.wtest.base

# 执行测试用例
for t in $test_dir/*Test ; do $t ; done

# 根据测试结果生成覆盖率
lcov -b $src_dir -c -d $src_dir -o .coverage.wtest.run
# 合并原始覆盖率和测试覆盖率，.coverage.total中包含了.coverage.wtest.base中没有跑到的部分(覆盖率为0)
lcov -a .coverage.wtest.base -a .coverage.wtest.run  -o .coverage.total
# 筛选出原始代码(去掉诸如第三方代码和/usr/include/c++/中的文件等)
lcov -e .coverage.total $src_dir"*" -o .coverage.total.filtered
# 去掉指定文件的覆盖率
while read ignorefile
do
    echo "goindg to exclude $base_dir$ignorefile"
    lcov -r .coverage.total.filtered $base_dir"$ignorefile" -o .coverage.total.filtered
done < ../.lcov-ignore

if [ -d html ] ; then
    rm -rf ./html/*
else
    mkdir html
fi

# 生成覆盖率信息的web页面并打包
genhtml -o ./html/ .coverage.total.filtered
if [ -e "./html.tar" ] ; then
    rm ./html.tar
fi
tar czvf html.tar html
rm .coverage.*
rm $src_dir*.gcda $src_dir*.gcno $test_dir*.gcno $test_dir*.gcda
rm $src_dir*.o $test_dir*.o