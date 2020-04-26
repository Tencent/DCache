How to compile:

move example/ to ../../ and then make

DBAccess开发步骤（以后端mysql为例）：
1、创建DB实例和table，table的字段设置和Cache字段一致，并增加一个额外的过期时间字段“sDCacheExpireTime”。
2、开发DBAccess服务，实现DbAccess.tars的接口。可以参考源码中的example。
3、部署DBAccess服务
4、修改Cache模块的Main/DbAccess 配置项，DBFlag=Y, ReadDbFlag=Y, ObjName=你所部署的DbAccess服务的obj信息。