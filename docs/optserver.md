
本文档描述了OptServer运行环境配置的过程。

## 1 数据库环境初始化

OptServer运行时需要读写数据库，所以必须先创建数据库。

### 1.1 添加用户
```sql
grant all on *.* to 'dcache'@'%' identified by 'dcache2019' with grant option;
grant all on *.* to 'dcache'@'localhost' identified by 'dcache2019' with grant option;
grant all on *.* to 'dcache'@'${主机名}' identified by 'dcache2019' with grant option;
flush privileges;
```
**注意${主机名}需要修改成自身机器的名称，机器名称可以通过cat /etc/hosts 查看。

### 1.2 创建数据库
执行[src/OptServer/sql目录](../src/OptServer/sql)的脚本exec-sql.sh创建数据库


```
chmod u+x exec-sql.sh

./exec-sql.sh
```

**如果exec-sql.sh脚本执行出错，可能是脚本中的数据库用户名及密码配置不正确，请修改后重试**

脚本执行后，会创建一个数据库: db_dcache_relation, 包括创建库中所有必须的表以及写入初始数据，db_dcache_relation是OptServer运行依赖的核心数据库，包括dcache应用和服务之间的对应关系，服务配置信息等。


## 2 服务配置

OptServer运行需要配置文件，包括前述创建的数据信息等都要填入配置文件，具体配置请参考[服务配置示例](server_config_example.md)

