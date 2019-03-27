DCache测试说明
====
DCache单元测试采用[googletest](https://github.com/abseil/googletest)，其具体使用方法请参考[官方文档](https://github.com/abseil/googletest/blob/master/googletest/docs/primer.md)。另外还引入了[googlemock](https://github.com/abseil/googletest/tree/master/googlemock)来辅助单元测试，可自行选用。

## 添加测试用例
测试代码位于此test目录下，目录结构和源码结构保持统一。原则上保证测试代码文件和源代码文件一一对应，即每个cpp文件都有对应的测试文件且统一命名，如源代码文件为：/src/Router/Transfer.cpp，对应的测试代码为/test/Router/TransferTest.cpp。
各自模块应包含此目录下的makefile.inc文件，增加测试代码后，在其模块目录执行make命令进行编译，会生成对应的测试可执行程序，如TransferTest，直接运行它就可以进行测试。
另外也可以通过run_test.sh脚本批量执行测试用例。使用之前需要先编译测试用例代码，然后执行脚本，并指定要执行测试用例的文件夹名称，如
> ./run_test.sh Router

便可批量执行所有Router目录下的测试用例，如遇执行失败会终止并给出失败用例的名称。

## 关于测试文件依赖
测试代码可能会对其他头文件产生依赖，请自行修改makefile中INCLUDE变量，添加头文件搜索路径。
测试代码依赖于源代码生成的.o文件，所以需要先编译源代码后再编译执行测试代码。正因如此，原始代码生成的.o文件中不应当含有main函数的定义(会和测试代码冲突)。建议将main函数单独放在一个独立的文件中，如Main.cpp。

## 代码覆盖率统计
代码覆盖率统计采用gcov和lcov配合完成。run_lcov.sh脚本用于统计模块代码覆盖率。
使用方法：执行run_lcov.sh并指定要统计覆盖率的模块名称，如
> ./run_lcov.sh Router

此命令会重新编译原始代码(src/Router)和测试代码(test/Router)并执行所有测试用例以统计覆盖率，最终在test/Router目录下生成覆盖率统计结果html目录和打包好的html.tar。
另外，由于DCache中有很多由.tars文件自动生成的代码，测试覆盖率时一般要将其去掉，只需将要去掉统计的文件名添加到.lcov-ignore文件中即可。